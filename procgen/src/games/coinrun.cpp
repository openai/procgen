#include "../basic-abstract-game.h"
#include "../assetgen.h"
#include <set>
#include <queue>
#include "../mazegen.h"
#include "../cpp-utils.h"
#include "../qt-utils.h"

const std::string NAME = "coinrun";

const float GOAL_REWARD = 10.0f;

const int GOAL = 1;
const int SAW = 2;
const int SAW2 = 3;
const int ENEMY = 5;
const int ENEMY1 = 6;
const int ENEMY2 = 7;

const int PLAYER_JUMP = 9;
const int PLAYER_RIGHT1 = 12;
const int PLAYER_RIGHT2 = 13;

const int WALL_MID = 15;
const int WALL_TOP = 16;
const int LAVA_MID = 17;
const int LAVA_TOP = 18;
const int ENEMY_BARRIER = 19;

const int CRATE = 20;

std::vector<std::string> WALKING_ENEMIES = {"slimeBlock", "slimePurple", "slimeBlue", "slimeGreen", "mouse", "snail", "ladybug", "wormGreen", "wormPink"};
std::vector<std::string> PLAYER_THEME_COLORS = {"Beige", "Blue", "Green", "Pink", "Yellow"};
std::vector<std::string> GROUND_THEMES = {"Dirt", "Grass", "Planet", "Sand", "Snow", "Stone"};

const int NUM_GROUND_THEMES = (int)(GROUND_THEMES.size());

class CoinRun : public BasicAbstractGame {
  public:
    std::shared_ptr<Entity> goal;
    float last_agent_y = 0.0f;
    int wall_theme = 0;
    bool has_support = false;
    bool facing_right = false;
    bool is_on_crate = false;
    float gravity = 0.0f;
    float air_control = 0.0f;

    CoinRun()
        : BasicAbstractGame(NAME) {
        visibility = 13;
        mixrate = 0.2f;

        main_width = 64;
        main_height = 64;

        out_of_bounds_object = WALL_MID;
    }

    void load_background_images() override {
        main_bg_images_ptr = &platform_backgrounds;
    }

    QRectF get_adjusted_image_rect(int type, const QRectF &rect) override {
        if (type == PLAYER || type == PLAYER_JUMP || type == PLAYER_RIGHT1 || type == PLAYER_RIGHT2) {
            return adjust_rect(rect, QRectF(0, -.7415, 1, 1.7415));
        }

        return BasicAbstractGame::get_adjusted_image_rect(type, rect);
    }

    void asset_for_type(int type, std::vector<std::string> &names) override {
        if (type == PLAYER) {
            for (const auto &color : PLAYER_THEME_COLORS) {
                names.push_back("kenney/Players/128x256/" + color + "/alien" + color + "_stand.png");
            }
        } else if (type == PLAYER_JUMP) {
            for (const auto &color : PLAYER_THEME_COLORS) {
                names.push_back("kenney/Players/128x256/" + color + "/alien" + color + "_jump.png");
            }
        } else if (type == PLAYER_RIGHT1) {
            for (const auto &color : PLAYER_THEME_COLORS) {
                names.push_back("kenney/Players/128x256/" + color + "/alien" + color + "_walk1.png");
            }
        } else if (type == PLAYER_RIGHT2) {
            for (const auto &color : PLAYER_THEME_COLORS) {
                names.push_back("kenney/Players/128x256/" + color + "/alien" + color + "_walk2.png");
            }
        } else if (type == ENEMY1) {
            for (const auto &enemy : WALKING_ENEMIES) {
                names.push_back("kenney/Enemies/" + enemy + ".png");
            }
        } else if (type == ENEMY2) {
            for (const auto &enemy : WALKING_ENEMIES) {
                names.push_back("kenney/Enemies/" + enemy + "_move.png");
            }
        } else if (type == GOAL) {
            names.push_back("kenney/Items/coinGold.png");
        } else if (type == WALL_TOP) {
            for (const auto &ground : GROUND_THEMES) {
                names.push_back("kenney/Ground/" + ground + "/" + to_lower(ground) + "Mid.png");
            }
        } else if (type == WALL_MID) {
            for (const auto &ground : GROUND_THEMES) {
                names.push_back("kenney/Ground/" + ground + "/" + to_lower(ground) + "Center.png");
            }
        } else if (type == LAVA_TOP) {
            names.push_back("kenney/Tiles/lavaTop_low.png");
        } else if (type == LAVA_MID) {
            names.push_back("kenney/Tiles/lava.png");
        } else if (type == SAW) {
            names.push_back("kenney/Enemies/sawHalf.png");
        } else if (type == SAW2) {
            names.push_back("kenney/Enemies/sawHalf_move.png");
        } else if (type == CRATE) {
            names.push_back("kenney/Tiles/boxCrate.png");
            names.push_back("kenney/Tiles/boxCrate_double.png");
            names.push_back("kenney/Tiles/boxCrate_single.png");
            names.push_back("kenney/Tiles/boxCrate_warning.png");
        }
    }

    void handle_agent_collision(const std::shared_ptr<Entity> &obj) override {
        BasicAbstractGame::handle_agent_collision(obj);

        if (obj->type == ENEMY) {
            step_data.done = true;
        } else if (obj->type == SAW) {
            step_data.done = true;
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

    void handle_grid_collision(const std::shared_ptr<Entity> &obj, int type, int i, int j) override {
        if (obj->type == PLAYER) {
            if (type == GOAL) {
                step_data.reward += GOAL_REWARD;
                step_data.done = true;
                step_data.level_complete = true;
            } else if (is_lava(type)) {
                step_data.done = true;
            }
        }
    }

    void update_agent_velocity() override {
        float mixrate_x = has_support ? mixrate : (mixrate * air_control);
        agent->vx = (1 - mixrate_x) * agent->vx + mixrate_x * maxspeed * action_vx;
        if (fabs(agent->vx) < mixrate_x * maxspeed)
            agent->vx = 0;
        if (action_vy > 0) {
            agent->vy = max_jump;
        } else {
            if (has_support) {
                agent->vy += .2 * action_vy;
            }
        }

        if (!(has_support && action_vy > 0)) {
            agent->vy -= gravity;
            agent->vy = clip_abs(agent->vy, max_jump);
        }
    }

    bool is_wall(int type) {
        return type == WALL_MID || type == WALL_TOP;
    }

    bool is_lava(int type) {
        return type == LAVA_MID || type == LAVA_TOP;
    }

    bool use_block_asset(int type) override {
        return BasicAbstractGame::use_block_asset(type) || is_wall(type);
    }

    bool is_blocked_ents(const std::shared_ptr<Entity> &src, const std::shared_ptr<Entity> &target, bool is_horizontal) override {
        if (target->type == CRATE && !is_horizontal) {
            if (agent->vy >= 0)
                return false;
            if (action_vy < 0)
                return false;
            if (last_agent_y < (target->y + target->ry + agent->ry))
                return false;

            is_on_crate = true;

            return true;
        }

        return BasicAbstractGame::is_blocked_ents(src, target, is_horizontal);
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
            if (fabs(agent->vx) < .01 && action_vx == 0 && has_support) {
                return PLAYER;
            } else {
                return (cur_time / 5 % 2 == 0 || !has_support) ? PLAYER_RIGHT1 : PLAYER_RIGHT2;
            }
        } else if (type == ENEMY_BARRIER) {
            return -1;
        }

        return BasicAbstractGame::image_for_type(type);
    }

    void fill_block_top(int x, int y, int dx, int dy, char fill, char top) {
        fassert(dy > 0);
        fill_elem(x, y, dx, dy - 1, fill);
        fill_elem(x, y + dy - 1, dx, 1, top);
    }

    void fill_ground_block(int x, int y, int dx, int dy) {
        fill_block_top(x, y, dx, dy, WALL_MID, WALL_TOP);
    }

    void fill_lava_block(int x, int y, int dx, int dy) {
        fill_block_top(x, y, dx, dy, LAVA_MID, LAVA_TOP);
    }

    void init_floor_and_walls() {
        fill_elem(0, 0, main_width, 1, WALL_TOP);
        fill_elem(0, 0, 1, main_height, WALL_MID);
        fill_elem(main_width - 1, 0, 1, main_height, WALL_MID);
        fill_elem(0, main_height - 1, main_width, 1, WALL_MID);
    }

    void create_saw_enemy(int x, int y) {
        add_entity(x + .5, y + .5, 0, 0, .5, SAW);
    }

    void create_enemy(int x, int y) {
        auto ent = add_entity(x + .5, y + .5, .15 * (rand_gen.randn(2) * 2 - 1), 0, .5, ENEMY);
        ent->smart_step = true;
        ent->image_type = ENEMY1;
        ent->render_z = 1;
        choose_random_theme(ent);
    }

    void create_crate(int x, int y) {
        auto ent = add_entity(x + .5, y + .5, 0, 0, .5, CRATE);
        choose_random_theme(ent);
    }

    void generate_coin_to_the_right() {
        int max_difficulty = 3;
        int dif = rand_gen.randn(max_difficulty) + 1;

        int num_sections = rand_gen.randn(dif) + dif;
        int curr_x = 5;
        int curr_y = 1;

        int pit_threshold = dif;
        int danger_type = rand_gen.randn(3);

        bool allow_pit = (options.debug_mode & (1 << 1)) == 0;
        bool allow_crate = (options.debug_mode & (1 << 2)) == 0;
        bool allow_dy = (options.debug_mode & (1 << 3)) == 0;

        int w = main_width;

        float _max_dy = max_jump * max_jump / (2 * gravity);
        float _max_dx = maxspeed * 2 * max_jump / gravity;

        int max_dy = (_max_dy - .5);
        int max_dx = (_max_dx - .5);

        bool allow_monsters = true;

        if (options.distribution_mode == EasyMode) {
            allow_monsters = false;
        }

        for (int section_idx = 0; section_idx < num_sections; section_idx++) {
            if (curr_x + 15 >= w) {
                break;
            }

            int dy = rand_gen.randn(4) + 1 + int(dif / 3);

            if (!allow_dy) {
                dy = 0;
            }

            if (dy > max_dy) {
                dy = max_dy;
            }

            if (curr_y >= 20) {
                dy *= -1;
            } else if (curr_y >= 5 && rand_gen.randn(2) == 1) {
                dy *= -1;
            }

            int dx = rand_gen.randn(2 * dif) + 3 + int(dif / 3);

            curr_y += dy;

            if (curr_y < 1) {
                curr_y = 1;
            }

            bool use_pit = allow_pit && (dx > 7) && (curr_y > 3) && (rand_gen.randn(20) >= pit_threshold);

            if (use_pit) {
                int x1 = rand_gen.randn(3) + 1;
                int x2 = rand_gen.randn(3) + 1;
                int pit_width = dx - x1 - x2;

                if (pit_width > max_dx) {
                    pit_width = max_dx;
                    x2 = dx - x1 - pit_width;
                }

                fill_ground_block(curr_x, 0, x1, curr_y);
                fill_ground_block(curr_x + dx - x2, 0, x2, curr_y);

                int lava_height = rand_gen.randn(curr_y - 3) + 1;

                if (danger_type == 0) {
                    fill_lava_block(curr_x + x1, 1, pit_width, lava_height);
                } else if (danger_type == 1) {
                    for (int ei = 0; ei < pit_width; ei++) {
                        create_saw_enemy(curr_x + x1 + ei, 1);
                    }
                } else if (danger_type == 2) {
                    for (int ei = 0; ei < pit_width; ei++) {
                        create_enemy(curr_x + x1 + ei, 1);
                    }
                }

                if (pit_width > 4) {
                    int x3, w1;
                    if (pit_width == 5) {
                        x3 = 1 + rand_gen.randn(2);
                        w1 = 1 + rand_gen.randn(2);
                    } else if (pit_width == 6) {
                        x3 = 2 + rand_gen.randn(2);
                        w1 = 1 + rand_gen.randn(2);
                    } else {
                        x3 = 2 + rand_gen.randn(2);
                        int x4 = 2 + rand_gen.randn(2);
                        w1 = pit_width - x3 - x4;
                    }

                    fill_ground_block(curr_x + x1 + x3, curr_y - 1, w1, 1);
                }

            } else {
                fill_ground_block(curr_x, 0, dx, curr_y);

                int ob1_x = -1;
                int ob2_x = -1;

                if (rand_gen.randn(10) < (2 * dif) && dx > 3) {
                    ob1_x = curr_x + rand_gen.randn(dx - 2) + 1;
                    create_saw_enemy(ob1_x, curr_y);
                }

                if (rand_gen.randn(10) < dif && dx > 3 && (max_dx >= 4) && allow_monsters) {
                    ob2_x = curr_x + rand_gen.randn(dx - 2) + 1;

                    create_enemy(ob2_x, curr_y);
                }

                if (allow_crate) {
                    for (int i = 0; i < 2; i++) {
                        int crate_x = curr_x + rand_gen.randn(dx - 2) + 1;

                        if (rand_gen.randn(2) == 1 && ob1_x != crate_x && ob2_x != crate_x) {
                            int pile_height = rand_gen.randn(3) + 1;

                            for (int j = 0; j < pile_height; j++) {
                                create_crate(crate_x, curr_y + j);
                            }
                        }
                    }
                }
            }

            if (!is_wall(get_obj(curr_x - 1, curr_y))) {
                set_obj(curr_x - 1, curr_y, ENEMY_BARRIER);
            }

            curr_x += dx;

            set_obj(curr_x, curr_y, ENEMY_BARRIER);
        }

        set_obj(curr_x, curr_y, GOAL);

        fill_ground_block(curr_x, 0, 1, curr_y);
        fill_elem(curr_x + 1, 0, main_width - curr_x - 1, main_height, WALL_MID);
    }

    void game_reset() override {
        BasicAbstractGame::game_reset();

        gravity = 0.2f;
        max_jump = 1.5;
        air_control = 0.15f;
        maxspeed = .5;
        has_support = false;
        facing_right = true;

        if (options.distribution_mode == EasyMode) {
            agent->image_theme = 0;
            wall_theme = 0;
            background_index = 0;
        } else {
            choose_random_theme(agent);
            wall_theme = rand_gen.randn(NUM_GROUND_THEMES);
        }

        agent->rx = .5;
        agent->ry = 0.5787f;

        agent->x = 1 + agent->rx;
        agent->y = 1 + agent->ry;
        last_agent_y = agent->y;
        is_on_crate = false;

        init_floor_and_walls();
        generate_coin_to_the_right();
    }

    bool can_support(int obj) {
        return is_wall(obj) || obj == out_of_bounds_object;
    }

    void set_action_xy(int move_action) override {
        action_vx = move_action / 3 - 1;
        action_vy = (move_action % 3) - 1;

        if (action_vx > 0)
            facing_right = true;
        if (action_vx < 0)
            facing_right = false;

        int obj_below_1 = get_obj_from_floats(agent->x - (agent->rx - .01), agent->y - (agent->ry + .01));
        int obj_below_2 = get_obj_from_floats(agent->x + (agent->rx - .01), agent->y - (agent->ry + .01));

        has_support = (is_on_crate || can_support(obj_below_1) || can_support(obj_below_2)) && agent->vy == 0;

        is_on_crate = false;

        if (action_vy == 1) {
            if (!has_support) {
                action_vy = 0;
            }
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
                auto trail = add_entity_rxy(ent->x, ent->y - ent->ry * .5, 0, 0.01f, 0.3f, 0.2f, TRAIL);
                trail->expire_time = 8;
                trail->alpha = .5;

                ent->image_type = cur_time / 5 % 2 == 0 ? ENEMY1 : ENEMY2;
                ent->is_reflected = ent->vx > 0;
            } else if (ent->type == SAW) {
                ent->image_type = cur_time % 2 == 0 ? SAW : SAW2;
            }
        }

        last_agent_y = agent->y;
    }

    void serialize(WriteBuffer *b) override {
        BasicAbstractGame::serialize(b);
        b->write_float(last_agent_y);
        b->write_int(wall_theme);
        b->write_bool(has_support);
        b->write_bool(facing_right);
        b->write_bool(is_on_crate);
        b->write_float(gravity);
        b->write_float(air_control);
    }

    void deserialize(ReadBuffer *b) override {
        BasicAbstractGame::deserialize(b);
        last_agent_y = b->read_float();
        wall_theme = b->read_int();
        has_support = b->read_bool();
        facing_right = b->read_bool();
        is_on_crate = b->read_bool();
        gravity = b->read_float();
        air_control = b->read_float();
    }
};

REGISTER_GAME(NAME, CoinRun);
