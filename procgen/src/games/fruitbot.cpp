#include "../basic-abstract-game.h"
#include "../assetgen.h"
#include <set>
#include <queue>

const std::string NAME = "fruitbot";

const float COMPLETION_BONUS = 10.0;
const int POSITIVE_REWARD = 1.0f;
const int PENALTY = -4.0f;

const int BARRIER = 1;
const int OUT_OF_BOUNDS_WALL = 2;
const int PLAYER_BULLET = 3;
const int BAD_OBJ = 4;
const int GOOD_OBJ = 7;
const int LOCKED_DOOR = 10;
const int LOCK = 11;
const int PRESENT = 12;

const int KEY_DURATION = 8;

const float DOOR_ASPECT_RATIO = 3.25;

class FruitBotGame : public BasicAbstractGame {
  public:
    float min_dim = 0.0f;
    float bullet_vscale = 0.0f;
    int last_fire_time = 0;

    FruitBotGame()
        : BasicAbstractGame(NAME) {
        mixrate = .5;
        maxspeed = 0.85f;

        min_dim = 5;
        bullet_vscale = .5;
        bg_tile_ratio = -1;

        out_of_bounds_object = OUT_OF_BOUNDS_WALL;
    }

    void load_background_images() override {
        main_bg_images_ptr = &topdown_backgrounds;
    }

    void asset_for_type(int type, std::vector<std::string> &names) override {
        if (type == PLAYER) {
            names.push_back("misc_assets/robot_3Dblue.png");
        } else if (type == BARRIER || type == OUT_OF_BOUNDS_WALL) {
            names.push_back("misc_assets/tileStone_slope.png");
        } else if (type == PLAYER_BULLET) {
            names.push_back("misc_assets/keyRed2.png");
        } else if (type == BAD_OBJ) {
            names.push_back("misc_assets/food1.png");
            names.push_back("misc_assets/food2.png");
            names.push_back("misc_assets/food3.png");
            names.push_back("misc_assets/food4.png");
            names.push_back("misc_assets/food5.png");
            names.push_back("misc_assets/food6.png");
        } else if (type == GOOD_OBJ) {
            names.push_back("misc_assets/fruit1.png");
            names.push_back("misc_assets/fruit2.png");
            names.push_back("misc_assets/fruit3.png");
            names.push_back("misc_assets/fruit4.png");
            names.push_back("misc_assets/fruit5.png");
            names.push_back("misc_assets/fruit6.png");
        } else if (type == LOCKED_DOOR) {
            names.push_back("misc_assets/fenceYellow.png");
        } else if (type == LOCK) {
            names.push_back("misc_assets/lockRed2.png");
        } else if (type == PRESENT) {
            names.push_back("misc_assets/present1.png");
            names.push_back("misc_assets/present2.png");
            names.push_back("misc_assets/present3.png");
        }
    }

    bool will_reflect(int src, int target) override {
        return BasicAbstractGame::will_reflect(src, target) || (src == BAD_OBJ && (target == BARRIER || target == WALL_OBJ));
    }

    bool is_blocked(const std::shared_ptr<Entity> &src, int target, bool is_horizontal) override {
        return BasicAbstractGame::is_blocked(src, target, is_horizontal) || (src->type == PLAYER && target == OUT_OF_BOUNDS_WALL);
    }

    float get_tile_aspect_ratio(const std::shared_ptr<Entity> &ent) override {
        if (ent->type == BARRIER)
            return 1;
        if (ent->type == LOCKED_DOOR)
            return DOOR_ASPECT_RATIO;

        return 0;
    }

    void handle_agent_collision(const std::shared_ptr<Entity> &obj) override {
        BasicAbstractGame::handle_agent_collision(obj);

        if (obj->type == BARRIER) {
            step_data.done = true;
        } else if (obj->type == BAD_OBJ) {
            step_data.reward += PENALTY;
            obj->will_erase = true;
        } else if (obj->type == LOCKED_DOOR) {
            step_data.done = true;
        } else if (obj->type == GOOD_OBJ) {
            step_data.reward += POSITIVE_REWARD;
            obj->will_erase = true;
        } else if (obj->type == PRESENT) {
            if (!step_data.done) {
            }
            step_data.reward += COMPLETION_BONUS;
            step_data.done = true;
            step_data.level_complete = true;
        }
    }

    void handle_collision(const std::shared_ptr<Entity> &src, const std::shared_ptr<Entity> &target) override {
        if (src->type == PLAYER_BULLET) {
            if (target->type == BARRIER) {
                src->will_erase = true;
            } else if (target->type == LOCK) {
                src->will_erase = true;
                target->will_erase = true;

                // find and erase the corresponding door entity
                for (auto ent : entities) {
                    if (ent->type == LOCKED_DOOR && fabs(ent->y - target->y) < 1) {
                        ent->will_erase = true;
                        break;
                    }
                }
            }
        }
    }

    bool use_block_asset(int type) override {
        return BasicAbstractGame::use_block_asset(type) || (type == BARRIER) || (type == LOCKED_DOOR) || (type == PRESENT);
    }

    void choose_center(float &cx, float &cy) override {
        cx = main_width / 2.0;
        cy = agent->y + main_width / 2.0 - 2 * agent->ry;
        visibility = main_width;
    }

    void choose_world_dim() override {
        if (options.distribution_mode == EasyMode) {
            main_width = 10;
        } else {
            main_width = 20;
        }

        main_height = 60;
    }

    void set_action_xy(int move_action) override {
        action_vx = move_action / 3 - 1;
        action_vy = 0.2f; //(move_action % 3) * .2;
        action_vrot = 0;
    }

    void add_walls(float ry, bool use_door, float min_pct) {
        float rw = main_width;
        float wall_ry = 0.3f;
        float lock_rx = .25;
        float lock_ry = 0.45f;

        float pct = min_pct + .2 * rand_gen.rand01();

        if (use_door) {
            pct += 0.1f;
            float lock_pct_w = 2 * lock_rx / main_width;
            float door_pct_w = (wall_ry * 2 * DOOR_ASPECT_RATIO) / main_width;
            int num_doors = ceil((pct - 2 * lock_pct_w) / door_pct_w);
            pct = 2 * lock_pct_w + door_pct_w * num_doors;
        }

        float gapw = pct * rw;

        float w1 = rand_gen.rand01() * (rw - gapw);
        float w2 = rw - w1 - gapw;

        add_entity_rxy(w1 / 2, ry, 0, 0, w1 / 2, wall_ry, BARRIER);
        add_entity_rxy(rw - w2 / 2, ry, 0, 0, w2 / 2, wall_ry, BARRIER);

        if (use_door) {
            int is_on_right = rand_gen.randn(2);
            float lock_x = w1 + lock_rx + is_on_right * (gapw - 2 * lock_rx);
            float door_x = w1 + gapw / 2 - (is_on_right * 2 - 1) * lock_rx;

            add_entity_rxy(door_x, ry, 0, 0, gapw / 2 - lock_rx, wall_ry, LOCKED_DOOR);
            add_entity_rxy(lock_x, ry - lock_ry + wall_ry, 0, 0, lock_rx, lock_ry, LOCK);
        }
    }

    void game_reset() override {
        BasicAbstractGame::game_reset();

        last_fire_time = 0;

        int min_sep = 4;
        int num_walls = 10;
        int object_group_size = 6;
        int buf_h = 4;
        float door_prob = .125;
        float min_pct = .1;

        if (options.distribution_mode == EasyMode) {
            num_walls = 5;
            object_group_size = 2;
            door_prob = 0;
            min_pct = .2;
        }

        std::vector<int> partition = rand_gen.partition(main_height - min_sep * num_walls - buf_h, num_walls);

        int curr_h = 0;

        for (int part : partition) {
            int dy = min_sep + part;
            curr_h += dy;

            bool use_door = (dy > 5) && rand_gen.rand01() < door_prob;

            add_walls(curr_h, use_door, min_pct);
        }

        agent->y = agent->ry;

        int num_good = rand_gen.randn(10) + 10;
        int num_bad = rand_gen.randn(10) + 10;

        for (int i = 0; i < main_width; i++) {
            auto present = add_entity_rxy(i + .5, main_height - .5, 0, 0, .5, .5, PRESENT);
            choose_random_theme(present);
        }

        spawn_entities(num_good, .5, GOOD_OBJ, 0, 0, main_width, main_height);
        spawn_entities(num_bad, .5, BAD_OBJ, 0, 0, main_width, main_height);

        for (auto ent : entities) {
            if (ent->type == GOOD_OBJ || ent->type == BAD_OBJ) {
                ent->image_theme = rand_gen.randn(object_group_size);
                fit_aspect_ratio(ent);
            }
        }

        agent->rotation = -1 * PI / 2;
    }

    void game_step() override {
        BasicAbstractGame::game_step();

        if (special_action == 1 && (cur_time - last_fire_time) >= KEY_DURATION) {
            float vx = 0;
            float vy = 1;
            auto new_bullet = add_entity(agent->x, agent->y, vx * bullet_vscale, vy * bullet_vscale, .25, PLAYER_BULLET);
            new_bullet->expire_time = KEY_DURATION;
            new_bullet->collides_with_entities = true;
            last_fire_time = cur_time;
        }
    }

    void serialize(WriteBuffer *b) override {
        BasicAbstractGame::serialize(b);
        b->write_float(min_dim);
        b->write_float(bullet_vscale);
        b->write_int(last_fire_time);
    }

    void deserialize(ReadBuffer *b) override {
        BasicAbstractGame::deserialize(b);
        min_dim = b->read_float();
        bullet_vscale = b->read_float();
        last_fire_time = b->read_int();
    }
};

REGISTER_GAME(NAME, FruitBotGame);
