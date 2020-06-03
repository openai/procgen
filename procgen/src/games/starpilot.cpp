#include "../basic-abstract-game.h"
#include "../assetgen.h"

const std::string NAME = "starpilot";

const float V_SCALE = 2.0f / 5.0f;

const float BG_RATIO = 18;
const float ENEMY_REWARD = 1.0;
const float COMPLETION_BONUS = 10.0;

const int BULLET_PLAYER = 1;
const int BULLET2 = 2;
const int BULLET3 = 3;
const int FLYER = 4;
const int METEOR = 5;
const int CLOUD = 6;
const int TURRET = 7;
const int FAST_FLYER = 8;

const int FINISH_LINE = 9;

const int SHOOTER_WIN_TIME = 500;

const int NUM_BASIC_OBJECTS = 9;
const int NUM_SHIP_THEMES = 7;

bool spawn_cmp(const std::shared_ptr<Entity> &x, const std::shared_ptr<Entity> &y) {
    return (x->spawn_time > y->spawn_time);
}

class StarPilotGame : public BasicAbstractGame {
  public:
    std::vector<std::shared_ptr<Entity>> spawners;

    float hp_vs[NUM_BASIC_OBJECTS] = {};
    float hp_healths[NUM_BASIC_OBJECTS] = {};
    float hp_bullet_r[NUM_BASIC_OBJECTS] = {};
    float hp_object_r[NUM_BASIC_OBJECTS] = {};
    float hp_object_prob_weight[NUM_BASIC_OBJECTS] = {};
    float total_prob_weight = 0.0f;
    float hp_slow_v = 0.0f;
    float hp_weapon_bullet_dist = 0.0f;
    float hp_spawn_right_threshold = 0.0f;

    int hp_min_enemy_delta_t = 0;
    int hp_max_group_size = 0;
    int hp_max_enemy_delta_t = 0;

    StarPilotGame()
        : BasicAbstractGame(NAME) {
        main_width = 16;
        main_height = 16;
    }

    void load_background_images() override {
        main_bg_images_ptr = &space_backgrounds;
    }

    void asset_for_type(int type, std::vector<std::string> &names) override {
        if (type == PLAYER) {
            names.push_back("misc_assets/playerShip2_blue.png");
        } else if (type == BULLET_PLAYER) {
            names.push_back("misc_assets/towerDefense_tile295.png");
        } else if (type == BULLET2) {
            names.push_back("misc_assets/towerDefense_tile296.png");
        } else if (type == BULLET3) {
            names.push_back("misc_assets/towerDefense_tile297.png");
        } else if (type == FLYER || type == FAST_FLYER) {
            names.push_back("misc_assets/spaceShips_001.png");
            names.push_back("misc_assets/spaceShips_002.png");
            names.push_back("misc_assets/spaceShips_003.png");
            names.push_back("misc_assets/spaceShips_004.png");
            names.push_back("misc_assets/spaceShips_005.png");
            names.push_back("misc_assets/spaceShips_006.png");
            names.push_back("misc_assets/spaceShips_007.png");
        } else if (type == METEOR) {
            names.push_back("misc_assets/spaceMeteors_001.png");
            names.push_back("misc_assets/spaceMeteors_002.png");
            names.push_back("misc_assets/spaceMeteors_003.png");
            names.push_back("misc_assets/spaceMeteors_004.png");
            names.push_back("misc_assets/meteorGrey_big1.png");
            names.push_back("misc_assets/meteorGrey_big2.png");
            names.push_back("misc_assets/meteorGrey_big3.png");
            names.push_back("misc_assets/meteorGrey_big4.png");
        } else if (type == CLOUD) {
            names.push_back("misc_assets/spaceEffect1.png");
            names.push_back("misc_assets/spaceEffect2.png");
            names.push_back("misc_assets/spaceEffect3.png");
            names.push_back("misc_assets/spaceEffect4.png");
            names.push_back("misc_assets/spaceEffect5.png");
            names.push_back("misc_assets/spaceEffect6.png");
            names.push_back("misc_assets/spaceEffect7.png");
            names.push_back("misc_assets/spaceEffect8.png");
            names.push_back("misc_assets/spaceEffect9.png");
        } else if (type == TURRET) {
            names.push_back("misc_assets/spaceStation_018.png");
            names.push_back("misc_assets/spaceStation_019.png");
        } else if (type == FINISH_LINE) {
            names.push_back("misc_assets/spaceRockets_001.png");
            names.push_back("misc_assets/spaceRockets_002.png");
            names.push_back("misc_assets/spaceRockets_003.png");
            names.push_back("misc_assets/spaceRockets_004.png");
        }
    }

    void game_draw(QPainter &p, const QRect &rect) override {
        float scale = rect.height() / main_height;

        QColor bg_color = QColor(0, 0, 0);

        p.fillRect(rect, bg_color);

        if (options.use_backgrounds) {
            float bg_k = 3;
            float t = cur_time;
            float x_off = -t * scale * hp_slow_v * 2 / char_dim;

            QRectF r_bg = QRectF(x_off, -rect.height() * (bg_k - 1) / 2, rect.height() * bg_k * BG_RATIO, rect.height() * bg_k);
            tile_image(p, main_bg_images_ptr->at(background_index).get(), r_bg, 1);
        }

        draw_foreground(p, rect);
    }

    void handle_agent_collision(const std::shared_ptr<Entity> &obj) override {
        BasicAbstractGame::handle_agent_collision(obj);

        if (obj->type == FINISH_LINE) {
            step_data.done = true;
            step_data.reward += COMPLETION_BONUS;
            step_data.level_complete = true;
        } else if (is_lethal(obj)) {
            step_data.done = true;
        }
    }

    void handle_collision(const std::shared_ptr<Entity> &src, const std::shared_ptr<Entity> &target) override {
        if (src->type == BULLET_PLAYER && target->type != CLOUD && is_destructible(target->type)) {
            src->will_erase = true;
            target->health -= 1;

            add_entity(src->x, src->y, target->vx, target->vy, .5 * src->rx, EXPLOSION);
        }
    }

    void init_hps() {
        float scale = 1;

        for (int i = 0; i < NUM_BASIC_OBJECTS; i++) {
            hp_vs[i] = 1;
            hp_healths[i] = 0;
            hp_object_prob_weight[i] = 1;
            hp_object_r[i] = scale / 2;
        }

        float default_bullet_r = scale / 2.5;

        if (options.distribution_mode == EasyMode) {
            hp_object_prob_weight[METEOR] = 0;
            hp_object_prob_weight[CLOUD] = 0;
            hp_object_prob_weight[TURRET] = 0;
            hp_object_prob_weight[FAST_FLYER] = 0;
            hp_vs[FLYER] = .75;
            hp_vs[BULLET2] = 1.25;
            hp_healths[TURRET] = 5;
            hp_healths[FLYER] = 2;
            hp_healths[FAST_FLYER] = 1;
            maxspeed = 0.75;
        } else if (options.distribution_mode == HardMode) {
            hp_vs[BULLET2] = 2;

            hp_healths[TURRET] = 5;
            hp_healths[FLYER] = 2;
            hp_healths[FAST_FLYER] = 1;
            maxspeed = 0.75;
        } else if (options.distribution_mode == ExtremeMode) {
            hp_vs[BULLET2] = 2;
            hp_healths[TURRET] = 10;
            hp_healths[FLYER] = 5;
            hp_healths[FAST_FLYER] = 2;
            maxspeed = 0.5;
            default_bullet_r = scale / 5;
        } else {
            fassert(false);
        }

        for (int i = 0; i < NUM_BASIC_OBJECTS; i++) {
            hp_bullet_r[i] = default_bullet_r;
        }

        hp_healths[METEOR] = 500;

        hp_vs[FAST_FLYER] = 1.5;

        hp_vs[BULLET_PLAYER] = 2;

        hp_vs[BULLET3] = 2;
        hp_object_r[TURRET] = scale * 2;
        hp_object_r[METEOR] = scale * 2;
        hp_object_r[CLOUD] = scale * 2;

        hp_object_prob_weight[FLYER] = 3;

        hp_slow_v = .5;
        hp_max_group_size = 5;

        hp_weapon_bullet_dist = 3;

        hp_min_enemy_delta_t = 10;
        hp_max_enemy_delta_t = hp_min_enemy_delta_t + 20;

        hp_spawn_right_threshold = 0.9f;

        hp_object_prob_weight[BULLET_PLAYER] = 0;
        hp_object_prob_weight[BULLET2] = 0;
        hp_object_prob_weight[BULLET3] = 0;

        total_prob_weight = 0;

        for (int i = 2; i < NUM_BASIC_OBJECTS; i++) {
            total_prob_weight += hp_object_prob_weight[i];
        }
    }

    void add_spawners() {
        int t = 1 + rand_gen.randint(hp_min_enemy_delta_t, hp_max_enemy_delta_t);

        bool can_spawn_left = options.distribution_mode != EasyMode;

        for (int i = 0; t <= SHOOTER_WIN_TIME; i++) {
            int group_size = 1;
            float start_weight = rand_gen.rand01() * total_prob_weight;
            float curr_weight = start_weight;
            int type;

            for (type = 2; type < NUM_BASIC_OBJECTS; type++) {
                curr_weight -= hp_object_prob_weight[type];

                if (curr_weight <= 0) {
                    break;
                }
            }

            if (type >= NUM_BASIC_OBJECTS) {
                type = NUM_BASIC_OBJECTS - 1;
            }

            float r = hp_object_r[type];
            int flyer_theme = 0;

            if (type == FLYER || type == FAST_FLYER) {
                group_size = rand_gen.randint(0, hp_max_group_size) + 1;
                flyer_theme = rand_gen.randn(NUM_SHIP_THEMES);
            }

            float y_pos = rand_pos(r, main_height);

            for (int j = 0; j < group_size; j++) {
                int spawn_time = t + j * 5;
                int fire_time = rand_gen.randint(10, 100);

                float k = 2 * PI / 4;
                float theta = (rand_gen.rand01() - .5) * k;
                float v_scale = hp_vs[type];

                if (rand_gen.randint(0, 2) == 1) {
                    theta = 0;
                }

                float health = hp_healths[type];

                if (type == METEOR || type == CLOUD) {
                    theta = 0;
                    v_scale = hp_slow_v;
                    fire_time = -1;
                } else if (type == TURRET) {
                    theta = 0;
                    v_scale = hp_slow_v;
                    fire_time = rand_gen.randint(20, 30);
                }

                v_scale *= V_SCALE;

                float vx = -1 * cos(theta) * v_scale;
                float vy = sin(theta) * v_scale;

                bool spawn_right = true;
                float x_pos;

                if (type == FLYER || type == FAST_FLYER) {
                    if (rand_gen.rand01() > hp_spawn_right_threshold && can_spawn_left) {
                        spawn_right = false;
                    }
                }

                if (spawn_right) {
                    x_pos = main_width + r;
                } else {
                    x_pos = -r;
                    vx *= -1;
                }

                auto spawner = std::make_shared<Entity>(x_pos, y_pos, vx, vy, r, type);
                spawner->fire_time = fire_time;
                spawner->spawn_time = spawn_time;
                spawner->health = health;

                if (type == CLOUD) {
                    spawner->render_z = 1;
                    choose_random_theme(spawner);
                } else if (type == METEOR) {
                    choose_random_theme(spawner);
                } else if (type == FLYER || type == FAST_FLYER) {
                    spawner->image_theme = flyer_theme;
                    spawner->rotation = ((vx > 0) ? -1 : 1) * PI / 2;
                } else if (type == TURRET) {
                    choose_random_theme(spawner);
                    match_aspect_ratio(spawner);
                }

                spawners.push_back(spawner);
            }

            t += rand_gen.randint(hp_min_enemy_delta_t, hp_max_enemy_delta_t);
        }
    }

    void game_reset() override {
        BasicAbstractGame::game_reset();

        options.center_agent = false;

        init_hps();

        spawners.clear();

        add_spawners();

        std::sort(spawners.begin(), spawners.end(), spawn_cmp);

        agent->rotation = PI / 2;
        choose_random_theme(agent);
    }

    bool is_lethal(const std::shared_ptr<Entity> &e1) {
        int type = e1->type;

        return type == FLYER || type == FAST_FLYER || type == BULLET2 || type == BULLET3 || type == TURRET || type == METEOR;
    }

    bool is_destructible(int type) {
        return type == FLYER || type == FAST_FLYER || type == TURRET || type == METEOR;
    }

    bool should_fire(const std::shared_ptr<Entity> &e1, int cur_time) {
        if (e1->fire_time <= 0) {
            return false;
        }

        if (e1->type == TURRET) {
            return (cur_time - e1->spawn_time) % e1->fire_time == 0;
        }

        return cur_time - e1->spawn_time == e1->fire_time;
    }

    void game_step() override {
        BasicAbstractGame::game_step();

        bool is_firing = special_action != 0;

        for (int i = (int)(entities.size()) - 1; i >= 0; i--) {
            auto m = entities[i];

            if (m->type == PLAYER)
                continue;

            if (should_fire(m, cur_time)) {
                int bullet_type = m->type == TURRET ? BULLET3 : BULLET2;
                float bullet_r = hp_bullet_r[m->type];
                float b_vx = agent->x - m->x;
                float b_vy = agent->y - m->y;
                float bv_scale = hp_vs[bullet_type] * V_SCALE / sqrt(b_vx * b_vx + b_vy * b_vy);
                b_vx = b_vx * bv_scale;
                b_vy = b_vy * bv_scale;

                std::shared_ptr<Entity> new_bullet(new Entity(m->x, m->y, b_vx, b_vy, bullet_r, bullet_type));
                new_bullet->face_direction(b_vx, b_vy, -1 * PI / 2);
                entities.push_back(new_bullet);
            }

            if (m->health <= 0 && is_destructible(m->type) && !m->will_erase) {
                spawn_child(m, EXPLOSION, .5 * m->rx, true);

                step_data.reward += ENEMY_REWARD;
                m->will_erase = true;
            }
        }

        while (spawners.size() > 0 && cur_time == spawners[int(spawners.size()) - 1]->spawn_time) {
            entities.push_back(spawners[int(spawners.size()) - 1]);
            spawners.pop_back();
        }

        float bullet_r = hp_bullet_r[PLAYER];

        if (is_firing) {
            float theta = special_action == 2 ? PI : 0;
            float v_scale = hp_vs[BULLET_PLAYER] * V_SCALE;

            float vx = cos(theta) * v_scale;
            float vy = sin(theta) * v_scale;
            float x_off = agent->rx * cos(theta);

            auto bullet = std::make_shared<Entity>(agent->x + x_off, agent->y, vx, vy, bullet_r, BULLET_PLAYER);
            bullet->collides_with_entities = true;
            bullet->face_direction(vx, vy);
            bullet->rotation -= PI / 2;
            entities.push_back(bullet);
        }

        if (cur_time == SHOOTER_WIN_TIME) {
            auto finish = std::make_shared<Entity>(main_width, main_height / 2, -1 * hp_slow_v * V_SCALE, 0, 2, main_height / 2, FINISH_LINE);
            choose_random_theme(finish);
            match_aspect_ratio(finish, false);
            finish->x = main_width + finish->rx;
            entities.push_back(finish);
        }
    }

    void serialize(WriteBuffer *b) override {
        BasicAbstractGame::serialize(b);
        write_entities(b, spawners);
    }

    void deserialize(ReadBuffer *b) override {
        BasicAbstractGame::deserialize(b);
        read_entities(b, spawners);

        init_hps();
    }
};

REGISTER_GAME(NAME, StarPilotGame);
