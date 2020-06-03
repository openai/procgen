#include "../basic-abstract-game.h"
#include "../assetgen.h"
#include <set>
#include <queue>
#include "../mazegen.h"

const std::string NAME = "ninja";

const float GOAL_REWARD = 10.0f;

const int GOAL = 1;
const int BOMB = 6;
const int THROWING_STAR = 7;

const int PLAYER_JUMP = 9;
const int PLAYER_RIGHT1 = 12;
const int PLAYER_RIGHT2 = 13;
const int FIRE = 14;

const int WALL_MID = 20;
const int NUM_WALL_THEMES = 3;

class Ninja : public BasicAbstractGame {
  public:
    bool has_support = false;
    bool facing_right = false;
    int last_fire_time = 0;
    int wall_theme = 0;
    float gravity = 0.0f;
    float air_control = 0.0f;
    float jump_charge = 0.0f;
    float jump_charge_inc = 0.0f;

    Ninja()
        : BasicAbstractGame(NAME) {
        main_width = 64;
        main_height = 64;

        out_of_bounds_object = WALL_MID;
    }

    void load_background_images() override {
        main_bg_images_ptr = &platform_backgrounds;
    }

    void asset_for_type(int type, std::vector<std::string> &names) override {
        std::shared_ptr<QImage> asset_ptr = nullptr;

        if (type == WALL_MID) {
            names.push_back("misc_assets/tile_bricksGrey.png");
            names.push_back("misc_assets/tile_bricksGrown.png");
            names.push_back("misc_assets/tile_bricksRed.png");
        } else if (type == GOAL) {
            names.push_back("platformer/shroom1.png");
            names.push_back("platformer/shroom2.png");
            names.push_back("platformer/shroom3.png");
            names.push_back("platformer/shroom4.png");
            names.push_back("platformer/shroom5.png");
            names.push_back("platformer/shroom6.png");
        } else if (type == PLAYER) {
            names.push_back("platformer/zombie_idle.png");
        } else if (type == PLAYER_JUMP) {
            names.push_back("platformer/zombie_jump.png");
        } else if (type == PLAYER_RIGHT1) {
            names.push_back("platformer/zombie_walk1.png");
        } else if (type == PLAYER_RIGHT2) {
            names.push_back("platformer/zombie_walk2.png");
        } else if (type == BOMB) {
            names.push_back("misc_assets/bomb.png");
        } else if (type == THROWING_STAR) {
            names.push_back("misc_assets/saw.png");
        } else if (type == FIRE) {
            names.push_back("misc_assets/bomb.png");
        }
    }

    void handle_agent_collision(const std::shared_ptr<Entity> &obj) override {
        BasicAbstractGame::handle_agent_collision(obj);

        if (obj->type == EXPLOSION) {
            step_data.done = true;
        } else if (obj->type == GOAL) {
            step_data.reward += GOAL_REWARD;
            step_data.level_complete = true;
            step_data.done = true;
        }
    }

    void handle_grid_collision(const std::shared_ptr<Entity> &obj, int type, int i, int j) override {
        if (obj->type == PLAYER) {
            if (type == FIRE) {
                step_data.done = true;
            } else if (type == BOMB) {
                step_data.done = true;
            }
        } else if (obj->type == THROWING_STAR) {
            if (type == BOMB) {
                obj->will_erase = true;
                set_obj(i, j, SPACE);
                add_entity(i + .5, j + .5, 0, 0, .5, EXPLOSION);
            }
            if (is_wall(type)) {
                obj->will_erase = true;
            }
        }
    }

    void update_agent_velocity() override {
        float mixrate_x = has_support ? mixrate : (mixrate * air_control);
        agent->vx = (1 - mixrate_x) * agent->vx + mixrate_x * maxspeed * action_vx;

        if (action_vy < 1 && jump_charge > 0) {
            agent->vy = jump_charge * max_jump;
            jump_charge = 0;
        }

        if (!has_support) {
            if (agent->vy > -2) {
                agent->vy -= gravity;
            }
        }
    }

    bool is_wall(int type) {
        return type == WALL_MID;
    }

    int theme_for_grid_obj(int type) override {
        if (is_wall(type))
            return wall_theme;

        return 0;
    }

    bool use_block_asset(int type) override {
        return BasicAbstractGame::use_block_asset(type) || is_wall(type);
    }

    bool is_blocked(const std::shared_ptr<Entity> &src, int target, bool is_horizontal) override {
        if (is_wall(target)) {
            if (src->type == PLAYER) {
                return true;
            } else if (src->type == THROWING_STAR) {
                // throwing stars stick to walls
                src->vx = 0;
                src->vy = 0;
                return true;
            }
        }

        return BasicAbstractGame::is_blocked(src, target, is_horizontal);
    }

    int image_for_type(int type) override {
        if (type == PLAYER) {
            if (fabs(agent->vx) < .01 && action_vx == 0 && has_support) {
                return PLAYER;
            } else {
                return (cur_time / 5 % 2 == 0 || !has_support) ? PLAYER_RIGHT1 : PLAYER_RIGHT2;
            }
        }

        return BasicAbstractGame::image_for_type(type);
    }

    void game_draw(QPainter &p, const QRect &rect) override {
        BasicAbstractGame::game_draw(p, rect);

        QColor charge_color = QColor(66, 245, 135);

        float bar_height = 3 * jump_charge;

        QRectF dist_rect2 = get_abs_rect(.25, visibility - .5 - bar_height, .5, bar_height);
        p.fillRect(dist_rect2, charge_color);
    }

    void fill_block_top(int x, int y, int dx, int dy, char fill, char top) {
        if (dy <= 0)
            return;
        fill_elem(x, y, dx, dy - 1, fill);
        fill_elem(x, y + dy - 1, dx, 1, top);
    }

    void fill_ground_block(int x, int y, int dx, int dy) {
        fill_block_top(x, y, dx, dy, WALL_MID, WALL_MID);
    }

    void init_floor_and_walls() {
        fill_elem(0, 0, main_width, 1, WALL_MID);
        fill_elem(0, 0, 1, main_height, WALL_MID);
        fill_elem(main_width - 1, 0, 1, main_height, WALL_MID);
        fill_elem(0, main_height - 1, main_width, 1, WALL_MID);
    }

    void generate_coin_to_the_right(int difficulty) {
        int min_gap = difficulty - 1;
        int min_plat_w = 1;
        int inc_dy = 4;

        if (options.distribution_mode == EasyMode) {
            min_gap -= 1;
            if (min_gap < 0)
                min_gap = 0;
            min_plat_w = 3;
            inc_dy = 2;
        }

        float bomb_prob = .25 * (difficulty - 1);
        int max_gap_inc = difficulty == 1 ? 1 : 2;

        int num_sections = rand_gen.randn(difficulty) + difficulty;
        int start_x = 5;
        int curr_x = start_x;
        int curr_y = main_height / 2;
        int min_y = curr_y;

        int w = main_width;

        float _max_dy = max_jump * max_jump / (2 * gravity);

        int max_dy = (_max_dy - .5);

        int prev_x;
        int prev_y;

        fill_ground_block(0, 0, start_x, curr_y);
        fill_elem(0, curr_y + 8, start_x, main_height - curr_y - 8, WALL_MID);

        for (int i = 0; i < num_sections; i++) {
            prev_x = curr_x;
            prev_y = curr_y;
            int num_edges = rand_gen.randn(2) + 1;
            int max_y = -1;
            int last_edge_y = -1;

            for (int j = 0; j < num_edges; j++) {
                curr_x = prev_x + j;

                if (curr_x + 15 >= w) {
                    break;
                }

                curr_y = prev_y;

                int dy = rand_gen.randn(inc_dy) + 1 + int(difficulty / 3);

                if (dy > max_dy) {
                    dy = max_dy;
                }

                if (curr_y >= main_height - 15) {
                    dy *= -1;
                } else if (curr_y >= 5 && rand_gen.rand01() < .4) {
                    dy *= -1;
                }

                curr_y += dy;

                if (curr_y < 3) {
                    curr_y = 3;
                }

                if (abs(curr_y - last_edge_y) <= 1) {
                    curr_y = last_edge_y + 2;
                }

                int dx = min_plat_w + rand_gen.randn(3);

                fill_ground_block(curr_x, curr_y - 1, dx, 1);

                curr_x += dx;
                curr_x += min_gap + rand_gen.randn(max_gap_inc + 1);

                if (curr_y > max_y)
                    max_y = curr_y;
                if (curr_y < min_y)
                    min_y = curr_y;

                last_edge_y = curr_y;
            }

            if (rand_gen.rand01() < bomb_prob) {
                set_obj(rand_gen.randn(curr_x - prev_x + 1) + prev_x, max_y + 2, BOMB);
            }

            int ceiling_height = 11;
            int ceiling_start = max_y - 1 + ceiling_height;

            fill_ground_block(prev_x, ceiling_start, curr_x - prev_x, main_height - ceiling_start);
        }

        auto ent = add_entity(curr_x + .5, curr_y + .5, 0, 0, .5, GOAL);
        choose_random_theme(ent);

        fill_ground_block(curr_x, curr_y - 1, 1, 1);
        fill_elem(curr_x, curr_y + 6, 1, main_height - curr_y - 6, WALL_MID);

        int fire_y = min_y - 2;
        if (fire_y < 1)
            fire_y = 1;

        fill_ground_block(start_x, 0, main_width - start_x, fire_y);
        fill_elem(start_x, fire_y, main_width - start_x, 1, FIRE);
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
        jump_charge = 0;
        jump_charge_inc = .25;
        visibility = 16;

        agent->rx = .5;
        agent->ry = .5;

        agent->x = 1 + agent->rx;
        agent->y = main_height / 2 + agent->ry;

        if (options.distribution_mode == EasyMode) {
            max_jump = 1.25;
            jump_charge_inc = 1;
            visibility = 10;
        }

        int max_difficulty = 3;
        int difficulty = rand_gen.randn(max_difficulty) + 1;

        last_fire_time = 0;

        wall_theme = rand_gen.randn(NUM_WALL_THEMES);

        init_floor_and_walls();
        generate_coin_to_the_right(difficulty);
    }

    bool can_support(int obj) {
        return is_wall(obj) || obj == out_of_bounds_object;
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
            jump_charge += jump_charge_inc;

            if (jump_charge > 1) {
                jump_charge = 1;
            }
        } else {
            action_vy = 0;
        }

        if (!has_support) {
            jump_charge = 0;
        }
    }

    void game_step() override {
        BasicAbstractGame::game_step();

        if (action_vx > 0)
            agent->is_reflected = false;
        if (action_vx < 0)
            agent->is_reflected = true;

        if (special_action > 0 && (cur_time - last_fire_time) >= 3) {
            float theta = 0;
            float bullet_vel = 1;

            if (special_action == 1) {
                theta = 0;
            } else if (special_action == 2) {
                theta = PI / 4;
            } else if (special_action == 3) {
                theta = PI / 2;
            } else if (special_action == 4) {
                theta = -1 * PI / 4;
            }

            if (agent->is_reflected) {
                theta = PI - theta;
            }

            auto new_bullet = add_entity(agent->x, agent->y, bullet_vel * cos(theta), bullet_vel * sin(theta), .25, THROWING_STAR);
            new_bullet->collides_with_entities = true;
            new_bullet->expire_time = 15;
            new_bullet->smart_step = true;
            last_fire_time = cur_time;
        }
    }

    void serialize(WriteBuffer *b) override {
        BasicAbstractGame::serialize(b);
        b->write_bool(has_support);
        b->write_bool(facing_right);
        b->write_int(last_fire_time);
        b->write_int(wall_theme);
        b->write_float(gravity);
        b->write_float(air_control);
        b->write_float(jump_charge);
        b->write_float(jump_charge_inc);
    }

    void deserialize(ReadBuffer *b) override {
        BasicAbstractGame::deserialize(b);
        has_support = b->read_bool();
        facing_right = b->read_bool();
        last_fire_time = b->read_int();
        wall_theme = b->read_int();
        gravity = b->read_float();
        air_control = b->read_float();
        jump_charge = b->read_float();
        jump_charge_inc = b->read_float();
    }
};

REGISTER_GAME(NAME, Ninja);
