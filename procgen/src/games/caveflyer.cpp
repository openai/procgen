#include "../basic-abstract-game.h"
#include "../assetgen.h"
#include "../roomgen.h"
#include <set>
#include <queue>

const std::string NAME = "caveflyer";

const float GOAL_REWARD = 10.0f;
const float TARGET_REWARD = 3.0f;

const int GOAL = 1;
const int OBSTACLE = 2;
const int TARGET = 3;
const int PLAYER_BULLET = 4;
const int ENEMY = 5;
const int CAVEWALL = 8;
const int EXHAUST = 9;

const int MARKER = 1003;

class CaveFlyerGame : public BasicAbstractGame {
  public:
    std::unique_ptr<RoomGenerator> room_manager;

    CaveFlyerGame()
        : BasicAbstractGame(NAME) {
        mixrate = 0.9f;
        room_manager = std::make_unique<RoomGenerator>(this);
    }

    void load_background_images() override {
        main_bg_images_ptr = &space_backgrounds;
    }

    void asset_for_type(int type, std::vector<std::string> &names) override {
        if (type == GOAL) {
            names.push_back("misc_assets/ufoGreen2.png");
        } else if (type == OBSTACLE) {
            names.push_back("misc_assets/meteorBrown_big1.png");
        } else if (type == TARGET) {
            names.push_back("misc_assets/ufoRed2.png");
        } else if (type == PLAYER_BULLET) {
            names.push_back("misc_assets/laserBlue02.png");
        } else if (type == ENEMY) {
            names.push_back("misc_assets/enemyShipBlue4.png");
        } else if (type == PLAYER) {
            names.push_back("misc_assets/playerShip1_red.png");
        } else if (type == CAVEWALL) {
            names.push_back("misc_assets/groundA.png");
        } else if (type == EXHAUST) {
            names.push_back("misc_assets/towerDefense_tile295.png");
        }
    }

    void handle_agent_collision(const std::shared_ptr<Entity> &obj) override {
        BasicAbstractGame::handle_agent_collision(obj);

        if (obj->type == GOAL) {
            step_data.reward += GOAL_REWARD;
            step_data.level_complete = true;
            step_data.done = true;
        } else if (obj->type == OBSTACLE) {
            step_data.done = true;
        } else if (obj->type == ENEMY) {
            step_data.done = true;
        } else if (obj->type == TARGET) {
            step_data.done = true;
        }
    }

    void update_agent_velocity() override {
        float v_scale = get_agent_acceleration_scale();

        agent->vx += mixrate * maxspeed * action_vx * v_scale * .2;
        agent->vy += mixrate * maxspeed * action_vy * v_scale * .2;

        decay_agent_velocity();
    }

    bool use_block_asset(int type) override {
        return BasicAbstractGame::use_block_asset(type) || (type == CAVEWALL);
    }

    bool is_blocked(const std::shared_ptr<Entity> &src, int target, bool is_horizontal) override {
        if (BasicAbstractGame::is_blocked(src, target, is_horizontal))
            return true;
        if (src->type == PLAYER && target == CAVEWALL)
            return true;

        return false;
    }

    void handle_collision(const std::shared_ptr<Entity> &src, const std::shared_ptr<Entity> &target) override {
        if (target->type == PLAYER_BULLET) {
            bool erase_bullet = false;

            if (src->type == TARGET) {
                src->health -= 1;
                erase_bullet = true;

                if (src->health <= 0 && !src->will_erase) {
                    spawn_child(src, EXPLOSION, .5 * src->rx);
                    src->will_erase = true;
                    step_data.reward += TARGET_REWARD;
                }
            } else if (src->type == OBSTACLE) {
                erase_bullet = true;
            } else if (src->type == ENEMY) {
                erase_bullet = true;
            } else if (src->type == GOAL) {
                erase_bullet = true;
            }

            if (erase_bullet && !target->will_erase) {
                target->will_erase = true;
                auto explosion = spawn_child(target, EXPLOSION, .5 * target->rx);
                explosion->vx = src->vx;
                explosion->vy = src->vy;
            }
        }
    }

    bool will_reflect(int src, int target) override {
        return BasicAbstractGame::will_reflect(src, target) || (src == ENEMY && (target == CAVEWALL || target == out_of_bounds_object));
    }

    void choose_world_dim() override {
        DistributionMode dist_diff = options.distribution_mode;

        int world_dim = 20;

        if (dist_diff == EasyMode) {
            world_dim = 30;
        } else if (dist_diff == HardMode) {
            world_dim = 40;
        } else if (dist_diff == MemoryMode) {
            world_dim = 60;
        }

        main_width = world_dim;
        main_height = world_dim;
    }

    void game_reset() override {
        BasicAbstractGame::game_reset();

        out_of_bounds_object = WALL_OBJ;

        for (int i = 0; i < grid_size; i++) {
            if (rand_gen.rand01() < .5) {
                set_obj(i, WALL_OBJ);
            } else {
                set_obj(i, SPACE);
            }
        }

        for (int iteration = 0; iteration < 4; iteration++) {
            room_manager->update();
        }

        std::set<int> best_room;
        room_manager->find_best_room(best_room);
        fassert(best_room.size() > 0);

        for (int i = 0; i < grid_size; i++) {
            set_obj(i, WALL_OBJ);
        }

        std::vector<int> free_cells;

        for (int i : best_room) {
            set_obj(i, SPACE);
            free_cells.push_back(i);
        }

        std::vector<int> selected_idxs = rand_gen.simple_choose((int)(free_cells.size()), 2);
        int agent_cell = free_cells[selected_idxs[0]];
        int goal_cell = free_cells[selected_idxs[1]];

        agent->x = (agent_cell % main_width) + .5;
        agent->y = (agent_cell / main_width) + .5;

        auto ent = spawn_entity_at_idx(goal_cell, .5, GOAL);
        ent->collides_with_entities = true;

        std::vector<int> goal_path;
        room_manager->find_path(agent_cell, goal_cell, goal_path);

        bool should_prune = options.distribution_mode != MemoryMode;

        if (should_prune) {
            std::set<int> wide_path;
            wide_path.insert(goal_path.begin(), goal_path.end());
            room_manager->expand_room(wide_path, 4);

            for (int i = 0; i < grid_size; i++) {
                set_obj(i, WALL_OBJ);
            }

            for (int i : wide_path) {
                set_obj(i, SPACE);
            }
        }

        for (int iteration = 0; iteration < 4; iteration++) {
            room_manager->update();

            for (int i : goal_path) {
                set_obj(i, SPACE);
            }
        }

        for (int i : goal_path) {
            set_obj(i, MARKER);
        }

        free_cells.clear();

        for (int i = 0; i < grid_size; i++) {
            if (get_obj(i) == SPACE) {
                free_cells.push_back(i);
            } else if (get_obj(i) == WALL_OBJ) {
                set_obj(i, CAVEWALL);
            }
        }

        int chunk_size = ((int)(free_cells.size()) / 80);
        int num_objs = 3 * chunk_size;

        std::vector<int> obstacle_idxs = rand_gen.simple_choose((int)(free_cells.size()), num_objs);

        for (int i = 0; i < num_objs; i++) {
            int val = free_cells[obstacle_idxs[i]];

            if (i < chunk_size) {
                auto e = spawn_entity_at_idx(val, .5, OBSTACLE);
                e->collides_with_entities = true;
            } else if (i < 2 * chunk_size) {
                auto e = spawn_entity_at_idx(val, .5, TARGET);
                e->health = 5;
                e->collides_with_entities = true;
            } else {
                auto e = spawn_entity_at_idx(val, .5, ENEMY);
                float vel = (.1 * rand_gen.rand01() + .1) * (rand_gen.randn(2) * 2 - 1);
                if (rand_gen.rand01() < .5) {
                    e->vx = vel;
                } else {
                    e->vy = vel;
                }
                e->smart_step = true;
                e->collides_with_entities = true;
            }
        }

        for (int i = 0; i < grid_size; i++) {
            int val = get_obj(i);
            if (val == MARKER)
                val = SPACE;
            set_obj(i, val);
        }

        out_of_bounds_object = CAVEWALL;
        visibility = options.distribution_mode == EasyMode ? 10 : 16;
    }

    void set_action_xy(int move_action) override {
        float acceleration = move_action % 3 - 1;
        if (acceleration < 0)
            acceleration *= 0.33f;

        float theta = -1 * agent->rotation + PI / 2;

        if (acceleration > 0) {
            auto exhaust = add_entity(agent->x - agent->rx * cos(theta), agent->y - agent->ry * sin(theta), 0, 0, .5 * agent->rx, EXHAUST);
            exhaust->expire_time = 4;
            exhaust->rotation = -1 * theta - PI / 2;
            exhaust->grow_rate = 1.25;
            exhaust->alpha_decay = 0.8f;
        }

        action_vy = acceleration * sin(theta);
        action_vx = acceleration * cos(theta);
        action_vrot = move_action / 3 - 1;
    }

    void game_step() override {
        BasicAbstractGame::game_step();

        if (special_action == 1) {
            float theta = -1 * agent->rotation + PI / 2;
            float vx = cos(theta);
            float vy = sin(theta);
            auto new_bullet = add_entity_rxy(agent->x, agent->y, vx, vy, 0.1f, 0.25f, PLAYER_BULLET);
            new_bullet->expire_time = 10;
            new_bullet->rotation = agent->rotation;
        }

        for (int ent_idx = (int)(entities.size()) - 1; ent_idx >= 0; ent_idx--) {
            auto ent = entities[ent_idx];
            if (ent->type == ENEMY) {
                ent->face_direction(ent->vx, ent->vy, -1 * PI / 2);
            }

            if (ent->type != PLAYER_BULLET)
                continue;

            bool found_wall = false;

            for (int i = 0; i < 2; i++) {
                for (int j = 0; j < 2; j++) {
                    int type2 = get_obj_from_floats(ent->x + ent->rx * (2 * i - 1), ent->y + ent->ry * (2 * j - 1));
                    found_wall = found_wall || type2 == CAVEWALL;
                }
            }

            if (found_wall) {
                ent->will_erase = true;
                spawn_child(ent, EXPLOSION, .5 * ent->rx);
            }
        }

        erase_if_needed();
    }
};

REGISTER_GAME(NAME, CaveFlyerGame);
