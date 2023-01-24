#include "stdafx.hpp"
#include "vacbypass.hpp"

typedef HMODULE(__stdcall* pLoadLibraryA)(LPCSTR);
typedef FARPROC(__stdcall* pGetProcAddress)(HMODULE, LPCSTR);

typedef INT(__stdcall* dllmain)(HMODULE, DWORD, LPVOID);

struct LoaderData
{
	LPVOID ImageBase;

	PIMAGE_NT_HEADERS NtHeaders;
	PIMAGE_BASE_RELOCATION BaseReloc;
	PIMAGE_IMPORT_DESCRIPTOR ImportDirectory;

	pLoadLibraryA fnLoadLibraryA;
	pGetProcAddress fnGetProcAddress;
};

DWORD FindProcessId(std::string processName)
{
	PROCESSENTRY32 processInfo;
	processInfo.dwSize = sizeof(processInfo);

	HANDLE processSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (processSnapshot == INVALID_HANDLE_VALUE)
		return 0;

	Process32First(processSnapshot, &processInfo);
	if (!processName.compare(processInfo.szExeFile))
	{
		CloseHandle(processSnapshot);
		return processInfo.th32ProcessID;
	}

	while (Process32Next(processSnapshot, &processInfo))
	{
		if (!processName.compare(processInfo.szExeFile))
		{
			CloseHandle(processSnapshot);
			return processInfo.th32ProcessID;
		}
	}

	CloseHandle(processSnapshot);
	return 0;
}

DWORD __stdcall LibraryLoader(LPVOID Memory)
{
	LoaderData* LoaderParams = (LoaderData*)Memory;

	PIMAGE_BASE_RELOCATION pIBR = LoaderParams->BaseReloc;

	DWORD delta = (DWORD)((LPBYTE)LoaderParams->ImageBase - LoaderParams->NtHeaders->OptionalHeader.ImageBase); // Calculate the delta

	while (pIBR->VirtualAddress)
	{
		if (pIBR->SizeOfBlock >= sizeof(IMAGE_BASE_RELOCATION))
		{
			int count = (pIBR->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
			PWORD list = (PWORD)(pIBR + 1);

			for (int i = 0; i < count; i++)
			{
				if (list[i])
				{
					PDWORD ptr = (PDWORD)((LPBYTE)LoaderParams->ImageBase + (pIBR->VirtualAddress + (list[i] & 0xFFF)));
					*ptr += delta;
				}
			}
		}

		pIBR = (PIMAGE_BASE_RELOCATION)((LPBYTE)pIBR + pIBR->SizeOfBlock);
	}

	PIMAGE_IMPORT_DESCRIPTOR pIID = LoaderParams->ImportDirectory;

	// Resolve DLL imports
	while (pIID->Characteristics)
	{
		PIMAGE_THUNK_DATA OrigFirstThunk = (PIMAGE_THUNK_DATA)((LPBYTE)LoaderParams->ImageBase + pIID->OriginalFirstThunk);
		PIMAGE_THUNK_DATA FirstThunk = (PIMAGE_THUNK_DATA)((LPBYTE)LoaderParams->ImageBase + pIID->FirstThunk);

		HMODULE hModule = LoaderParams->fnLoadLibraryA((LPCSTR)LoaderParams->ImageBase + pIID->Name);

		if (!hModule)
			return FALSE;

		while (OrigFirstThunk->u1.AddressOfData)
		{
			if (OrigFirstThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)
			{
				// Import by ordinal
				DWORD Function = (DWORD)LoaderParams->fnGetProcAddress(hModule,
					(LPCSTR)(OrigFirstThunk->u1.Ordinal & 0xFFFF));

				if (!Function)
					return FALSE;

				FirstThunk->u1.Function = Function;
			}
			else
			{
				// Import by name
				PIMAGE_IMPORT_BY_NAME pIBN = (PIMAGE_IMPORT_BY_NAME)((LPBYTE)LoaderParams->ImageBase + OrigFirstThunk->u1.AddressOfData);
				DWORD Function = (DWORD)LoaderParams->fnGetProcAddress(hModule, (LPCSTR)pIBN->Name);
				if (!Function)
					return FALSE;

				FirstThunk->u1.Function = Function;
			}
			OrigFirstThunk++;
			FirstThunk++;
		}
		pIID++;
	}

	if (LoaderParams->NtHeaders->OptionalHeader.AddressOfEntryPoint)
	{
		dllmain EntryPoint = (dllmain)((LPBYTE)LoaderParams->ImageBase + LoaderParams->NtHeaders->OptionalHeader.AddressOfEntryPoint);

		return EntryPoint((HMODULE)LoaderParams->ImageBase, DLL_PROCESS_ATTACH, NULL); // Call the entry point
	}
	return TRUE;
}

void inject(unsigned char* dllMemory, DWORD ProcessId)
{
	LoaderData LoaderParams;

	LPBYTE FileBuffer = LPBYTE(dllMemory);

	// Target Dll's DOS Header
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)FileBuffer;
	// Target Dll's NT Headers
	PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)(FileBuffer + pDosHeader->e_lfanew);

	// Opening target process.
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, ProcessId);
	// Allocating memory for the DLL
	PVOID ExecutableImage = VirtualAllocEx(hProcess, NULL, pNtHeaders->OptionalHeader.SizeOfImage,
		MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

	// Copy the headers to target process
	WriteProcessMemory(hProcess, ExecutableImage, FileBuffer,
		pNtHeaders->OptionalHeader.SizeOfHeaders, NULL);

	// Target Dll's Section Header
	PIMAGE_SECTION_HEADER pSectHeader = (PIMAGE_SECTION_HEADER)(pNtHeaders + 1);
	// Copying sections of the dll to the target process
	for (int i = 0; i < pNtHeaders->FileHeader.NumberOfSections; i++)
	{
		WriteProcessMemory(hProcess, (PVOID)((LPBYTE)ExecutableImage + pSectHeader[i].VirtualAddress),
			(PVOID)(FileBuffer + pSectHeader[i].PointerToRawData), pSectHeader[i].SizeOfRawData, NULL);
	}

	// Allocating memory for the loader code.
	PVOID LoaderMemory = VirtualAllocEx(hProcess, NULL, 4096, MEM_COMMIT | MEM_RESERVE,
		PAGE_EXECUTE_READWRITE); // Allocate memory for the loader code

	LoaderParams.ImageBase = ExecutableImage;
	LoaderParams.NtHeaders = (PIMAGE_NT_HEADERS)((LPBYTE)ExecutableImage + pDosHeader->e_lfanew);

	LoaderParams.BaseReloc = (PIMAGE_BASE_RELOCATION)((LPBYTE)ExecutableImage
		+ pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
	LoaderParams.ImportDirectory = (PIMAGE_IMPORT_DESCRIPTOR)((LPBYTE)ExecutableImage
		+ pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

	LoaderParams.fnLoadLibraryA = LoadLibraryA;
	LoaderParams.fnGetProcAddress = GetProcAddress;

	// Write the loader information to target process
	WriteProcessMemory(hProcess, LoaderMemory, &LoaderParams, sizeof(LoaderData),
		NULL);
	// Write the loader code to target process
	WriteProcessMemory(hProcess, (PVOID)((LoaderData*)LoaderMemory + 1), LibraryLoader,
		(DWORD)LibraryLoader, NULL);
	// Create a remote thread to execute the loader code
	HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)((LoaderData*)LoaderMemory + 1),
		LoaderMemory, 0, NULL);

	// Wait for the loader to finish executing
	WaitForSingleObject(hThread, INFINITE);

	//std::cin.get();
	// free the allocated loader code
	VirtualFreeEx(hProcess, LoaderMemory, 0, MEM_RELEASE);
}

bool inject_local(const DWORD process_id, const char* dll) {
	if (process_id == NULL)
		return false;

	char custom_dll[MAX_PATH];
	GetFullPathNameA(dll, MAX_PATH, custom_dll, nullptr);

	const HANDLE h_process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, process_id);
	const LPVOID allocated_mem = VirtualAllocEx(h_process, nullptr, sizeof custom_dll, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	if (WriteProcessMemory(h_process, allocated_mem, custom_dll, sizeof custom_dll, nullptr)) {
		CreateRemoteThread(h_process, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(LoadLibrary),
			allocated_mem, 0, nullptr);

		if (h_process)
			CloseHandle(h_process);

		return TRUE;
	}
	return FALSE;
}


int main(int argc, char** argv)
{
	std::atexit([]()
		{
			std::cin.get();
		});

	AllocConsole();

	HKEY h_key = nullptr;
	if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_QUERY_VALUE, &h_key) != ERROR_SUCCESS)
	{
		RegCloseKey(h_key);

		std::cout << "failed to find steam registry" << std::endl;
		return EXIT_FAILURE;
	}

	char steam_path_reg[MAX_PATH];
	steam_path_reg[0] = '"';
	DWORD steam_path_size = sizeof(steam_path_reg) - sizeof(char);

	if (RegQueryValueExA(h_key, "SteamExe", nullptr, nullptr, (LPBYTE)(steam_path_reg + 1), &steam_path_size) != ERROR_SUCCESS)
	{
		RegCloseKey(h_key);

		std::cout << "failed to get steam path" << std::endl;
		return EXIT_FAILURE;
	}
	RegCloseKey(h_key);

	auto steam_path = std::string(steam_path_reg) + "\"";

	do
	{
		memory::kill_process("steam.exe");
		memory::kill_process("steamservice.exe");
		memory::kill_process("steamwebhelper.exe");

		if (!memory::is_process_open("steam.exe") && !memory::is_process_open("steamservice.exe") && !memory::is_process_open("steamwebhelper.exe"))
			break;

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	} while (true);

	PROCESS_INFORMATION pi;
	if (!memory::open_process(steam_path, { "-console" }, pi))
	{
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

		std::cout << "failed to open steam.exe" << std::endl;
		return EXIT_FAILURE;
	}

	if (memory::is_process_open("steamservice.exe"))
	{
		std::cout << "steamservice is running, steam did not run as admin" << std::endl;
		return EXIT_FAILURE;
	}

	inject(vac_bypass, pi.dwProcessId);
	//inject_local(pi.dwProcessId, "Bypasser.dll");

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	std::cout << "Success" << std::endl;

	FreeConsole();

	return EXIT_SUCCESS;
}
