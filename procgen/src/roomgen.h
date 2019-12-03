#pragma once

/*

Cellular-automata based room generation

*/

#include "basic-abstract-game.h"

class RoomGenerator {
  public:
    RoomGenerator(BasicAbstractGame *game)
        : game(game){};

    void update();
    void find_path(int src, int dst, std::vector<int> &path);
    void find_best_room(std::set<int> &best_room);
    void expand_room(std::set<int> &set, int n);

  private:
    BasicAbstractGame *game;

    void build_room(int idx, std::set<int> &room);
    int count_neighbors(int idx, int type);
};