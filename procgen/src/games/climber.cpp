#include "../basic-abstract-game.h"
#include "../assetgen.h"
#include <set>
#include <queue>
#include "../mazegen.h"

const std::string NAME = "climber";

const float COIN_REWARD = 1.0f;
const float COMPLETION_BONUS = 10.0f;

const int COIN = 1;
const int ENEMY = 5;
const int ENEMY1 = 6;
const int ENEMY2 = 7;

const int PLAYER_JUMP = 9;
const int PLAYER_RIGHT1 = 12;
const int PLAYER_RIGHT2 = 13;

const int WALL_MID = 15;
const int WALL_TOP = 16;
const int ENEMY_BARRIER = 19;

const float PATROL_RANGE = 4;

const int NUM_WALL_THEMES = 4;

class Climber : public BasicAbstractGame {
  public:
    bool has_support = false;
    bool facing_right = false;
    int coin_quota = 0;
    int coins_collected = 0;
    int wall_theme = 0;
    float gravity = 0.0f;
    float air_control = 0.0f;

    Climber()
        : BasicAbstractGame(NAME) {
        out_of_bounds_object = WALL_MID;
    }

    void load_background_images() override {
        main_bg_images_ptr = &platform_backgrounds;
    }

    void asset_for_type(int type, std::vector<std::string> &names) override {
        if (type == PLAYER) {
            names.push_back("platformer/playerBlue_stand.png");
            names.push_back("platformer/playerGreen_stand.png");
            names.push_back("platformer/playerGrey_stand.png");
            names.push_back("platformer/playerRed_stand.png");
        } else if (type == PLAYER_JUMP) {
            names.push_back("platformer/playerBlue_walk4.png");
            names.push_back("platformer/playerGreen_walk4.png");
            names.push_back("platformer/playerGrey_walk4.png");
            names.push_back("platformer/playerRed_walk4.png");
        } else if (type == PLAYER_RIGHT1) {
            names.push_back("platformer/playerBlue_walk1.png");
            names.push_back("platformer/playerGreen_walk1.png");
            names.push_back("platformer/playerGrey_walk1.png");
            names.push_back("platformer/playerRed_walk1.png");
        } else if (type == PLAYER_RIGHT2) {
            names.push_back("platformer/playerBlue_walk2.png");
            names.push_back("platformer/playerGreen_walk2.png");
            names.push_back("platformer/playerGrey_walk2.png");
            names.push_back("platformer/playerRed_walk2.png");
        } else if (type == WALL_TOP) {
            names.push_back("platformer/tileBlue_05.png");
            names.push_back("platformer/tileGreen_05.png");
            names.push_back("platformer/tileYellow_06.png");
            names.push_back("platformer/tileBrown_06.png");
        } else if (type == WALL_MID) {
            names.push_back("platformer/tileBlue_08.png");
            names.push_back("platformer/tileGreen_08.png");
            names.push_back("platformer/tileYellow_09.png");
            names.push_back("platformer/tileBrown_09.png");
        } else if (type == ENEMY1) {
            names.push_back("platformer/enemySwimming_1.png");
        } else if (type == ENEMY2) {
            names.push_back("platformer/enemySwimming_2.png");
        } else if (type == COIN) {
            names.push_back("platformer/yellowCrystal.png");
        }
    }

    void handle_agent_collision(const std::shared_ptr<Entity> &obj) override {
        BasicAbstractGame::handle_agent_collision(obj);

        if (obj->type == ENEMY) {
            step_data.done = true;
        } else if (obj->type == COIN) {
            step_data.reward += COIN_REWARD;
            coins_collected += 1;
            obj->will_erase = true;
        }
    }

    int theme_for_grid_obj(int type) override {
        if (is_wall(type))
            return wall_theme;

        return 0;
    }

    bool will_reflect(int src, int target) override {
        return BasicAbstractGame::will_reflect(src, target) || (src == ENEMY && (is_wall(target) || target == ENEMY_BARRIER));
    }

    void update_agent_velocity() override {
        float mixrate_x = has_support ? mixrate : (mixrate * air_control);
        agent->vx = (1 - mixrate_x) * agent->vx + mixrate_x * maxspeed * action_vx;
        if (action_vy > 0)
            agent->vy = max_jump;

        if (!has_support) {
            if (agent->vy > -2) {
                agent->vy -= gravity;
            }
        }
    }

    bool is_wall(int type) {
        return type == WALL_MID || type == WALL_TOP;
    }

    bool use_block_asset(int type) override {
        return BasicAbstractGame::use_block_asset(type) || is_wall(type);
    }

    bool is_blocked(const std::shared_ptr<Entity> &src, int target, bool is_horizontal) override {
        if (BasicAbstractGame::is_blocked(src, target, is_horizontal))
            return true;
        if (src->type == PLAYER && is_wall(target))
            return true;

        return false;
    }

    int image_for_type(int type) override {
        if (type == PLAYER) {
            if (!has_support) {
                return PLAYER_JUMP;
            } else if (fabs(agent->vx) < .01 && action_vx == 0 && has_support) {
                return PLAYER;
            } else {
                return (cur_time / 5 % 2 == 0 || !has_support) ? PLAYER_RIGHT1 : PLAYER_RIGHT2;
            }
        } else if (type == ENEMY_BARRIER) {
            return -1;
        }

        return BasicAbstractGame::image_for_type(type);
    }

    void init_floor_and_walls() {
        fill_elem(0, 0, main_width, 1, WALL_TOP);
        fill_elem(0, 0, 1, main_height, WALL_MID);
        fill_elem(main_width - 1, 0, 1, main_height, WALL_MID);
        fill_elem(0, main_height - 1, main_width, 1, WALL_MID);
    }

    int choose_delta_y() {
        int max_dy = max_jump * max_jump / (2 * gravity);
        int min_dy = 3;

        return rand_gen.randn(max_dy - min_dy + 1) + min_dy;
    }

    void generate_platforms() {
        int difficulty = rand_gen.randn(3);
        int min_platforms = difficulty * difficulty + 1;
        int max_platforms = (difficulty + 1) * (difficulty + 1) + 1;
        int num_platforms = rand_gen.randn(max_platforms - min_platforms + 1) + min_platforms;

        coin_quota = 0;
        coins_collected = 0;

        int curr_x = rand_gen.randn(main_width - 4) + 2;
        int curr_y = 0;

        int margin_x = 3;
        float enemy_prob = options.distribution_mode == EasyMode ? .2 : .5;

        for (int i = 0; i < num_platforms; i++) {
            int delta_y = choose_delta_y();

            // only spawn enemies that won't be trapped in tight spaces
            bool can_spawn_enemy = (curr_x >= margin_x) && (curr_x <= main_width - margin_x);

            if (can_spawn_enemy && (rand_gen.rand01() < enemy_prob)) {
                auto ent = add_entity(curr_x + .5, curr_y + rand_gen.randn(2) + 2 + .5, .15 * (rand_gen.randn(2) * 2 - 1), 0, .5, ENEMY);
                ent->image_type = ENEMY1;
                ent->smart_step = true;
                ent->climber_spawn_x = curr_x + .5;
                match_aspect_ratio(ent);
            }

            curr_y += delta_y;
            int plat_len = 2 + rand_gen.randn(10);

            int vx = rand_gen.randn(2) * 2 - 1;
            if (curr_x < margin_x)
                vx = 1;
            if (curr_x > main_width - margin_x)
                vx = -1;

            std::vector<int> candidates;

            for (int j = 0; j < plat_len; j++) {
                int nx = curr_x + (j + 1) * vx;
                if (nx <= 0 || nx >= main_width - 1)
                    break;
                candidates.push_back(nx);
                set_obj(nx, curr_y, WALL_TOP);
            }

            if (rand_gen.rand01() < .5 || i == num_platforms - 1) {
                int coin_x = rand_gen.choose_one(candidates);
                add_entity(coin_x + .5, curr_y + 1.5, 0, 0, 0.3f, COIN);
                coin_quota += 1;
            }

            int next_x = rand_gen.choose_one(candidates);
            curr_x = next_x;
        }
    }

    void choose_world_dim() override {
        main_width = options.distribution_mode == EasyMode ? 16 : 20;
        main_height = 64;
    }

    void game_reset() override {
        BasicAbstractGame::game_reset();

        gravity = 0.2f;
        max_jump = 1.5;
        air_control = 0.15f;
        maxspeed = .5;
        has_support = false;
        facing_right = true;

        agent->rx = .5;
        agent->ry = .5;

        agent->x = 1 + agent->rx;
        agent->y = 1 + agent->ry;
        choose_random_theme(agent);
        wall_theme = rand_gen.randn(NUM_WALL_THEMES);

        init_floor_and_walls();
        generate_platforms();
    }

    bool can_support(int obj) {
        return is_wall(obj) || obj == out_of_bounds_object;
    }

    void choose_center(float &cx, float &cy) override {
        cx = main_width / 2.0;
        cy = agent->y + main_width / 2.0 - 5 * agent->ry;
        visibility = main_width;
    }

    void set_action_xy(int move_action) override {
        action_vx = move_action / 3 - 1;
        action_vy = (move_action % 3) - 1;
        if (action_vy < 0)
            action_vy = 0;

        if (action_vx > 0)
            facing_right = true;
        if (action_vx < 0)
            facing_right = false;

        int obj_below_1 = get_obj_from_floats(agent->x - (agent->rx - .01), agent->y - (agent->ry + .01));
        int obj_below_2 = get_obj_from_floats(agent->x + (agent->rx - .01), agent->y - (agent->ry + .01));

        has_support = can_support(obj_below_1) || can_support(obj_below_2);

        if (has_support && action_vy == 1) {
            action_vy = 1;
        } else {
            action_vy = 0;
        }
    }

    void game_step() override {
        BasicAbstractGame::game_step();

        if (action_vx > 0)
            agent->is_reflected = false;
        if (action_vx < 0)
            agent->is_reflected = true;

        for (int i = (int)(entities.size()) - 1; i >= 0; i--) {
            auto ent = entities[i];

            if (ent->type == ENEMY) {
                if (ent->x > ent->climber_spawn_x + PATROL_RANGE) {
                    ent->vx = -1 * fabs(ent->vx);
                } else if (ent->x < ent->climber_spawn_x - PATROL_RANGE) {
                    ent->vx = fabs(ent->vx);
                }

                ent->image_type = cur_time / 5 % 2 == 0 ? ENEMY1 : ENEMY2;
                ent->is_reflected = ent->vx < 0;
            }
        }

        if (coin_quota == coins_collected) {
            step_data.done = true;
            step_data.reward += COMPLETION_BONUS;
            step_data.level_complete = true;
        }
    }

    void serialize(WriteBuffer *b) override {
        BasicAbstractGame::serialize(b);
        b->write_bool(has_support);
        b->write_bool(facing_right);
        b->write_int(coin_quota);
        b->write_int(coins_collected);
        b->write_int(wall_theme);
        b->write_float(gravity);
        b->write_float(air_control);
    }

    void deserialize(ReadBuffer *b) override {
        BasicAbstractGame::deserialize(b);
        has_support = b->read_bool();
        facing_right = b->read_bool();
        coin_quota = b->read_int();
        coins_collected = b->read_int();
        wall_theme = b->read_int();
        gravity = b->read_float();
        air_control = b->read_float();
    }
};

REGISTER_GAME(NAME, Climber);
