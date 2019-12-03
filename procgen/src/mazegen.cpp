#include "mazegen.h"
#include "object-ids.h"
#include "cpp-utils.h"

struct Wall {
    int x1;
    int y1;
    int x2;
    int y2;
};

MazeGen::MazeGen(RandGen *_rand_gen, int _maze_dim) {
    rand_gen = _rand_gen;
    maze_dim = _maze_dim;
    array_dim = maze_dim + 2;
    cell_sets.resize(array_dim * array_dim);
    cell_sets_idxs.resize(array_dim * array_dim);
    free_cells.resize(array_dim * array_dim);
    grid.resize(array_dim, array_dim);
}

int MazeGen::lookup(int x, int y) {
    return cell_sets_idxs[maze_dim * y + x];
}

void MazeGen::set_free_cell(int x, int y) {
    grid.set(x + MAZE_OFFSET, y + MAZE_OFFSET, SPACE);
    int cell = maze_dim * y + x;
    if (free_cell_set.find(cell) == free_cell_set.end()) {
        free_cells[num_free_cells] = cell;
        free_cell_set.insert(cell);
        num_free_cells += 1;
    }
}

int MazeGen::get_obj(int idx) {
    int x = idx % array_dim;
    int y = idx / array_dim;

    if (x <= 0 || x >= array_dim - 1)
        return INVALID_OBJ;
    if (y <= 0 || y >= array_dim - 1)
        return INVALID_OBJ;

    return grid.get(x, y);
}

void MazeGen::get_neighbors(int idx, int type, std::vector<int> &neighbors) {
    int x = idx % array_dim;
    int y = idx / array_dim;

    neighbors.clear();

    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0)
                continue;
            if (dx != 0 && dy != 0)
                continue;

            int n_idx = grid.to_index(x + dx, y + dy);
            if (get_obj(n_idx) == type) {
                neighbors.push_back(n_idx);
            }
        }
    }
}

int MazeGen::expand_to_type(std::set<int> &s0, std::set<int> &s1, int type) {
    std::set<int> curr = s0;

    std::vector<int> target_elems;
    std::vector<int> adj_space;

    while (curr.size() > 0) {
        std::set<int> next;

        for (int elem : curr) {
            get_neighbors(elem, type, target_elems);
            get_neighbors(elem, SPACE, adj_space);

            for (int j : adj_space) {
                if (!set_contains(s0, j) && !set_contains(s1, j)) {
                    next.insert(j);
                    s1.insert(j);
                }
            }

            if (target_elems.size() > 0) {
                return target_elems[0];
            }
        }

        curr = next;
    }

    return -1;
}

std::vector<int> MazeGen::filter_cells(int type) {
    std::vector<int> found;

    for (int i = 0; i < array_dim * array_dim; i++) {
        if (get_obj(i) == type) {
            found.push_back(i);
        }
    }

    return found;
}

void MazeGen::generate_maze() {
    for (int i = 0; i < array_dim; i++) {
        for (int j = 0; j < array_dim; j++) {
            grid.set(i, j, WALL_OBJ);
        }
    }

    grid.set(MAZE_OFFSET, MAZE_OFFSET, 0);

    std::vector<Wall> walls;

    num_free_cells = 0;
    free_cell_set.clear();

    std::set<int> *s0 = &cell_sets[0];
    s0->clear();
    s0->insert(0);
    cell_sets_idxs[0] = 0;

    for (int i = 1; i < maze_dim * maze_dim; i++) {
        std::set<int> *s1 = &cell_sets[i];
        s1->clear();
        s1->insert(i);
        cell_sets_idxs[i] = i;
    }

    for (int i = 1; i < maze_dim; i += 2) {
        for (int j = 0; j < maze_dim; j += 2) {
            if (i > 0 && i < maze_dim - 1) {
                walls.push_back(Wall({i - 1, j, i + 1, j}));
            }
        }
    }

    for (int i = 0; i < maze_dim; i += 2) {
        for (int j = 1; j < maze_dim; j += 2) {
            if (j > 0 && j < maze_dim - 1) {
                walls.push_back(Wall({i, j - 1, i, j + 1}));
            }
        }
    }

    while (walls.size() > 0) {
        int n = rand_gen->randn((int)(walls.size()));
        Wall wall = walls[n];

        int s0_idx = lookup(wall.x1, wall.y1);
        s0 = &cell_sets[s0_idx];
        int s1_idx = lookup(wall.x2, wall.y2);
        std::set<int> *s1 = &cell_sets[s1_idx];

        int x0 = (wall.x1 + wall.x2) / 2;
        int y0 = (wall.y1 + wall.y2) / 2;
        int center = maze_dim * y0 + x0;

        bool can_remove =
            (grid.get(x0 + MAZE_OFFSET, y0 + MAZE_OFFSET) == WALL_OBJ) &&
            (s0_idx != s1_idx);

        if (can_remove) {
            set_free_cell(wall.x1, wall.y1);
            set_free_cell(x0, y0);
            set_free_cell(wall.x2, wall.y2);

            s1->insert(s0->begin(), s0->end());
            s1->insert(center);

            std::set<int>::iterator it;
            for (it = s1->begin(); it != s1->end(); ++it) {
                cell_sets_idxs[*it] = s1_idx;
            }
        }

        walls.erase(walls.begin() + n);
    }
}

// Generate a maze that has no dead ends. Approximates a MsPacman style maze.
void MazeGen::generate_maze_no_dead_ends() {
    generate_maze();

    std::vector<int> adj_space;
    std::vector<int> adj_wall;

    for (int i = 0; i < array_dim * array_dim; i++) {
        if (get_obj(i) == SPACE) {
            get_neighbors(i, SPACE, adj_space);

            if (adj_space.size() == 1) {
                get_neighbors(i, WALL_OBJ, adj_wall);

                if (adj_wall.size() > 0) {
                    int n = rand_gen->randn((int)(adj_wall.size()));
                    grid.set_index(adj_wall[n], SPACE);
                }
            }
        }
    }
}

// Generate a maze with doors
void MazeGen::generate_maze_with_doors(int num_doors) {
    generate_maze();

    std::vector<int> forks;

    std::vector<int> adj_space;
    std::vector<int> adj_wall;

    for (int i = 0; i < array_dim * array_dim; i++) {
        if (get_obj(i) == SPACE) {
            get_neighbors(i, SPACE, adj_space);
            get_neighbors(i, WALL_OBJ, adj_wall);

            if (adj_space.size() > 2) {
                forks.push_back(i);
            }
        }
    }

    std::vector<int> chosen = rand_gen->choose_n(forks, num_doors);

    num_doors = (int)(chosen.size());

    for (int i : chosen) {
        grid.set_index(i, DOOR_OBJ);
    }

    int agent_cell;
    {
        std::vector<int> space_cells = filter_cells(SPACE);
        std::vector<int> door_neighbors;

        // don't let the agent spawn next to a door, as there might not be room
        // for the key
        do {
            agent_cell = rand_gen->choose_one(space_cells);
            door_neighbors.clear();
            get_neighbors(agent_cell, DOOR_OBJ, door_neighbors);
        } while (door_neighbors.size() > 0);

        grid.set_index(agent_cell, AGENT_OBJ);
    }

    std::set<int> s0;
    s0.insert(agent_cell);

    for (int door_num = 0; door_num < num_doors + 1; door_num++) {
        std::set<int> s1;
        int found_door = -1;

        if (door_num < num_doors) {
            found_door = expand_to_type(s0, s1, DOOR_OBJ);
            grid.set_index(found_door, DOOR_OBJ + door_num + 1);
            s0.insert(s1.begin(), s1.end());
        }

        expand_to_type(s0, s1, -999);

        std::vector<int> space_cells;

        for (int x : s1) {
            space_cells.push_back(x);
        }

        fassert(space_cells.size() > 0);

        int key_cell = rand_gen->choose_one(space_cells);
        grid.set_index(key_cell, door_num == num_doors
                                     ? EXIT_OBJ
                                     : (KEY_OBJ + door_num + 1));

        s0.insert(s1.begin(), s1.end());

        if (found_door >= 0) {
            s0.insert(found_door);
        }
    }
}

void MazeGen::place_objects(int start_obj, int num_objs) {
    for (int j = 0; j < num_objs; j++) {
        int m = rand_gen->randn(num_free_cells);

        while (free_cells[m] == -1 || free_cells[m] == 0) {
            m = rand_gen->randn(num_free_cells);
        }

        int coin_cell = free_cells[m];
        free_cells[m] = -1;

        grid.set(coin_cell % maze_dim + MAZE_OFFSET,
                 coin_cell / maze_dim + MAZE_OFFSET, start_obj + j);
    }
}
