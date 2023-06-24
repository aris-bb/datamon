#include <iostream>

#include "../libdatamon/libdatamon.hpp"

// example data that we want to monitor for read/write access
struct Player {
  int health;
  int armor;
  int ammo;
  char name[32];

  // padding to make the struct as big as the page size
  // otherwise the PAGE_GUARD flag might interfere with other memory
  // preventing the callback from being called, etc.
  char padding[4052];
};

void callback(void* accessing_address, bool read, void* data) {
  // this callback is called whenever the example data is accessed

  // print "[DATAMON]" in red
  std::cout << "\033[1;31m[DATAMON]\033[0m";

  // print other information about the access
  std::cout << " Intercepted " << (read ? "read" : "write")
            << ". Data address: " << std::hex
            << reinterpret_cast<uintptr_t>(data) << std::dec
            << ", caused from: " << std::hex
            << reinterpret_cast<uintptr_t>(accessing_address) << std::dec
            << ".\n";
}

int main() {
  // allocate the example data on the heap
  auto player = std::make_unique<Player>();

  // initialize datamon and watch the example data
  datamon::Datamon dm{player.get(), sizeof(*player), callback};

  // do some stuff with the example data and see how datamon intercepts it
  // all of the accesses below will be intercepted by datamon

  std::cout << "Setting health to 100.\n";

  player->health = 100;

  std::cout << "Setting armor to 100.\n";

  player->armor = 100;

  std::cout << "Setting ammo to 100.\n";

  player->ammo = 100;

  std::cout << "Setting name to \"datamon\".\n";

  strcpy_s(player->name, "datamon");

  std::cout << "Reading health...\n";
  int health = player->health;
  std::cout << "Health: " << health << "\n";

  std::cout << "Reading armor...\n";
  int armor = player->armor;
  std::cout << "Armor: " << armor << "\n";

  std::cout << "Reading ammo...\n";
  int ammo = player->ammo;
  std::cout << "Ammo: " << ammo << "\n";

  std::cout << "Reading name...\n";
  char name[32];
  strcpy_s(name, player->name);
  std::cout << "Name: " << name << "\n";

  return 0;
}