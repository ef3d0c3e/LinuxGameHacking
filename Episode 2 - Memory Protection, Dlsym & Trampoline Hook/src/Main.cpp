#include <iostream>
#include <thread>
#include <fmt/format.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>

#include "Tramp.hpp"
#include "SDL2/SDL.h"

void makeWritable(std::uintptr_t addr, std::size_t len)
{
	if (auto&& [_addr, _len] = getPage(addr, len); mprotect(reinterpret_cast<void*>(_addr), _len, PROT_EXEC | PROT_READ | PROT_WRITE) == -1)
		throw "makeWritable() : `mprotect` failed";
}

std::uintptr_t getSymbol(const std::string& file, const std::string& symbol)
{
	void* handle = dlopen(file.c_str(), 2);
	if (handle == nullptr)
		throw fmt::format("dlopen({}) returned nullptr", file);
	void* addr = dlsym(handle, symbol.c_str());
	if (addr == nullptr)
		throw fmt::format("dlsym({}) returned nullptr in `{}`", symbol, file);
	if (dlclose(handle) != 0)
		throw fmt::format("dlclose({}) failed", file);

	return reinterpret_cast<std::uintptr_t>(addr);
}

static std::uint8_t bytes[4] = {};
static Tramp* swapWindow = nullptr;

static void mySwapWindow(SDL_Window* window)
{
	swapWindow->GetOriginalFunction<void(*)(SDL_Window*)>()(window);
	std::cout << "Frame!\n";
}

void Initialize()
{
	fmt::print("Hack loaded!\n");

	// 53                    - push rbx
	// 83 EC 18              - sub esp,18
	// E8 F28FF7FF           - call libSDL2-2.0.so.0+A6F6 <- moves the address of the next instruction into rbx
	// 81 C3 A4380400        - add ebx,000438A4
	// 8B 44 24 20           - mov eax,[rsp+20]
	// 8B 93 24860000        - mov edx,[rbx+00008624]
	// 85 D2                 - test edx,edx
	swapWindow = new Tramp(getSymbol("libSDL2-2.0.so.0", "SDL_GL_SwapWindow"), reinterpret_cast<std::uintptr_t>(&mySwapWindow), 9,
	[](std::uintptr_t addr, std::uintptr_t gateway)
	{
		*reinterpret_cast<std::uint8_t*>(gateway+4) = 0xBB; // mov into ebx
		for (std::size_t i = 0; i < 4; ++i)
		{
			bytes[i] = *reinterpret_cast<std::uint8_t*>(gateway+5+i); // Store for later
			*reinterpret_cast<std::uint8_t*>(gateway+5+i) = ((addr+9) >> 8*i) & 0xFF;
		}
	});
}

std::thread* thread;
int __attribute__((constructor)) Start()
{
	thread = new std::thread(Initialize);

	return 0;
}

void __attribute__((destructor)) Stop()
{
	thread->join();
	fmt::print("Hack unloaded!\n");
}
