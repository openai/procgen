#include "../basic-abstract-game.h"
#include "../assetgen.h"
#include "../roomgen.h"
#include <set>
#include <queue>

const std::string NAME = "safezone";

const float GOAL_REWARD = 1.0f;
const float COMPLETION_BONUS = 5.0f;

const int TURRET = 3;
const int GOAL = 4;
const int FIRE = 5;
const int GOAL_MARKER = 6;
const int FIRE_TRAIL = 7;
const int EXHAUST = 9;

const int EXPIRE_TIME = 50;
const int GOAL_TARGET = 5;

const int TURRET_SPAWN_TIME = 10;
const int TURRET_STOP_TIME = 20;
const int TURRET_FIRE_TIME = 40;

class SafeZoneGame : public BasicAbstractGame {
  public:
    std::vector<int> starts;
    bool is_horizontal;
    int safe_zone_width;
    int gold_collected;
    float turret_theta;
    float goal_r;
    float fire_vel;
    float turret_r;

    SafeZoneGame()
        : BasicAbstractGame(NAME) {
        mixrate = 0.9f;
    }

    void load_background_images() override {
        main_bg_images_ptr = &space_backgrounds;
    }

    int image_for_type(int type) override {
        return type;
    }

    void asset_for_type(int type, std::vector<std::string> &names) override {
        if (type == PLAYER) {
            names.push_back("misc_assets/playerShip1_blue.png");
        } else if (type == FIRE) {
            names.push_back("safezone/fire1.png");
            names.push_back("safezone/fire2.png");
            names.push_back("safezone/fire3.png");
            names.push_back("safezone/fire4.png");
            names.push_back("safezone/fire5.png");
            names.push_back("safezone/fire6.png");
        } else if (type == TURRET) {
            names.push_back("safezone/spaceBuilding_011.png");
        } else if (type == EXHAUST) {
            names.push_back("misc_assets/towerDefense_tile295.png");
        } else if (type == GOAL) {
            names.push_back("safezone/tileGreen_21.png");
        }
    }

    bool should_preserve_type_themes(int type) override {
        return type == FIRE;
    }

    void handle_agent_collision(const std::shared_ptr<Entity> &obj) override {
        BasicAbstractGame::handle_agent_collision(obj);

        if (obj->type == GOAL) {
            step_data.reward += GOAL_REWARD;
            obj->will_erase = true;
            gold_collected += 1;

            if (gold_collected == GOAL_TARGET) {
                step_data.reward += COMPLETION_BONUS;
                step_data.done = true;
            }
        } else if (obj->type == TURRET) {
            step_data.done = true;
        } else if (obj->type == FIRE) {
            step_data.done = true;
        }
    }

    void update_agent_velocity() override {
        float v_scale = get_agent_acceleration_scale();

        agent->vx += mixrate * maxspeed * action_vx * v_scale * .2;
        agent->vy += mixrate * maxspeed * action_vy * v_scale * .2;

        decay_agent_velocity();
    }

    QRectF get_adjusted_image_rect(int type, const QRectF &rect) override {
        if (type == FIRE) {
            // manually adjust image rects for vertical fire assets
            if (rect.height() > rect.width()) {
                return QRectF(rect.x() - rect.height() / 2 + rect.width() / 2, rect.y() - rect.width() / 2 + rect.height() / 2, rect.height(), rect.width());
            }
        }

        return BasicAbstractGame::get_adjusted_image_rect(type, rect);
    }

    bool use_block_asset(int type) override {
        return true;
    }

    void choose_world_dim() override {
        int world_dim;

        if (options.distribution_mode == EasyMode) {
            world_dim = 20;
        } else {
            world_dim = 25;
        }

        main_width = world_dim;
        main_height = world_dim;
    }

    void game_reset() override {
        BasicAbstractGame::game_reset();

        starts.clear();
        turret_theta = 0;
        gold_collected = 0;

        options.center_agent = false;
        out_of_bounds_object = WALL_OBJ;

        float marker_r = .3;
        float marker_margin = .2;
        float marker_off = marker_r + marker_margin;

        for (int i = 0; i < GOAL_TARGET; i++) {
            auto ent = add_entity(main_width - i * (2 * marker_r + marker_margin) - marker_off, main_height - marker_off, marker_r, GOAL_MARKER);
            ent->image_type = -1;
            ent->render_z = 1;
            ent->health = i;
        }

        is_horizontal = false;
        turret_theta = 0;

        float agent_r;

        if (options.distribution_mode == EasyMode) {
            goal_r = 1.5;
            fire_vel = 1;
            turret_r = 1;
            safe_zone_width = 2;
            agent_r = 1;
        } else {
            goal_r = 1;
            fire_vel = 3;
            turret_r = .5;
            safe_zone_width = 5;
            agent_r = .75;
        }

        agent->rx = agent_r;
        agent->ry = agent_r;
        reposition_agent();
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
            exhaust->image_theme = 0;
        }

        action_vy = acceleration * sin(theta);
        action_vx = acceleration * cos(theta);
        action_vrot = move_action / 3 - 1;
    }

    void game_step() override {
        BasicAbstractGame::game_step();

        int interval = 75;

        for (auto ent : entities) {
            if (ent->type == GOAL_MARKER) {
                ent->image_type = ent->health >= gold_collected ? -1 : GOAL;
            }
        }

        if (cur_time % interval == TURRET_SPAWN_TIME) {
            float inc = 2 * turret_r;
            int num_turrets = main_width / inc;

            std::vector<int> freelanes;
            freelanes.resize(main_width, 0);

            starts = rand_gen.simple_choose(num_turrets - safe_zone_width + 1, 3);

            for (int s : starts) {
                for (int i = 0; i < safe_zone_width; i++) {
                    freelanes[s + i] = 1;
                }
            }

            is_horizontal = rand_gen.randn(2) == 1;
            bool will_reflect = rand_gen.randn(2) == 1;

            if (will_reflect) {
                turret_theta = is_horizontal ? PI : 3 * PI / 2;
            } else {
                turret_theta = is_horizontal ? 0 : PI / 2;
            }

            for (int i = 0; i < num_turrets; i++) {
                if (freelanes[i] != 1) {
                    float end = will_reflect ? main_width : -1 * inc;
                    float x_off = is_horizontal ? end : i * inc;
                    float y_off = is_horizontal ? i * inc: end;
                    auto ent = add_entity(x_off + turret_r, y_off + turret_r, turret_r, TURRET);
                    float turret_vel = inc / (TURRET_STOP_TIME - TURRET_SPAWN_TIME);
                    ent->vx = cos(turret_theta) * turret_vel;
                    ent->vy = sin(turret_theta) * turret_vel;
                    ent->expire_time = EXPIRE_TIME;
                    ent->rotation = -1 * turret_theta + PI / 2;
                }
            }

            auto ent = spawn_entity(goal_r, GOAL, 0, 0, main_width, main_height);
            ent->expire_time = 2 * EXPIRE_TIME;
        } else if (cur_time % interval >= TURRET_FIRE_TIME) {
            for (int i = 0; i < (int)(entities.size()); i++) {
                auto ent = entities[i];
                if (ent->type == TURRET) {
                    float r_fire = 1;
                    float vx = cos(turret_theta) * fire_vel;
                    float vy = sin(turret_theta) * fire_vel;
                    float rx = fabs(cos(turret_theta) * r_fire + sin(turret_theta) * .2f);
                    float ry = fabs(cos(turret_theta) * .2f + sin(turret_theta) * r_fire);
                    float x_off = cos(turret_theta) * (.5 + r_fire + r_fire * (cur_time % 6) / 3.0);
                    float y_off = sin(turret_theta) * (.5 + r_fire + r_fire * (cur_time % 6) / 3.0);
                    auto new_fire = add_entity_rxy(ent->x + x_off, ent->y + y_off, vx, vy, rx, ry, FIRE);
                    new_fire->image_theme = 0;//is_horizontal ? 1 : 0;
                    new_fire->rotation = PI - turret_theta;
                }
            }
        } else if (cur_time % interval >= TURRET_STOP_TIME) {
            for (auto ent : entities) {
                if (ent->type == TURRET) {
                    ent->vx = 0;
                    ent->vy = 0;
                }
            }
        }

        for (int i = (int)(entities.size()) - 1; i >= 0; i--) {
            auto ent = entities[i];

            if (ent->type == FIRE || ent->type == FIRE_TRAIL) {
                ent->image_theme = cur_time % 6;
            } else if (ent->type == GOAL) {
                ent->rx = goal_r * (1 + .1 * cos(cur_time));
                ent->ry = goal_r * (1 + .1 * cos(cur_time));
            }
        }
    }

    void serialize(WriteBuffer *b) override {
        BasicAbstractGame::serialize(b);
        b->write_vector_int(starts);
        b->write_bool(is_horizontal);
        b->write_int(safe_zone_width);
        b->write_int(gold_collected);
        b->write_float(turret_theta);
        b->write_float(goal_r);
        b->write_float(fire_vel);
        b->write_float(turret_r);
    }

    void deserialize(ReadBuffer *b) override {
        BasicAbstractGame::deserialize(b);
        starts = b->read_vector_int();
        is_horizontal = b->read_bool();
        safe_zone_width = b->read_int();
        gold_collected = b->read_int();
        turret_theta = b->read_float();
        goal_r = b->read_float();
        fire_vel = b->read_float();
        turret_r = b->read_float();
    }
};

REGISTER_GAME(NAME, SafeZoneGame);
