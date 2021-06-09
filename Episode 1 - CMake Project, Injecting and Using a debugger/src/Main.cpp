#include <iostream>
#include <thread>
#include <fmt/format.h>
#include <dlfcn.h>

void Initialize()
{
	fmt::print("Hack loaded!\n");
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
