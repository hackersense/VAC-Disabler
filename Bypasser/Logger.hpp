#pragma once

#include <Windows.h>
#include <iostream>
#include <string>

#define EXTLOG(level, text) Logger::Log(level, __FILE__, text)
#define LOG_ERROR(text) EXTLOG(Logger::Level::Error, text)
#define LOG_WARNING(text) EXTLOG(Logger::Level::Warning, text)
#define LOG_INFO(text) EXTLOG(Logger::Level::Info, text)
#define LOG_DEBUG(text) EXTLOG(Logger::Level::Debug, text)
#define LOG_SUCCESS(text) EXTLOG(Logger::Level::Success, text)

class Logger
{
public:
	struct Prefix
	{
		char color;
		const char* text;
	};

	enum class Level
	{
		Error,
		Warning,
		Info,
		Debug,
		Success
	};

	static void Log(Level logLevel, const char* filename, std::string text)
	{
		std::cout << "[";

		const auto hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		const auto prefix = GetLevelPrefix(logLevel);
		SetConsoleTextAttribute(hConsole, prefix.color);

		std::cout << prefix.text;

		SetConsoleTextAttribute(hConsole, 15);

		std::cout << "] " << text << std::endl;
	}

	static Prefix GetLevelPrefix(Level level)
	{
		switch (level)
		{
		case Level::Error:
			return { FOREGROUND_INTENSITY | FOREGROUND_RED, "ERROR" };
		case Level::Warning:
			return { FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_RED, "WARNING" };
		case Level::Info:
			return { FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE, "INFO"};
		case Level::Debug:
			return { FOREGROUND_INTENSITY | FOREGROUND_BLUE, "DEBUG" };
		case Level::Success:
			return { FOREGROUND_INTENSITY | FOREGROUND_GREEN, "SUCCESS" };
		default:
			return { FOREGROUND_INTENSITY, "" };
		}
	}
};