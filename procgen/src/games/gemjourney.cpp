#include "../basic-abstract-game.h"
#include "../assetgen.h"
#include <set>
#include <queue>

const std::string NAME = "gemjourney";


const int COMPLETION_BONUS = 10.0f;
const int POSITIVE_REWARD = 1.0f;

const int CRYSTAL_TYPE = 1;
const int MINE_TYPE = 2;
const int PLAYER_BULLET_TYPE = 3;
const int DRONE_TYPE = 4;
const int SHOOTER_TYPE = 5;
const int ENEMY_BULLET_TYPE = 6;
const int SPAWNER_TYPE = 7;
const int PLAYER_TRAIL_TYPE = 8;
const int FADING_ENEMY = 9;

const int MAX_MINES = 15;
const int MAX_MINES_EASY = 5;
const int MAX_BULLETS = 5;
const float MIN_FIRE_SPEED = 0.005;
const float PLAYER_BULLET_SPEED = 0.3;
const int PLAYER_FIRE_PERIOD = 4;
const int ENEMY_ACCEL_PERIOD = 10;
const float ENEMY_MAX_SPEED = 0.2;
const float ENEMY_SPAWN_PROBABILITY_MIN = 0.005;
const float ENEMY_SPAWN_PROBABILITY_MAX = 0.03;
const float ENEMY_SHOOTER_PROBABILITY = 0.3;
const float ENEMY_FIRE_PROBABILITY = 0.05;
const float ENEMY_BULLET_SPEED = 0.3;
const int MAX_ENEMY_COUNT = 8;
const int MAX_ENEMY_COUNT_EASY = 3;
const int NUM_CRYSTALS = 10;
const int NUM_CRYSTALS_EASY = 6;
const int NUM_CRYSTAL_ANIMATION_FRAMES = 5;
const int NUM_PLAYER_BULLET_ANIMATION_FRAMES = 6;
const int NUM_ENEMY_BULLET_ANIMATION_FRAMES = 2;

class GemJourney : public BasicAbstractGame {
  public:
    int crystals_collected = 0;
    int last_fire_time = 0;
    int num_crystals = 0;
    int max_enemy_count = 0;
    float enemy_spawn_probability = 0.0;

    GemJourney()
        : BasicAbstractGame(NAME) {
    }

    void load_background_images() override {
        main_bg_images_ptr = &gemjourney_backgrounds;
    }

    void asset_for_type(int type, std::vector<std::string> &names) override {
        if (type == PLAYER) {
            names.push_back("gemjourney/pig.png");
        } else if (type == CRYSTAL_TYPE) {
            names.push_back("gemjourney/gem-1.png");
            names.push_back("gemjourney/gem-2.png");
            names.push_back("gemjourney/gem-3.png");
            names.push_back("gemjourney/gem-4.png");
            names.push_back("gemjourney/gem-5.png");
        } else if (type == MINE_TYPE) {
            names.push_back("gemjourney/croc.png");
        } else if (type == PLAYER_BULLET_TYPE) {
            names.push_back("gemjourney/fox-run-1.png");
            names.push_back("gemjourney/fox-run-2.png");
            names.push_back("gemjourney/fox-run-3.png");
            names.push_back("gemjourney/fox-run-4.png");
            names.push_back("gemjourney/fox-run-5.png");
            names.push_back("gemjourney/fox-run-6.png");
        } else if (type == DRONE_TYPE) {
            names.push_back("gemjourney/chick.png");
        } else if (type == SHOOTER_TYPE) {
            names.push_back("gemjourney/chicken.png");
        } else if (type == ENEMY_BULLET_TYPE) {
            names.push_back("kenney/Enemies/wormGreen.png");
            names.push_back("kenney/Enemies/wormGreen_move.png");
        } else if (type == SPAWNER_TYPE) {
            names.push_back("gemjourney/house.png");
        }
    }

    bool should_preserve_type_themes(int type) override {
        return type == CRYSTAL_TYPE;
    }

    void handle_agent_collision(const std::shared_ptr<Entity> &obj) override {
        BasicAbstractGame::handle_agent_collision(obj);

        if (obj->type == CRYSTAL_TYPE) {
            step_data.reward += POSITIVE_REWARD;
            obj->will_erase = true;
            crystals_collected += 1;
        } else if (obj->type == MINE_TYPE || obj->type == DRONE_TYPE || obj->type == SHOOTER_TYPE || obj->type == SPAWNER_TYPE || obj->type == ENEMY_BULLET_TYPE) {
            step_data.done = true;
        }
    }

    void handle_collision(const std::shared_ptr<Entity> &src, const std::shared_ptr<Entity> &target) override {
        if (src->type == PLAYER_BULLET_TYPE) {
            if (target->type == DRONE_TYPE || target->type == SHOOTER_TYPE) {
                src->will_erase = true;
                target->will_erase = true;

                auto ent = spawn_child(target, FADING_ENEMY, target->rx);
                ent->vrot = PI / 0.3f;
                ent->grow_rate = 1.0f / 1.2f;
                ent->expire_time = 4;
                ent->alpha_decay = 0.9f;
                ent->image_type = target->image_type;
                choose_step_random_theme(ent);
            } else if (target->type == MINE_TYPE) {
                src->will_erase = true;
            }
        } else if (src->type == ENEMY_BULLET_TYPE) {
            if (target->type == MINE_TYPE) {
                src->will_erase = true;
            }
        }
    }

    void update_agent_velocity() override {
        agent->vx = 0.97 * (agent->vx + action_vx * .05);
        agent->vy = 0.97 * (agent->vy + action_vy * .05);
    }

    void choose_world_dim() override {
        int world_dim;

        if (options.distribution_mode == EasyMode) {
            world_dim = 12;
        } else {
            world_dim = 20;
        }

        main_width = world_dim;
        main_height = world_dim;
    }

    void game_reset() override {
        BasicAbstractGame::game_reset();

        options.center_agent = false;
        enemy_spawn_probability = rand_gen.randrange(ENEMY_SPAWN_PROBABILITY_MIN, ENEMY_SPAWN_PROBABILITY_MAX);
        crystals_collected = 0;
        last_fire_time = 0;
        agent->rx = 0.5;
        agent->ry = 0.5;
        match_aspect_ratio(agent);
        agent->x = main_width / 2.0;
        agent->y = main_height / 2.0;

        int max_mines = 0;
        if (options.distribution_mode == EasyMode) {
            num_crystals = NUM_CRYSTALS_EASY;
            max_enemy_count = MAX_ENEMY_COUNT_EASY;
            max_mines = MAX_MINES_EASY;
        } else {
            num_crystals = NUM_CRYSTALS;
            max_enemy_count = MAX_ENEMY_COUNT;
            max_mines = MAX_MINES;
        }

        for (int i = 0; i < 2; i++) {
            auto ent = std::make_shared<Entity>(0, 0, 0, 0, 1.0, SPAWNER_TYPE);
            ent->y = main_height / 2.0;
            if (i % 2 == 0) {
                // spawn left
                ent->x = ent->rx;
                ent->is_reflected = true;
            } else {
                // spawn right
                ent->x = main_width - ent->rx;
            }
            choose_random_theme(ent);
            match_aspect_ratio(ent);
            entities.push_back(ent);
        }

        for (int i = 0; i < num_crystals; i++) {
            float ent_r = 0.5;
            auto ent = std::make_shared<Entity>(0, 0, 0, 0, ent_r, CRYSTAL_TYPE);
            choose_random_theme(ent);
            reposition(ent, 0, 0, main_width, main_height, true);
            choose_random_theme(ent);
            match_aspect_ratio(ent);
            entities.push_back(ent);
        }

        int num_mines = rand_gen.randn(max_mines+1);
        for (int i = 0; i < num_mines; i++) {
            float ent_r = 0.5;
            auto ent = std::make_shared<Entity>(0, 0, 0, 0, ent_r, MINE_TYPE);
            reposition(ent, 0, 0, main_width, main_height, true);
            choose_random_theme(ent);
            match_aspect_ratio(ent);
            entities.push_back(ent);
        }
    }

    void game_step() override {
        BasicAbstractGame::game_step();

        if (crystals_collected == num_crystals) {
            step_data.done = true;
            step_data.reward += COMPLETION_BONUS;
            step_data.level_complete = true;
        }

        int num_player_bullets = 0;
        int num_enemies = 0;
        for (const auto& e : entities) {
            if (e->type == PLAYER_BULLET_TYPE) {
                num_player_bullets++;
                e->image_theme = (e->image_theme + 1) % NUM_PLAYER_BULLET_ANIMATION_FRAMES;
            } else if (e->type == ENEMY_BULLET_TYPE) {
                e->image_theme = (e->image_theme + 1) % NUM_ENEMY_BULLET_ANIMATION_FRAMES;
            } else if (e->type == DRONE_TYPE || e->type == SHOOTER_TYPE) {
                num_enemies++;
            } else if (e->type == CRYSTAL_TYPE) {
                e->image_theme = (e->image_theme + 1) % NUM_CRYSTAL_ANIMATION_FRAMES;
            }
        }
        
        // movement trail for player
        {
            auto e = agent;
            auto trail = add_entity_rxy(e->x, e->y, 0, 0, e->rx, e->ry, PLAYER_TRAIL_TYPE);
            trail->alpha_decay = 0.5f;
            trail->image_type = PLAYER;
            trail->image_theme = 0;
            trail->vrot = e->vrot;
            trail->rotation = e->rotation;
            trail->expire_time = 5;
        }

        // maybe spawn a new enemy
        if (num_enemies < max_enemy_count && rand_gen.rand01() < enemy_spawn_probability) {
            int type = DRONE_TYPE;
            if (rand_gen.rand01() < ENEMY_SHOOTER_PROBABILITY) {
                type = SHOOTER_TYPE;
            }
            auto ent = std::make_shared<Entity>(0, 0, 0, 0, 0.5, type);
            ent->auto_erase = false;
            ent->spawn_time = cur_time;
            ent->y = main_height / 2.0;
            if (rand_gen.randn(2) == 0) {
                // spawn left
                ent->x = ent->rx;
            } else {
                // spawn right
                ent->x = main_width - ent->rx;
            }
            choose_random_theme(ent);
            match_aspect_ratio(ent);
            entities.push_back(ent);
            num_enemies++;
        }

        std::vector<std::shared_ptr<Entity>> new_entities;
        for (const auto& e : entities) {
            if (e->type == DRONE_TYPE || e->type == SHOOTER_TYPE) {
                // adjust enemy velocities
                if ((cur_time - e->spawn_time) % ENEMY_ACCEL_PERIOD == 0) {
                    float accel_x = rand_gen.rand01() - 0.5;
                    float accel_y = rand_gen.rand01() - 0.5;
                    e->vx += accel_x;
                    e->vy += accel_y;
                    float mag = sqrt(e->vx * e->vx + e->vy * e->vy);
                    if (mag > ENEMY_MAX_SPEED) {
                        e->vx *= ENEMY_MAX_SPEED / mag;
                        e->vy *= ENEMY_MAX_SPEED / mag;
                    }

                    if (e->x + e->vx < 0) {
                        e->vx = fabs(e->vx);
                    } else if (e->x + e->vx > main_width) {
                        e->vx = -fabs(e->vx);
                    }
                    if (e->y + e->vy < 0) {
                        e->vy = fabs(e->vy);
                    } else if (e->y + e->vy > main_height) {
                        e->vy = -fabs(e->vy);
                    }
                }
                
                if (e->type == SHOOTER_TYPE && rand_gen.rand01() < ENEMY_FIRE_PROBABILITY) {
                    float theta = rand_gen.rand01() * 2 * PI;
                    float vx = cos(theta) * ENEMY_BULLET_SPEED;
                    float vy = sin(theta) * ENEMY_BULLET_SPEED;
                    auto bullet = std::make_shared<Entity>(e->x, e->y, vx, vy, 0.5, ENEMY_BULLET_TYPE);
                    bullet->collides_with_entities = true;
                    if (bullet->vx > 0) {
                        bullet->is_reflected = true;
                    }
                    new_entities.push_back(bullet);
                }
            }
        }

        for (const auto& e : new_entities) {
            entities.push_back(e);
        }

        // we fire in the direction the agent is moving and the bullet will have 
        // the agent's velocity added to it
        bool is_firing = special_action != 0;
        float speed = sqrt(agent->vx * agent->vx + agent->vy * agent->vy);
        if (is_firing && speed > MIN_FIRE_SPEED && num_player_bullets < MAX_BULLETS && cur_time - last_fire_time > PLAYER_FIRE_PERIOD) {
            last_fire_time = cur_time;
            float theta = atan2(agent->vy, agent->vx);

            float vx = cos(theta) * PLAYER_BULLET_SPEED + agent->vx;
            float vy = sin(theta) * PLAYER_BULLET_SPEED + agent->vy;
            float x_off = agent->rx * cos(theta);
            float y_off = agent->ry * sin(theta);

            auto bullet = std::make_shared<Entity>(agent->x + x_off, agent->y + y_off, vx, vy, 0.75, PLAYER_BULLET_TYPE);
            bullet->collides_with_entities = true;
            if (bullet->vx < 0) {
                bullet->is_reflected = true;
            }
            entities.push_back(bullet);
        }
    }

    void serialize(WriteBuffer *b) override {
        BasicAbstractGame::serialize(b);
        b->write_int(crystals_collected);
        b->write_int(last_fire_time);
        b->write_int(num_crystals);
        b->write_int(max_enemy_count);
        b->write_float(enemy_spawn_probability);
    }

    void deserialize(ReadBuffer *b) override {
        BasicAbstractGame::deserialize(b);
        crystals_collected = b->read_int();
        last_fire_time = b->read_int();
        num_crystals = b->read_int();
        max_enemy_count = b->read_int();
        enemy_spawn_probability = b->read_float();
    }
};

REGISTER_GAME(NAME, GemJourney);
