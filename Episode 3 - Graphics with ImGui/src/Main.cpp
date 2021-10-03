#include <iostream>
#include <thread>
#include <fmt/format.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>

#include "Tramp.hpp"

#include "ImGui/imgui.h"
#include "ImGui/examples/imgui_impl_opengles3.h"
#include "ImGui/examples/libs/gl3w/GL/gl3w.h"
#include <SDL2/SDL.h>

namespace Vars
{
	extern bool showMenu;
}

bool Vars::showMenu = true;


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


static std::uint8_t swapWindowBytes[4] = {};
static Tramp* swapWindow = nullptr;
static std::uint8_t pollEventBytes[4] = {};
static Tramp* pollEvent = nullptr;

static void mySwapWindow(SDL_Window* window)
{
	static bool first = true;
	if (first)
	{
		first = false;

		gl3wInit();
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO();
		// Adapted from https://github.com/ocornut/imgui/blob/fba756176d659a8de8f711f68a4a04c675add942/backends/imgui_impl_sdl.cpp#L195
		io.KeyMap[ImGuiKey_Tab] = SDLK_TAB;
		io.KeyMap[ImGuiKey_LeftArrow] = SDL_SCANCODE_LEFT;
		io.KeyMap[ImGuiKey_RightArrow] = SDL_SCANCODE_RIGHT;
		io.KeyMap[ImGuiKey_UpArrow] = SDL_SCANCODE_UP;
		io.KeyMap[ImGuiKey_DownArrow] = SDL_SCANCODE_DOWN;
		io.KeyMap[ImGuiKey_PageUp] = SDL_SCANCODE_PAGEUP;
		io.KeyMap[ImGuiKey_PageDown] = SDL_SCANCODE_PAGEDOWN;
		io.KeyMap[ImGuiKey_Home] = SDL_SCANCODE_HOME;
		io.KeyMap[ImGuiKey_End] = SDL_SCANCODE_END;
		io.KeyMap[ImGuiKey_Delete] = SDLK_DELETE;
		io.KeyMap[ImGuiKey_Backspace] = SDLK_BACKSPACE;
		io.KeyMap[ImGuiKey_Enter] = SDLK_RETURN;
		io.KeyMap[ImGuiKey_Escape] = SDLK_ESCAPE;
		io.KeyMap[ImGuiKey_A] = SDLK_a;
		io.KeyMap[ImGuiKey_C] = SDLK_c;
		io.KeyMap[ImGuiKey_V] = SDLK_v;
		io.KeyMap[ImGuiKey_X] = SDLK_x;
		io.KeyMap[ImGuiKey_Y] = SDLK_y;
		io.KeyMap[ImGuiKey_Z] = SDLK_z;

		ImGui_ImplOpenGL3_Init("#version 100");
	}

	ImGuiIO& io = ImGui::GetIO();
	int w, h;
	SDL_GetWindowSize(window, &w, &h);
	io.DisplaySize = ImVec2((float)w, (float)h);
	io.DisplayFramebufferScale = ImVec2(1.f, 1.f);

	io.WantCaptureMouse = true;
	io.WantCaptureKeyboard = true;

	ImGui_ImplOpenGL3_NewFrame();

	static double lastTime = 0.0;
	double time = SDL_GetTicks() / 1000.0;
	io.DeltaTime = lastTime > 0.f ? ((float)(time-lastTime)) : 1.f/60.f;

	if (io.WantCaptureMouse)
	{
		int mx, my;
		SDL_GetMouseState(&mx, &my);

		io.MousePos = ImVec2((float)mx, (float)my);

		SDL_ShowCursor(io.MouseDrawCursor ? 0 : 1);
	}

	ImGui::NewFrame();
	{
		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.0f);
		ImGui::Begin("", (bool*)true, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs);

		if (Vars::showMenu)
		ImGui::ShowDemoWindow();

		ImGui::End();
	}
	ImGui::EndFrame();


	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	swapWindow->GetOriginalFunction<void(*)(SDL_Window*)>()(window);
}

int myPollEvent(SDL_Event* ev)
{
	ImGuiIO& io = ImGui::GetIO();

	if (ev->type == SDL_KEYDOWN && ev->key.keysym.sym == SDLK_INSERT)
		Vars::showMenu = !Vars::showMenu;


	switch (ev->type)
	{
		case SDL_MOUSEWHEEL:
			if (ev->wheel.y > 0)
				io.MouseWheel = 1;
			if (ev->wheel.y < 0)
				io.MouseWheel = -1;
			break;
		case SDL_MOUSEBUTTONDOWN:
			if (ev->button.button == SDL_BUTTON_LEFT)
				io.MouseDown[0] = true;
			if (ev->button.button == SDL_BUTTON_RIGHT)
				io.MouseDown[1] = true;
			if (ev->button.button == SDL_BUTTON_MIDDLE)
				io.MouseDown[2] = true;
			break;
		case SDL_MOUSEBUTTONUP:
			if (ev->button.button == SDL_BUTTON_LEFT)
				io.MouseDown[0] = false;
			if (ev->button.button == SDL_BUTTON_RIGHT)
				io.MouseDown[1] = false;
			if (ev->button.button == SDL_BUTTON_MIDDLE)
				io.MouseDown[2] = false;
			break;
		case SDL_TEXTINPUT:
			io.AddInputCharactersUTF8(ev->text.text);
			break;
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			int key = ev->key.keysym.sym & ~SDLK_SCANCODE_MASK;
			io.KeysDown[key] = (ev->type == SDL_KEYDOWN);
			io.KeyShift = ((SDL_GetModState() & KMOD_SHIFT) != 0);
			io.KeyCtrl = ((SDL_GetModState() & KMOD_CTRL) != 0);
			io.KeyAlt = ((SDL_GetModState() & KMOD_ALT) != 0);
			io.KeySuper = ((SDL_GetModState() & KMOD_GUI) != 0);
			break;
	}

	return pollEvent->GetOriginalFunction<int(*)(SDL_Event*)>()(ev);
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
			swapWindowBytes[i] = *reinterpret_cast<std::uint8_t*>(gateway+5+i); // Store for later
			*reinterpret_cast<std::uint8_t*>(gateway+5+i) = ((addr+9) >> 8*i) & 0xFF;
		}
	});

	// 53                    - push rbx
	// 83 EC 18              - sub esp,18
	// E8 BB8EFDFF           - call libSDL2-2.0.so.0+A6F6 <- must be patched (same as swapWindow)
	// 81 C3 6D370A00        - add ebx,000A376D
	// C7 44 24 04 00000000  - mov [rsp+04],00000000
	pollEvent = new Tramp(getSymbol("libSDL2-2.0.so.0", "SDL_PollEvent"), reinterpret_cast<std::uintptr_t>(&myPollEvent), 9,
	[](std::uintptr_t addr, std::uintptr_t gateway)
	{
		*reinterpret_cast<std::uint8_t*>(gateway+4) = 0xBB; // mov into ebx
		for (std::size_t i = 0; i < 4; ++i)
		{
			pollEventBytes[i] = *reinterpret_cast<std::uint8_t*>(gateway+5+i); // Store for later
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
	swapWindow->Release();
	pollEvent->Release();
	fmt::print("Hack unloaded!\n");
}
