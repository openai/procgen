#include "game-registry.h"

std::map<std::string, std::function<std::shared_ptr<Game>()>> *globalGameRegistry = nullptr;