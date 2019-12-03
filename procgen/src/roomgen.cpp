#include "roomgen.h"

int RoomGenerator::count_neighbors(int idx, int type) {
    int x, y;
    game->to_grid_xy(idx, &x, &y);
    int neighbors = 0;

    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            int curr_obj = game->get_obj(x + i, y + j);

            if (curr_obj == type) {
                neighbors++;
            }
        }
    }

    return neighbors;
}

void RoomGenerator::update() {
    // update cellular automata
    std::vector<int> next_cells;

    for (int i = 0; i < game->grid_size; i++) {
        if (count_neighbors(i, WALL_OBJ) >= 5) {
            next_cells.push_back(WALL_OBJ);
        } else {
            next_cells.push_back(SPACE);
        }
    }

    for (int i = 0; i < game->grid_size; i++) {
        game->set_obj(i, next_cells[i]);
    }
}

void RoomGenerator::build_room(int idx, std::set<int> &room) {
    std::queue<int> curr;

    if (game->get_obj(idx) != SPACE)
        return;

    curr.push(idx);

    while (curr.size() > 0) {
        int curr_idx = curr.front();
        curr.pop();

        if (game->get_obj(curr_idx) != SPACE)
            continue;

        int x, y;
        game->to_grid_xy(curr_idx, &x, &y);

        for (int i = -1; i <= 1; i++) {
            for (int j = -1; j <= 1; j++) {
                if ((i == 0 || j == 0) && (i + j != 0)) {
                    int next_idx = game->to_grid_idx(x + i, y + j);

                    if (!set_contains(room, next_idx) && game->get_obj(next_idx) == SPACE) {
                        curr.push(next_idx);
                        room.insert(next_idx);
                    }
                }
            }
        }
    }
}

void RoomGenerator::find_path(int src, int dst, std::vector<int> &path) {
    std::set<int> covered;
    std::vector<int> expanded;
    std::vector<int> parents;

    if (game->get_obj(src) != SPACE)
        return;

    expanded.push_back(src);
    parents.push_back(-1);

    int search_idx = 0;

    while (search_idx < int(expanded.size())) {
        int curr_idx = expanded[search_idx];

        if (curr_idx == dst)
            break;
        if (game->get_obj(curr_idx) != SPACE)
            continue;

        int x, y;
        game->to_grid_xy(curr_idx, &x, &y);

        for (int i = -1; i <= 1; i++) {
            for (int j = -1; j <= 1; j++) {
                if ((i == 0 || j == 0) && (i + j != 0)) {
                    int next_idx = game->to_grid_idx(x + i, y + j);

                    if (!set_contains(covered, next_idx) && game->get_obj(next_idx) == SPACE) {
                        expanded.push_back(next_idx);
                        parents.push_back(search_idx);
                        covered.insert(next_idx);
                    }
                }
            }
        }

        search_idx++;
    }

    if (expanded[search_idx] == dst) {
        std::vector<int> tmp;

        while (search_idx >= 0) {
            tmp.push_back(expanded[search_idx]);
            search_idx = parents[search_idx];
        }

        for (int j = (int)(tmp.size()) - 1; j >= 0; j--) {
            path.push_back(tmp[j]);
        }
    }
}

void RoomGenerator::find_best_room(std::set<int> &best_room) {
    std::set<int> all_rooms;
    all_rooms.clear();
    best_room.clear();

    int best_room_size = -1;

    for (int i = 0; i < game->grid_size; i++) {
        if (game->get_obj(i) == SPACE && !set_contains(all_rooms, i)) {
            std::set<int> next_room;
            build_room(i, next_room);
            all_rooms.insert(next_room.begin(), next_room.end());

            if (int(next_room.size()) > best_room_size) {
                best_room_size = (int)(next_room.size());
                best_room = next_room;
            }
        }
    }
}

void RoomGenerator::expand_room(std::set<int> &set, int n) {
    std::set<int> curr_set;
    curr_set.insert(set.begin(), set.end());

    for (int loop = 0; loop < n; loop++) {
        std::set<int> next;

        for (int curr_idx : curr_set) {
            if (game->get_obj(curr_idx) != SPACE)
                continue;

            int x, y;
            game->to_grid_xy(curr_idx, &x, &y);

            for (int i = -1; i <= 1; i++) {
                for (int j = -1; j <= 1; j++) {
                    if (i != 0 || j != 0) {
                        int next_idx = game->to_grid_idx(x + i, y + j);

                        if (!set_contains(set, next_idx) && game->get_obj(next_idx) == SPACE) {
                            set.insert(next_idx);
                            next.insert(next_idx);
                        }
                    }
                }
            }
        }

        curr_set = next;
    }
}
