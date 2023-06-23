#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>

// Abstract class
class Entity
{
public:
	virtual ~Entity() {}

	virtual const std::string& getName() = 0;
	virtual int getHealth() = 0;
};

// Player class, derived from entity
class Player : public Entity
{
	int m_health;
	std::string m_name;
public:
	Player(int health, std::string name):
		m_health{health}, m_name{name} {}
	
	
	virtual const std::string& getName() { return m_name; }
	virtual int getHealth() { return m_health; }
};

class Monster : public Entity
{
	int m_health;
public:
	Monster():
		m_health{100} {}


	virtual const std::string& getName() { return "Monster"; }
	virtual int getHealth() { return m_health*2; }
};

int getHealthCustom(void* thisptr)
{
	return 99;
}


long pageSize = sysconf(_SC_PAGESIZE);
std::pair<std::uintptr_t, std::size_t> getMinimumPage(std::uintptr_t addr, std::size_t len)
{
	std::uintptr_t ret = (addr / pageSize) * pageSize;

	std::size_t n = (addr - ret + len);
	if (n % pageSize)
		n /= pageSize, ++n;
	else
		n /= pageSize;
	
	return {ret, n*pageSize};
}

int main()
{
	Entity* ent1 = new Player(100, "Sven");

	char* vtbl = *((char**)ent1); // Get vtable address
	char* address = (char*)getHealthCustom;

	// Make writable
	auto [start, len] = getMinimumPage((std::uintptr_t)vtbl+3, 8);
	std::cout << mprotect((char*)start, len, PROT_READ | PROT_WRITE | PROT_EXEC) << std::endl;

	*((void**)vtbl+3) = address; // Write our custom function


	std::cout << ent1->getHealth() << std::endl; // Call

	return 0;
}
