#ifndef TRAMP_HPP
#define TRAMP_HPP

#include <functional>
#include <vector>
#include <memory>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>
#include <elf.h>
#include <link.h>

static long pageSize = sysconf(_SC_PAGESIZE);
static std::pair<std::uintptr_t, std::uintptr_t> getPage(std::uintptr_t addr, std::size_t len)
{
	std::uintptr_t ret = (addr / pageSize) * pageSize;

	std::size_t n (addr - ret + len);
	if (n % pageSize)
		n /= pageSize, ++n;
	else
		n /= pageSize;

	return {ret, n*pageSize};
}

static Elf64_Word getProtectionFlags(uintptr_t address)
{
	static Elf64_Word flags = 0;
	static std::uintptr_t addr = address;

	dl_iterate_phdr([](struct dl_phdr_info* info, std::size_t, void*)
	{
		std::uintptr_t startingAddr = 0;
		std::uintptr_t endingAddr = 0;

		for (Elf64_Half i = 0; i < info->dlpi_phnum; i++)
		{
			const ElfW(Phdr) *hdr = &info->dlpi_phdr[i];
			if (hdr->p_memsz)
			{
				startingAddr = info->dlpi_addr + hdr->p_vaddr;
				endingAddr = startingAddr + hdr->p_memsz;
				if (startingAddr <= addr && endingAddr >= addr)
					flags |= hdr->p_flags;
			}
		}

		return 0;
	}, nullptr);

	return flags;
}

class Tramp
{
	std::uintptr_t m_gateway = 0;
	std::uintptr_t m_addr = 0;
	std::size_t m_len;
public:
	Tramp(std::uintptr_t fn, std::uintptr_t hkFn, std::size_t len, std::function<void(std::uintptr_t, std::uintptr_t)> patch = [](std::uintptr_t, std::uintptr_t){})
	{
		m_addr = fn;
		m_len = len;

		if (m_len < 5)
			throw ("Tramp::Tramp() len is smaller than 5\n");

		// Allocate array that can hold the original opcodes
		const std::size_t gatewayLen = m_len + 5;
		m_gateway = reinterpret_cast<std::uintptr_t>(new std::uint8_t[gatewayLen]);

		const auto prot = getProtectionFlags(m_addr);
		// Set protection
		if (auto&& [addr, len] = getPage(m_addr, m_len); mprotect(reinterpret_cast<void*>(addr), len, PROT_READ | PROT_WRITE | PROT_EXEC) == -1)
			throw ("Tramp::Tramp() mprotect(addr) failed to set writable\n");

		// Copy bytes & Write new bytes
		std::memcpy(reinterpret_cast<void*>(m_gateway), reinterpret_cast<void*>(m_addr), m_len);
		reinterpret_cast<std::uint8_t*>(m_gateway)[m_len] = 0xE9; // jump near
		*reinterpret_cast<std::uint32_t*>(m_gateway+m_len+1) =  m_addr - m_gateway - 5;
		
		// Patches
		patch(m_addr, m_gateway);

		std::memset(reinterpret_cast<void*>(m_addr), 0x90, m_len);
		reinterpret_cast<std::uint8_t*>(m_addr)[0] = 0xE9; // jump near
		*reinterpret_cast<std::uint32_t*>(m_addr+1) = hkFn - m_addr - 5;

		// Revert original protection
		if (auto&& [addr, len] = getPage(m_addr, m_len); mprotect(reinterpret_cast<void*>(addr), len, prot) == -1)
			throw ("Tramp::Tramp() mprotect(addr) failed to restore permissions\n");

		// Make the gateway executable
		if (auto&& [addr, len] = getPage(reinterpret_cast<std::uintptr_t>(m_gateway), gatewayLen); mprotect(reinterpret_cast<void*>(addr), len, PROT_READ | PROT_WRITE | PROT_EXEC) == -1)
			throw ("Tramp::Tramp() mprotect(gateway) failed to set executable\n");
	}

	void Release(std::function<void(std::uintptr_t, std::uintptr_t)> patch = [](std::uintptr_t, std::uintptr_t){})
	{

		const auto prot = getProtectionFlags(m_addr);
		// Set protection
		if (auto&& [addr, len] = getPage(m_addr, m_len); mprotect(reinterpret_cast<void*>(addr), len, PROT_READ | PROT_WRITE | PROT_EXEC) == -1)
			throw ("Tramp::~Tramp() mprotect(addr) failed to set writable\n");

		// Write the original bytes
		std::memcpy(reinterpret_cast<void*>(m_addr), reinterpret_cast<void*>(m_gateway), m_len);

		patch(m_addr, m_gateway);

		// Revert original protection
		if (auto&& [addr, len] = getPage(m_addr, m_len); mprotect(reinterpret_cast<void*>(addr), len, prot) == -1)
			throw ("Tramp::~Tramp() mprotect(addr) failed to restore permissions\n");

		// Free memory
		delete[] reinterpret_cast<std::uint8_t*>(m_gateway);
	}

	~Tramp()
	{
	}

	template <class Fn>
	inline Fn GetOriginalFunction()
	{
		return reinterpret_cast<Fn>(m_gateway);
	}
};

#endif // TRAMP_HPP
