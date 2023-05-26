#pragma once


#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define NOMINMAX
#include <Windows.h>
#pragma warning(push)
#pragma warning( disable: 4005 )
#include <d3d11.h>
#pragma warning(pop)
#include <d3dcompiler.h>

#include <wrl/client.h>
template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

#include <cassert>
#include <cstdint>
#include <atomic>
#include <array>
#include <memory>
#include <cassert>
#include <stdexcept>
#include <functional>
#include <chrono>
#include <thread>
#include <optional>
#include <cmath>
#include <iostream>
#include <mutex>
#include <filesystem>
#include <fstream>


