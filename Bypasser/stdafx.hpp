#pragma once

#define WIN32_LEAN_AND_MEAN

// includes
#include <windows.h>
#include <iostream>
#include <iomanip>
#include <intrin.h>

#include <shared_mutex>
#include <filesystem>
#include <sstream>

// global defs
#include "crc32.hpp"
#include "valve.hpp"
#include "memory.hpp"
#include "Logger.hpp"