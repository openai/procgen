#include "../basic-abstract-game.h"
#include "../assetgen.h"
#include "../roomgen.h"
#include "../mazegen.h"
#include <set>
#include <queue>
#include <memory>

const std::string NAME = "hovercraft";

const int WORLD_DIM = 20;
const float REWARD = 1.0f;
const float LARGE_REWARD = 10.0f;
const float SURVIVAL_REWARD = .005f;

const int HEART = 1;
const int OBSTACLE = 2;
const int BARRIER = 3;
const int HEART_LARGE = 4;

const int NUM_WALL_THEMES = 3;

const int END_TIME = 500;

class Hovercraft : public BasicAbstractGame {
  public:
    int wall_theme;
    int wall_spawn_timer;
    int base_interval;
    float vy_scale;
    float gravity;
    float vel_in;

    Hovercraft()
        : BasicAbstractGame(NAME) {
        visibility = WORLD_DIM;
    }

    void load_background_images() override {
        main_bg_images_ptr = &platform_easy_backgrounds;
    }

    void asset_for_type(int type, std::vector<std::string> &names) override {
        if (type == PLAYER) {
            names.push_back("hovercraft/shipBeige_manned.png");
            names.push_back("hovercraft/shipBlue_manned.png");
            names.push_back("hovercraft/shipGreen_manned.png");
            names.push_back("hovercraft/shipPink_manned.png");
            names.push_back("hovercraft/shipYellow_manned.png");
        } else if (type == OBSTACLE) {
            names.push_back("hovercraft/blade_1.png");
            names.push_back("hovercraft/blade_2.png");
            names.push_back("hovercraft/blade_3.png");
        } else if (type == BARRIER) {
            names.push_back("misc_assets/tile_bricksGrey.png");
            names.push_back("misc_assets/tile_bricksGrown.png");
            names.push_back("misc_assets/tile_bricksRed.png");
        } else if (type == HEART) {
            names.push_back("hovercraft/tilePink_24.png");
        } else if (type == HEART_LARGE) {
            names.push_back("hovercraft/tilePink_24.png");
        }
    }

    bool use_block_asset(int type) override {
        return true;
    }

    float get_tile_aspect_ratio(const std::shared_ptr<Entity> &ent) override {
        if (ent->type == BARRIER)
            return -1;

        return 0;
    }

    void handle_agent_collision(const std::shared_ptr<Entity> &obj) override {
        BasicAbstractGame::handle_agent_collision(obj);

        if (obj->type == BARRIER) {
            step_data.done = true;
        } else if (obj->type == OBSTACLE) {
            step_data.done = true;
        } else if (obj->type == HEART) {
            step_data.reward += REWARD;
            obj->will_erase = true;
        } else if (obj->type == HEART_LARGE) {
            step_data.reward += LARGE_REWARD;
            step_data.done = true;
        }
    }

    void game_draw(QPainter &p, const QRect &rect) override {
        float scale = rect.height() / main_height;

        QColor bg_color = QColor(0, 0, 0);
        p.fillRect(rect, bg_color);

        if (options.use_backgrounds) {
            float bg_k = 3;
            float t = cur_time;
            float x_off = -t * scale * vel_in;
            float BG_RATIO = 18.0;

            QRectF r_bg = QRectF(x_off, -rect.height() * (bg_k - 1) / 2, rect.height() * bg_k * BG_RATIO, rect.height() * bg_k);
            tile_image(p, main_bg_images_ptr->at(background_index).get(), r_bg, 1);
        }

        draw_foreground(p, rect);
    }

    void choose_world_dim() override {
        main_width = WORLD_DIM;
        main_height = WORLD_DIM;
    }

    void update_agent_velocity() override {
        float v_scale = get_agent_acceleration_scale();

        agent->vx = (1 - mixrate) * agent->vx;
        agent->vx += mixrate * maxspeed * action_vx * v_scale;

        if (action_vy > 0) {
            agent->vy += vy_scale * action_vy;
        }

        if (agent->vy > 2) {
            agent->vy = 2;
        }
    }

    void game_reset() override {
        BasicAbstractGame::game_reset();

        choose_random_theme(agent);
        wall_theme = rand_gen.randn(NUM_WALL_THEMES);

        options.center_agent = false;

        agent->x = 1;
        agent->y = WORLD_DIM / 2;

        agent->rx = .65;
        agent->ry = .65;

        wall_spawn_timer = 0;

        if (options.distribution_mode == EasyMode) {
            vy_scale = .25;
            gravity = -0.1;
            vel_in = .125;
            base_interval = 10;
            agent->image_theme = 1;
            wall_theme = 2;
            background_index = 0;
        } else {
            vy_scale = .5;
            gravity = -0.2;
            vel_in = .25;
            base_interval = 5;
        }
    }

    void game_step() override {
        BasicAbstractGame::game_step();

        agent->vy += gravity;

        if (cur_time >= END_TIME) {
            if (cur_time == END_TIME + 2 * base_interval) {
                float obj_r = 2;
                std::shared_ptr<Entity> ent(new Entity(main_width + obj_r, main_height / 2, -1 * vel_in, 0, obj_r, obj_r, HEART_LARGE));
                entities.push_back(ent);
                match_aspect_ratio(ent);
            } else if (cur_time == END_TIME + 6 * base_interval) {
                std::shared_ptr<Entity> ent(new Entity(main_width + .5, main_height / 2, -1 * vel_in, 0, .5, main_height / 2, BARRIER));
                entities.push_back(ent);
                ent->image_theme = wall_theme;
            }
        } else {
            if (wall_spawn_timer == 0) {
                float obj_ry = 3 + rand_gen.randn(4);
                float obj_y = (main_height - 2 * obj_ry) * rand_gen.rand01() + obj_ry;
                std::shared_ptr<Entity> ent(new Entity(main_width + .5, obj_y, -1 * vel_in, 0, .5, obj_ry, BARRIER));

                if (!has_any_collision(ent)) {
                    ent->image_theme = wall_theme;
                    wall_spawn_timer = (3 + rand_gen.randn(7)) * base_interval;
                    entities.push_back(ent);
                } else {
                    wall_spawn_timer = 3;
                }
            } else {
                wall_spawn_timer -= 1;
            }

            if (cur_time % (2 * base_interval) == 0) {
                int type = rand_gen.randn(3) == 1 ? HEART : OBSTACLE;
                float obj_r = type == HEART ? .65 : 1;
                float margin = type == HEART ? 0 : 2.0;
                float obj_y = (main_height - 2) * rand_gen.rand01() + 1;
                std::shared_ptr<Entity> ent(new Entity(main_width, obj_y, -1 * vel_in, 0, obj_r, obj_r, type));

                if (!has_any_collision(ent, margin)) {
                    choose_random_theme(ent);
                    match_aspect_ratio(ent);
                    entities.push_back(ent);
                }
            }
        }

        for (auto ent : entities) {
            if (ent->type == OBSTACLE) {
                float k = ent->image_theme == 1 ? 1 : 8; // asset-specific rotation angles
                ent->rotation = -1 * PI * (cur_time % 2) / k;
            }
        }

        auto trail = add_entity_rxy(agent->x, agent->y - agent->ry * .5, 0, 0.01f, 0.3f, 0.2f, TRAIL);
        trail->expire_time = 8;
        trail->alpha = .5;

        float margin = .1;

        if (agent->y < agent->ry + margin) {
            step_data.done = true;
        } else if (agent->y > main_height - agent->ry - margin) {
            step_data.done = true;
        }

        step_data.reward += SURVIVAL_REWARD;
    }

    void serialize(WriteBuffer *b) override {
        BasicAbstractGame::serialize(b);
        b->write_int(wall_theme);
        b->write_int(wall_spawn_timer);
        b->write_int(base_interval);
        b->write_float(vy_scale);
        b->write_float(gravity);
        b->write_float(vel_in);
    }

    void deserialize(ReadBuffer *b) override {
        BasicAbstractGame::deserialize(b);
        wall_theme = b->read_int();
        wall_spawn_timer = b->read_int();
        base_interval = b->read_int();
        vy_scale = b->read_float();
        gravity = b->read_float();
        vel_in = b->read_float();
    }
};

REGISTER_GAME(NAME, Hovercraft);
