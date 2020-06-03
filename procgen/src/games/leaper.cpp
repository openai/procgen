#include "../basic-abstract-game.h"
#include "../qt-utils.h"

const std::string NAME = "leaper";

const int LOG = 1;
const int ROAD = 2;
const int WATER = 3;
const int CAR = 4;
const int FINISH_LINE = 5;

const float MONSTER_RADIUS = 0.25;
const float LOG_RADIUS = 0.45f;

const int GOAL_REWARD = 10.0f;

const int NSTEP = 5;
const int FROG_ANIMATION_FRAMES = NSTEP;

const float MAX_SPEED = 2 / (NSTEP - 1.0);
const float VEL_DECAY = MAX_SPEED / NSTEP;

float sign(float x) {
    return x > 0 ? +1 : (x == 0 ? 0 : -1);
}

class LeaperGame : public BasicAbstractGame {
  public:
    int bottom_road_y = 0;
    std::vector<float> road_lane_speeds;
    int bottom_water_y = 0;
    std::vector<float> water_lane_speeds;
    int goal_y = 0;

    LeaperGame()
        : BasicAbstractGame(NAME) {
        maxspeed = MAX_SPEED;
        timeout = 500;
    }

    void load_background_images() override {
        main_bg_images_ptr = &topdown_backgrounds;
    }

    void asset_for_type(int type, std::vector<std::string> &names) override {
        if (type == ROAD) {
            names.push_back("misc_assets/roadTile6b.png");
        } else if (type == WATER) {
            names.push_back("misc_assets/terrainTile6.png");
        } else if (type == CAR) {
            names.push_back("misc_assets/car_yellow_5.png");
            names.push_back("misc_assets/car_black_1.png");
            names.push_back("misc_assets/car_blue_2.png");
            names.push_back("misc_assets/car_green_3.png");
            names.push_back("misc_assets/car_red_4.png");
        } else if (type == LOG) {
            names.push_back("misc_assets/elementWood044.png");
        } else if (type == PLAYER) {
            names.push_back("misc_assets/frog1.png");
            names.push_back("misc_assets/frog2.png");
            names.push_back("misc_assets/frog4.png");
            names.push_back("misc_assets/frog6.png");
            names.push_back("misc_assets/frog7.png");
        } else if (type == FINISH_LINE) {
            names.push_back("misc_assets/finish2.png");
        }
    }

    float get_tile_aspect_ratio(const std::shared_ptr<Entity> &ent) override {
        if (ent->type == FINISH_LINE) {
            return 1;
        }

        return BasicAbstractGame::get_tile_aspect_ratio(ent);
    }

    void handle_agent_collision(const std::shared_ptr<Entity> &obj) override {
        if (obj->type == CAR) {
            step_data.done = true;
        } else if (obj->type == FINISH_LINE && agent->vx == 0 && agent->vy == 0) {
            step_data.reward += GOAL_REWARD;
            step_data.done = true;
            step_data.level_complete = true;
        }
    }

    bool use_block_asset(int type) override {
        return type == WATER || type == ROAD;
    }

    bool should_preserve_type_themes(int type) override {
        return type == PLAYER;
    }

    float rand_sign() {
        if (rand_gen.rand01() < 0.5) {
            return 1.0;
        } else {
            return -1.0;
        }
    }

    void choose_world_dim() override {
        int dist_diff = options.distribution_mode;

        int world_dim = 20;

        if (dist_diff == EasyMode) {
            world_dim = 9;
        } else if (dist_diff == HardMode) {
            world_dim = 15;
        }

        main_width = world_dim;
        main_height = world_dim;
    }

    int choose_extra_space() {
        return options.distribution_mode == EasyMode ? 0 : rand_gen.randn(2);
    }

    void game_reset() override {
        BasicAbstractGame::game_reset();

        options.center_agent = false;

        agent->y = agent->ry;

        float min_car_speed = 0.05f;
        float max_car_speed = 0.2f;
        float min_log_speed = 0.05f;
        float max_log_speed = 0.1f;

        if (options.distribution_mode == EasyMode) {
            min_car_speed = 0.03f;
            max_car_speed = 0.12f;
            min_log_speed = 0.025f;
            max_log_speed = 0.075f;
        } else if (options.distribution_mode == ExtremeMode) {
            min_car_speed = 0.1f;
            max_car_speed = 0.3f;
            min_log_speed = 0.1f;
            max_log_speed = 0.2f;
        }

        // road
        bottom_road_y = choose_extra_space() + 1;

        int max_diff = options.distribution_mode == EasyMode ? 3 : 4;
        int difficulty = rand_gen.randn(max_diff + 1);

        // half the time we add an extra lane to either roads or water
        int extra_lane_option = options.distribution_mode == EasyMode ? 0 : rand_gen.randn(4);

        int num_road_lanes = difficulty + (extra_lane_option == 2 ? 1 : 0);
        road_lane_speeds.clear();
        for (int lane = 0; lane < num_road_lanes; lane++) {
            road_lane_speeds.push_back(rand_sign() * rand_gen.randrange(min_car_speed, max_car_speed));
            fill_elem(0, bottom_road_y + lane, main_width, 1, ROAD);
        }

        // water
        bottom_water_y = bottom_road_y + num_road_lanes + choose_extra_space() + 1;

        water_lane_speeds.clear();
        int num_water_lanes = difficulty + (extra_lane_option == 3 ? 1 : 0);
        int curr_sign = rand_sign();
        for (int lane = 0; lane < num_water_lanes; lane++) {
            water_lane_speeds.push_back(curr_sign * rand_gen.randrange(min_log_speed, max_log_speed));
            curr_sign *= -1;
            fill_elem(0, bottom_water_y + lane, main_width, 1, WATER);
        }

        goal_y = bottom_water_y + num_water_lanes + 1;

        // spawn initial entities
        for (int i = 0; i < main_width / std::min(min_car_speed, min_log_speed); i++) {
            spawn_entities();
            step_entities(entities);
        }

        add_entity_rxy(main_width / 2.0, goal_y - .5, 0, 0, main_width / 2.0, .5, FINISH_LINE);
    }

    void spawn_entities() {
        // cars
        for (int lane = 0; lane < int(road_lane_speeds.size()); lane++) {
            float speed = road_lane_speeds[lane];
            float spawn_prob = fabs(speed) / 6.0;
            if (rand_gen.rand01() < spawn_prob) {
                float x = speed > 0 ? (-1 * MONSTER_RADIUS) : (main_width + MONSTER_RADIUS);
                auto m = std::make_shared<Entity>(x, bottom_road_y + lane + 0.5, speed, 0, 2 * MONSTER_RADIUS, MONSTER_RADIUS, CAR);
                choose_random_theme(m);
                if (speed < 0) {
                    m->rotation = PI;
                }
                if (!has_any_collision(m)) {
                    entities.push_back(m);
                }
            }
        }

        // logs
        for (int lane = 0; lane < int(water_lane_speeds.size()); lane++) {
            float speed = water_lane_speeds[lane];
            float spawn_prob = fabs(speed) / 2.0;
            if (rand_gen.rand01() < spawn_prob) {
                float x = speed > 0 ? (-1 * LOG_RADIUS) : (main_width + LOG_RADIUS);
                auto m = std::make_shared<Entity>(x, bottom_water_y + lane + 0.5, speed, 0, LOG_RADIUS, LOG);
                if (!has_any_collision(m)) {
                    entities.push_back(m);
                }
            }
        }
    }

    void decay_vel(float &vel) {
        float vel_sign = sign(1.0 * vel);
        vel = (fabs(vel) - VEL_DECAY);
        if (vel < 0)
            vel = 0;
        vel = vel * vel_sign;
    }

    void update_agent_velocity() override {
        if (agent->vx == 0 && agent->vy == 0) {
            if (action_vx != 0) {
                agent->vx = maxspeed * action_vx;
                agent->image_theme = 1;
                agent->rotation = (agent->vx > 0 ? 1 : -1) * PI / 2;
            } else if (action_vy != 0) {
                agent->vy = maxspeed * action_vy;
                agent->image_theme = 1;
                agent->rotation = agent->vy > 0 ? 0 : PI;
            }
        }

        decay_vel(agent->vx);
        decay_vel(agent->vy);
    }

    QRectF get_adjusted_image_rect(int type, const QRectF &rect) override {
        if (type == PLAYER) {
            return adjust_rect(rect, QRectF(0, -.275, 1, 1.55));
        }

        return BasicAbstractGame::get_adjusted_image_rect(type, rect);
    }

    void game_step() override {
        if (agent->image_theme >= 1) {
            agent->image_theme = (agent->image_theme + 1) % FROG_ANIMATION_FRAMES;
        }

        BasicAbstractGame::game_step();

        spawn_entities();

        bool standing_on_log = false;
        float log_vx = 0.0;
        float margin = -1 * agent->rx;
        for (auto &m : entities) {
            if (m->type == LOG && has_collision(agent, m, margin)) {
                // we're standing on a log, don't die
                standing_on_log = true;
                log_vx = m->vx;
            }
        }

        if (get_obj(agent->x, agent->y) == WATER) {
            if (!standing_on_log && agent->vx == 0 && agent->vy == 0) {
                step_data.done = true;
            }
        }

        if (standing_on_log) {
            agent->x += log_vx;
        }

        if (is_out_of_bounds(agent)) {
            step_data.done = true;
        }
    }

    void serialize(WriteBuffer *b) override {
        BasicAbstractGame::serialize(b);
        b->write_int(bottom_road_y);
        b->write_vector_float(road_lane_speeds);
        b->write_int(bottom_water_y);
        b->write_vector_float(water_lane_speeds);
        b->write_int(goal_y);
    }

    void deserialize(ReadBuffer *b) override {
        BasicAbstractGame::deserialize(b);
        bottom_road_y = b->read_int();
        road_lane_speeds = b->read_vector_float();
        bottom_water_y = b->read_int();
        water_lane_speeds = b->read_vector_float();
        goal_y = b->read_int();
    }
};

REGISTER_GAME(NAME, LeaperGame);
