#include "stdafx.hpp"
#include "hooks.hpp"

extern "C" BOOL WINAPI _CRT_INIT(HMODULE moduleHandle, DWORD reason, LPVOID reserved);

void MainThread(HMODULE hModule) {
	//while (!GetModuleHandleA("tier0_s.dll") || !GetModuleHandleA("steamservice.dll"))
	//	std::this_thread::sleep_for(std::chrono::seconds(2)); 
	
	hooks::hookImports(NULL, "kernel32.dll", "LoadLibraryExW", hooks::initialize);
	//hooks::run();

	_CRT_INIT(hModule, DLL_PROCESS_DETACH, nullptr);
	FreeLibraryAndExitThread(hModule, 0);

	return ExitThread(EXIT_SUCCESS);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ulReasonForCall, LPVOID lpReserved)
{
	if (ulReasonForCall == DLL_PROCESS_ATTACH)
	{
		const auto thread = CreateThread(0, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(MainThread), hModule, 0, 0);
		if (thread)
			CloseHandle(thread);
	}

	return TRUE;
}

