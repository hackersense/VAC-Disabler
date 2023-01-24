#pragma once

namespace valve
{
	inline void msg(const char* message, ...)
	{
		static auto* h_tier0 = GetModuleHandleA("tier0_s.dll");
		if (!h_tier0)
			return;

		using fn = void(__cdecl*)(const char* msg, va_list);
		static auto _msg = reinterpret_cast<fn>(GetProcAddress(h_tier0, "Msg"));
		if (!_msg)
			return;

		char buf[1024];

		va_list list;
		va_start(list, message);
		vsprintf_s(buf, message, list);
		va_end(list);

		return _msg(buf, list);
	}

	enum vac_result_t
	{
		SUCCESS = 1,
		OTHER_SUCCESS = 2,
		ENTRY_POINT_FAIL = 16,
		MODULE_LOAD_FAILED = 22,
	};

	struct mapped_module
	{
		std::byte pad04[4];
		void* m_pModuleBase;
		PIMAGE_NT_HEADERS m_pNTHeaders;
	};

	class c_module {
		typedef int(__stdcall* runfunc_t)(int, unsigned char*, int, unsigned char*, int*);
		char gap0[4];
	public:
		HMODULE m_module{ };
		HMODULE m_sec_module{ };
		runfunc_t m_runfunc{ };
		int m_last_result{ };
		std::size_t m_module_size{ };
		int m_raw{ };
	private:
		char gap1C[16];
	};

	struct vac_buffer
	{
		std::uint32_t m_unCRC32;
		std::byte pad04[4];
		mapped_module* m_pMappedModule;
		void* m_pRunFunc; // entrypoint
		int m_nLastResult;
		std::size_t m_nModuleSize;
		void* m_pRawModule;
	};

	inline std::vector<std::uintptr_t> uid_whitelist = {};
}
