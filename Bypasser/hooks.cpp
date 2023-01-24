#include "stdafx.hpp"

#include "hooks.hpp"

HMODULE WINAPI hooks::initialize(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	HMODULE result = LoadLibraryExW(lpLibFileName, hFile, dwFlags);

	if (wcsstr(lpLibFileName, L"steamui.dll")) {
		hookImports(lpLibFileName, "kernel32.dll", "LoadLibraryExW", initialize);
	}
	else if (wcsstr(lpLibFileName, L"steamservice.dll")) {
		AllocConsole();
		SetConsoleTitle("VAC-Bypass Debug");
		freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
		freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);

		auto* const h_steamservice = GetModuleHandleW(L"steamservice.dll");

		auto* const dw_process = memory::pattern_scan_ida(h_steamservice, "55 8B EC 81 EC 2C 03 00 00 8D");
		if (!dw_process)
		{
			LOG_ERROR("Failed to find VAC_PROCESS_LOG pattern");
		}
		else
		{
			o_process = reinterpret_cast<process_t>(memory::tramp_hook(reinterpret_cast<std::uintptr_t*>(dw_process), reinterpret_cast<std::uintptr_t>(hk_runprocces_log), 5));
			LOG_DEBUG("Hooked target: VAC_PROCESS_LOG");
		}

		auto* const dw_loading = memory::pattern_scan_ida(h_steamservice, "55 8B EC 83 EC 28 53 56 8B 75");
		if (!dw_loading)
		{
			LOG_ERROR("Failed to find VAC_LOADING pattern");
		}
		else
		{
			o_loading = reinterpret_cast<loading_t>(memory::tramp_hook(reinterpret_cast<std::uintptr_t*>(dw_loading), reinterpret_cast<std::uintptr_t>(hk_loading), 6));
			LOG_DEBUG("Hooked target: VAC_LOADING");
		}

		auto* const dw_calling = memory::pattern_scan_ida(h_steamservice, "55 8B EC 6A FF 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 64 A1 00 00 00 00 50 64 89 25 00 00 00 00 83 EC 6C");
		if (!dw_calling)
		{
			LOG_ERROR("Failed to find VAC_CALLING pattern");
		}
		else
		{
			o_calling = reinterpret_cast<calling_t>(memory::tramp_hook(reinterpret_cast<std::uintptr_t*>(dw_calling), reinterpret_cast<std::uintptr_t>(hk_calling), 5));
			LOG_DEBUG("Hooked target: VAC_CALLING");
		}

		// LibraryEX
		PBYTE VAC_LOAD_LIBRARY_CHECK = PBYTE(memory::findPattern(h_steamservice, "\x74\x47\x6A\x01\x6A", 0));
		if (!VAC_LOAD_LIBRARY_CHECK)
		{
			LOG_ERROR("Failed to find VAC_LOAD_LIBRARY_CHECK pattern");
		}
		else
		{
			DWORD old;
			VirtualProtect(VAC_LOAD_LIBRARY_CHECK, 1, PAGE_EXECUTE_READWRITE, &old);
			*VAC_LOAD_LIBRARY_CHECK = 0xEB;
			VirtualProtect(VAC_LOAD_LIBRARY_CHECK, 1, old, &old);
			hookImports(L"steamservice", "kernel32.dll", "LoadLibraryExW", hk_LoadLibraryExW);
			LOG_DEBUG("Hooked target: LoadLibraryExW");
		}

		LOG_SUCCESS("Initialization was successful!");
	}

	return result;
}

bool __stdcall hooks::hk_load_vac(valve::c_module* Src, char mode)
{
	auto is_valid_module = [&](const valve::c_module* module_struct) {
		const auto module_data = module_struct->m_raw;
		const auto module_size = module_struct->m_module_size;

		if (module_size < 0x200
			|| *reinterpret_cast<WORD*>(module_data) != 23117
			|| *reinterpret_cast<DWORD*>(module_data + 60) < 0x40
			|| *reinterpret_cast<DWORD*>(module_data + 60) >= module_size - 248
			|| *reinterpret_cast<DWORD*>((*reinterpret_cast<DWORD*>(module_data + 60)) + module_data) != 17744) {
			return false;
		}

		if (*reinterpret_cast<DWORD*>(module_data + 64) != 5655638
			|| *reinterpret_cast<DWORD*>(module_data + 68) != 1
			|| module_size < *reinterpret_cast<DWORD*>(module_data + 72)) {
			return false;
		}

		const auto v6 = (*reinterpret_cast<DWORD*>(module_data + 60)) + module_data + 24;

		if (*reinterpret_cast<WORD*>(v6) != 267
			&& *reinterpret_cast<WORD*>(v6) != 523) {
			return false;
		}

		return true;
	};

	if (!is_valid_module(Src)) {
		Src->m_last_result = valve::SUCCESS;
		return 1;
	}

	const auto nt_header_crc32 = memory::hash_header(reinterpret_cast<void*>(Src->m_raw));
	
	// unload vac module
	const auto raw = Src->m_raw;
	FreeLibrary(Src->m_module);
	Src->m_runfunc = nullptr;
	Src->m_raw = -1;
	Src->m_module_size = -1;
	Src->m_last_result = valve::SUCCESS;

	std::cout << "[";
	const auto hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	const auto prefix = Logger::GetLevelPrefix(Logger::Level::Success);
	SetConsoleTextAttribute(hConsole, prefix.color);
	std::cout << prefix.text;
	SetConsoleTextAttribute(hConsole, 15);
	std::cout << "] VAC module [ 0x";
	std::cout << std::setiosflags(std::ios::uppercase) << std::hex << nt_header_crc32;
	std::cout << " ] was successfully unloaded!" << std::endl;

	//LOG_SUCCESS("VAC module was successfully unloaded");

	return 1;
}

void __fastcall hooks::hk_runprocces_log(void* ecx, void* edx, const char* a2, const char* a3, int a4) {
	// @xref: "\\logs\\runprocess_log.txt"		
	// @note: this function collects all data about the launch of any game by the client.
	// return nothing.
	LOG_SUCCESS("Processed VAC Logs");
	return;
}

bool __stdcall hooks::hk_loading(valve::vac_buffer* h_mod, char injection_flags)
{
	auto is_valid_module = [&](const valve::vac_buffer* module_struct) {
		const auto module_data = reinterpret_cast<int>(module_struct->m_pRawModule);
		const auto module_size = module_struct->m_nModuleSize;

		if (module_size < 0x200
			|| *reinterpret_cast<WORD*>(module_data) != 23117
			|| *reinterpret_cast<DWORD*>(module_data + 60) < 0x40
			|| *reinterpret_cast<DWORD*>(module_data + 60) >= module_size - 248
			|| *reinterpret_cast<DWORD*>((*reinterpret_cast<DWORD*>(module_data + 60)) + module_data) != 17744) {
			return false;
		}

		if (*reinterpret_cast<DWORD*>(module_data + 64) != 5655638
			|| *reinterpret_cast<DWORD*>(module_data + 68) != 1
			|| module_size < *reinterpret_cast<DWORD*>(module_data + 72)) {
			return false;
		}

		const auto v6 = (*reinterpret_cast<DWORD*>(module_data + 60)) + module_data + 24;

		if (*reinterpret_cast<WORD*>(v6) != 267
			&& *reinterpret_cast<WORD*>(v6) != 523) {
			return false;
		}

		return true;
	};

	if (!is_valid_module(h_mod)) {
		h_mod->m_nLastResult = valve::SUCCESS;
		return 1;
	}

	const auto nt_header_crc32 = memory::hash_header(h_mod->m_pRawModule);
	const auto b_ret = o_loading(h_mod, injection_flags);

	if (nt_header_crc32)
	{
		//valve::msg("[ Sakura ] VAC trying to check module crc32 [ 0x%.8X ].\n", nt_header_crc32);

		if (nt_header_crc32 == 0xCC29049A || nt_header_crc32 == 0x2B8DD987)
			valve::uid_whitelist.push_back(h_mod->m_unCRC32);
		else
		{
			std::cout << "[";
			const auto hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
			const auto prefix = Logger::GetLevelPrefix(Logger::Level::Success);
			SetConsoleTextAttribute(hConsole, prefix.color);
			std::cout << prefix.text;
			SetConsoleTextAttribute(hConsole, 15);
			std::cout << "] " << "VAC trying to check module crc32 [ 0x";
			std::cout << std::setiosflags(std::ios::uppercase) << std::hex << nt_header_crc32;
			std::cout << " ], Processed!" << std::endl;
		}
	}

	if (h_mod->m_unCRC32 && std::find(valve::uid_whitelist.begin(), valve::uid_whitelist.end(), h_mod->m_unCRC32) != valve::uid_whitelist.end())
		return b_ret;

	if (h_mod->m_pRunFunc)
		h_mod->m_pRunFunc = nullptr;

	return b_ret;
}

int __fastcall hooks::hk_calling(void* ecx, void* edx, std::uint32_t crc_hash, char injection_mode, int unused1, int id, int param1, int unused2, int param2, int param3, int* param4, int* size_check)
{
	auto status = o_calling(ecx, edx, crc_hash, injection_mode, unused1, id, param1, unused2, param2, param3, param4, size_check);
	static const auto condition = static_cast<void*>(memory::pattern_scan_ida(L"steamservice", "0F 84 ? ? ? ? B9 ? ? ? ? 8B 06"));

	// patch crc hash checks.
	if (_ReturnAddress() == condition)
		return 1;

	if (status != valve::SUCCESS && status != valve::OTHER_SUCCESS)
	{
		status = valve::SUCCESS;
		LOG_SUCCESS("Patched status: " + std::to_string(status));
	}

	return status;
}

HMODULE WINAPI hooks::hk_LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	HMODULE result = LoadLibraryExW(lpLibFileName, hFile, dwFlags);

	LOG_DEBUG("VAC is trying to work!");
	//valve::msg("[ Sakura ] VAC work detected!\n");
	//MessageBoxA(NULL, "VAC work detected!", "Sakura", MB_ICONEXCLAMATION);

	hookImports(lpLibFileName, "kernel32.dll", "GetProcAddress", hk_GetProcAddress);
	hookImports(lpLibFileName, "kernel32.dll", "GetSystemInfo", hk_GetSystemInfo);

	return result;
}

FARPROC WINAPI hooks::hk_GetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
	FARPROC result = GetProcAddress(hModule, lpProcName);

	if (result) {
		if (!strcmp(lpProcName, "GetProcAddress"))
			return (FARPROC)hk_GetProcAddress;
		else if (!strcmp(lpProcName, "GetSystemInfo"))
			return (FARPROC)hk_GetSystemInfo;
		else if (!strcmp(lpProcName, "GetVersionExA"))
			return (FARPROC)hk_GetVersionExA;
		else if (!strcmp(lpProcName, "GetSystemDirectoryW"))
			return (FARPROC)hk_GetSystemDirectoryW;
		else if (!strcmp(lpProcName, "GetWindowsDirectoryW"))
			return (FARPROC)hk_GetWindowsDirectoryW;
		else if (!strcmp(lpProcName, "GetCurrentProcessId"))
			return (FARPROC)hk_GetCurrentProcessId;
		else if (!strcmp(lpProcName, "GetCurrentThreadId"))
			return (FARPROC)hk_GetCurrentThreadId;
	}
	return result;
}

VOID WINAPI hooks::hk_GetSystemInfo(LPSYSTEM_INFO lpSystemInfo)
{
	LOG_SUCCESS("Spoof VAC System Check.");
	GetSystemInfo(lpSystemInfo);
	lpSystemInfo->dwPageSize = 1337;
}

BOOL WINAPI hooks::hk_GetVersionExA(LPOSVERSIONINFOEXA lpVersionInformation)
{
	LOG_ERROR("Bypass malfunction detected! Steam will close...");
	MessageBoxW(NULL, L"Bypass malfunction detected! Steam will close...", L"VAC Bypass", MB_OK | MB_ICONERROR);
	ExitProcess(EXIT_FAILURE);
	return FALSE;
}

UINT WINAPI hooks::hk_GetSystemDirectoryW(LPWSTR lpBuffer, UINT uSize)
{
	LOG_ERROR("Bypass malfunction detected! Steam will close...");
	MessageBoxW(NULL, L"Bypass malfunction detected! Steam will close...", L"VAC Bypass", MB_OK | MB_ICONERROR);
	ExitProcess(EXIT_FAILURE);
	return 0;
}

UINT WINAPI hooks::hk_GetWindowsDirectoryW(LPWSTR lpBuffer, UINT uSize)
{
	LOG_ERROR("Bypass malfunction detected! Steam will close...");
	MessageBoxW(NULL, L"Bypass malfunction detected! Steam will close...", L"VAC Bypass", MB_OK | MB_ICONERROR);
	ExitProcess(EXIT_FAILURE);
	return 0;
}

DWORD WINAPI hooks::hk_GetCurrentProcessId()
{
	LOG_SUCCESS("Spoof VAC Process Check.");
	return 0;
}

DWORD WINAPI hooks::hk_GetCurrentThreadId()
{
	LOG_SUCCESS("Spoof VAC Thread Check.");
	return 0;
}

VOID hooks::hookImports(PCWSTR moduleName, PCSTR importModuleName, PCSTR functionName, PVOID fun)
{
	PBYTE module = (PBYTE)GetModuleHandleW(moduleName);

	if (module) {
		PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)(module + ((PIMAGE_DOS_HEADER)module)->e_lfanew);
		PIMAGE_IMPORT_DESCRIPTOR imports = (PIMAGE_IMPORT_DESCRIPTOR)(module + ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
		for (PIMAGE_IMPORT_DESCRIPTOR import = imports; import->Name; import++) {
			if (_strcmpi(PCHAR(module) + import->Name, importModuleName))
				continue;

			for (PIMAGE_THUNK_DATA original_first_thunk = (PIMAGE_THUNK_DATA)(module + import->OriginalFirstThunk), first_thunk = (PIMAGE_THUNK_DATA)(module + import->FirstThunk); original_first_thunk->u1.AddressOfData; original_first_thunk++, first_thunk++) {
				if (strcmp((PCSTR)((PIMAGE_IMPORT_BY_NAME)(module + original_first_thunk->u1.AddressOfData))->Name, functionName))
					continue;

				PDWORD functionAddress = &first_thunk->u1.Function;

				DWORD old;
				if (VirtualProtect(functionAddress, sizeof(fun), PAGE_READWRITE, &old)) {
					*functionAddress = (DWORD)fun;
					VirtualProtect(functionAddress, sizeof(fun), old, &old);
				}
				break;
			}
			break;
		}
	}
}
