#pragma once

/*

Generate a maze using kruskal's algorithm

*/

#include <memory>
#include <set>
#include "grid.h"
#include "randgen.h"

const int MAZE_OFFSET = 1;

class MazeGen {
  public:
    Grid<int> grid;

    MazeGen(RandGen *_rand_gen, int _maze_dim);
    void generate_maze();
    void generate_maze_no_dead_ends();
    void generate_maze_with_doors(int num_doors);
    void place_objects(int start_obj, int num_objs);

  private:
    RandGen *rand_gen;
    int maze_dim;
    int array_dim;

    int num_free_cells;
    std::vector<std::set<int>> cell_sets;
    std::vector<int> cell_sets_idxs;
    std::set<int> free_cell_set;
    std::vector<int> free_cells;

    void get_neighbors(int idx, int type, std::vector<int> &neighbors);
    int lookup(int x, int y);
    void set_free_cell(int x, int y);
    void set_obj(int idx, int type);
    int to_index(int x, int y);
    int get_obj(int idx);
    std::vector<int> filter_cells(int type);
    int expand_to_type(std::set<int> &s0, std::set<int> &s1, int type);
};
