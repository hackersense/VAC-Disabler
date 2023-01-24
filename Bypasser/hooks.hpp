#pragma once

namespace hooks
{
	HMODULE WINAPI initialize(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);

	bool __stdcall hk_load_vac(valve::c_module* Src, char mode);

	using process_t = void(__fastcall*)(void* ecx, void* edx, const char* a2, const char* a3, int a4);
	inline process_t o_process;
	void __fastcall hk_runprocces_log(void* ecx, void* edx, const char* a2, const char* a3, int a4);

	using loading_t = bool(__stdcall*)(valve::vac_buffer*, char);
	inline loading_t o_loading;
	bool __stdcall hk_loading(valve::vac_buffer*, char);

	using calling_t = int(__fastcall*)(void*, void*, std::uint32_t, char, int, int, int, int, int, int, int*, int*);
	inline calling_t o_calling;
	int __fastcall hk_calling(void*, void*, std::uint32_t, char, int, int, int, int, int, int, int*, int*);

	HMODULE WINAPI hk_LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
	FARPROC WINAPI hk_GetProcAddress(HMODULE hModule, LPCSTR lpProcName);
	VOID WINAPI hk_GetSystemInfo(LPSYSTEM_INFO lpSystemInfo);
	BOOL WINAPI hk_GetVersionExA(LPOSVERSIONINFOEXA lpVersionInformation);
	UINT WINAPI hk_GetSystemDirectoryW(LPWSTR lpBuffer, UINT uSize);
	UINT WINAPI hk_GetWindowsDirectoryW(LPWSTR lpBuffer, UINT uSize);
	DWORD WINAPI hk_GetCurrentProcessId(VOID);
	DWORD WINAPI hk_GetCurrentThreadId(VOID);
	VOID hookImports(PCWSTR moduleName, PCSTR importModuleName, PCSTR functionName, PVOID fun);
}
