#include "../basic-abstract-game.h"
#include "../assetgen.h"
#include <set>
#include <queue>

const std::string NAME = "caterpillar";

class CaterpillarSegmentEntity : public Entity {
  public:
    int turn_y = -1;
    float turn_vx = 0;
    int grid_x = 0;
    int grid_y = 0;

    CaterpillarSegmentEntity() {};

    CaterpillarSegmentEntity(float _x, float _y, float _dx, float _dy, float _r, int _type)
        : Entity(_x, _y, _dx, _dy, _r, _r, _type){};

    void serialize(WriteBuffer *b) override {
        Entity::serialize(b);
        b->write_int(turn_y);
        b->write_float(turn_vx);
        b->write_int(grid_x);
        b->write_int(grid_y);
    };

    void deserialize(ReadBuffer *b) override {
        Entity::deserialize(b);
        turn_y = b->read_int();
        turn_vx = b->read_float();
        grid_x = b->read_int();
        grid_y = b->read_int();
    };

    std::string get_type_name() override {
        return "caterpillar_segment";
    };
};

const int COMPLETION_BONUS = 12.0f;
const int POSITIVE_REWARD = 1.0f;
const int CATERPILLAR_SEGMENTS = 12;
const int MAX_MUSHROOMS = 64;
const int MAX_MUSHROOMS_EASY = 24;

const int CATERPILLAR_TYPE = 1;
const int MUSHROOM_TYPE1 = 2;
const int MUSHROOM_TYPE2 = 3;
const int MUSHROOM_TYPE3 = 4;
const int MUSHROOM_TYPE4 = 5;
const int PLAYER_BULLET_TYPE = 6;
const int PLAYER_BOX_TYPE = 7;
const int SPIDER_TYPE = 8;

const int MAX_BULLETS = 1;
const float PLAYER_BOX_HEIGHT = 5;
const float PLAYER_BULLET_SPEED = 1.0;
const float CATERPILLAR_SPEED = 0.5;
const float CATERPILLAR_SPEED_EASY = 0.25;
const float SPLIT_PROBABILITY = 0.1;
const float SPIDER_SPAWN_PROBABILITY = 0.005;
const float SPIDER_SPAWN_PROBABILITY_EASY = 0.0;
const float SPIDER_SPEED = 0.1;
const int SPIDER_CHANGE_VELOCITY_PERIOD = 30;
const int PLAYER_FIRE_PERIOD = 5;
const int NUM_ANIMATION_FRAMES = 2;

class Caterpillar : public BasicAbstractGame {
  public:
    int last_fire_time = 0;
    float caterpillar_speed = 0;

    Caterpillar()
        : BasicAbstractGame(NAME) {
        main_width = 20;
        main_height = 20;
        out_of_bounds_object = WALL_OBJ;
    }

    void load_background_images() override {
        main_bg_images_ptr = &caterpillar_backgrounds;
    }

    void asset_for_type(int type, std::vector<std::string> &names) override {
        if (type == PLAYER) {
            names.push_back("caterpillar/ship-0-2.png");
            names.push_back("caterpillar/ship-1-2.png");
        } else if (type == PLAYER_BULLET_TYPE) {
            names.push_back("caterpillar/laser-bolts-1-0.png");
            names.push_back("caterpillar/laser-bolts-1-1.png");
        } else if (type == CATERPILLAR_TYPE) {
            names.push_back("caterpillar/enemy-small-0-0.png");
            names.push_back("caterpillar/enemy-small-0-1.png");
        } else if (type == MUSHROOM_TYPE1) {
            names.push_back("caterpillar/power-up-0-0.png");
        } else if (type == MUSHROOM_TYPE2) {
            names.push_back("caterpillar/power-up-0-1.png");
        } else if (type == MUSHROOM_TYPE3) {
            names.push_back("caterpillar/power-up-1-0.png");
        } else if (type == MUSHROOM_TYPE4) {
            names.push_back("caterpillar/power-up-1-1.png");
        } else if (type == PLAYER_BOX_TYPE) {
            names.push_back("misc_assets/transparent.png");
        } else if (type == SPIDER_TYPE) {
            names.push_back("caterpillar/enemy-medium-0-0.png");
            names.push_back("caterpillar/enemy-medium-0-1.png");
        }
    }

    bool should_preserve_type_themes(int type) override {
        return type == PLAYER || type == PLAYER_BULLET_TYPE || type == CATERPILLAR_TYPE || type == SPIDER_TYPE;
    }

    int randsign() {
        if (rand_gen.rand01() < 0.5) {
            return -1;
        } else {
            return 1;
        }
    }

    void handle_collision(const std::shared_ptr<Entity> &src, const std::shared_ptr<Entity> &target) override {
        if (src->type == PLAYER_BULLET_TYPE) {
            if (target->type == CATERPILLAR_TYPE) {
                // use a smaller bounding box for the laser, otherwise it's too easy to hit stuff
                // if the laser is too small though, it's too hard to see it
                src->will_erase = true;
                target->will_erase = true;
                int x = round(target->x - 0.5);
                int y = round(target->y - 0.5);
                if (x >= 0 && x < main_width && y >= 0 && y < main_height) {
                    set_obj(x, y, MUSHROOM_TYPE1);
                }
                step_data.reward += POSITIVE_REWARD;
            }
            if (target->type == SPIDER_TYPE) {
                src->will_erase = true;
                target->will_erase = true;
                add_entity(target->x, target->y, 0, 0, .5 * target->rx, EXPLOSION);
            }
        }
    }

    void handle_agent_collision(const std::shared_ptr<Entity> &obj) override {
        if (obj->type == CATERPILLAR_TYPE || obj->type == SPIDER_TYPE) {
            step_data.done = true;
        }
    }

    bool is_blocked(const std::shared_ptr<Entity> &src, int target, bool is_horizontal) override {
        if (src->type == PLAYER) {
            if (target == PLAYER_BOX_TYPE || target == MUSHROOM_TYPE1 || target == MUSHROOM_TYPE2 || target == MUSHROOM_TYPE3 || target == MUSHROOM_TYPE4) {
                return true;
            }
        }
        return BasicAbstractGame::is_blocked(src, target, is_horizontal);
    }

    void handle_grid_collision(const std::shared_ptr<Entity> &obj, int type, int i, int j) override {
        if (obj->type == PLAYER_BULLET_TYPE) {
            int new_type = -1;
            if (type == MUSHROOM_TYPE1) {
                new_type = MUSHROOM_TYPE2;
            } else if (type == MUSHROOM_TYPE2) {
                new_type = MUSHROOM_TYPE3;
            } else if (type == MUSHROOM_TYPE3) {
                new_type = MUSHROOM_TYPE4;
            } else if (type == MUSHROOM_TYPE4) {
                new_type = SPACE;
            }
            if (new_type != -1) {
                obj->will_erase = true;
                set_obj(i, j, new_type);
            }
        }
    }

    void choose_world_dim() override {
        int world_dim;

        if (options.distribution_mode == EasyMode) {
            world_dim = 15;
        } else {
            world_dim = 20;
        }

        main_width = world_dim;
        main_height = world_dim;
    }

    void game_reset() override {
        BasicAbstractGame::game_reset();

        options.center_agent = false;

        last_fire_time = 0;
        agent->rx = 0.4;
        agent->ry = 0.4;
        match_aspect_ratio(agent, false);
        int player_grid_x = rand_gen.randint(1, main_width);
        int player_grid_y = rand_gen.randint(1, PLAYER_BOX_HEIGHT);
        agent->x = player_grid_x + 0.5;
        agent->y = player_grid_y + 0.5;

        int max_mushrooms = 0;
        if (options.distribution_mode == EasyMode) {
            max_mushrooms = MAX_MUSHROOMS_EASY;
            caterpillar_speed = CATERPILLAR_SPEED_EASY;
        } else {
            max_mushrooms = MAX_MUSHROOMS;
            caterpillar_speed = CATERPILLAR_SPEED;
        }

        int num_mushrooms = rand_gen.randint(5, max_mushrooms + 1);
        for (int i = 0; i < num_mushrooms; i++) {
            int x = rand_gen.randint(0, main_width);
            int y = rand_gen.randint(1, main_height - 1);
            if (player_grid_x == x && player_grid_y == y) {
                continue;
            }
            set_obj(x, y, MUSHROOM_TYPE1);
        }

        for (int x = 0; x < main_width; x++) {
            set_obj(x, PLAYER_BOX_HEIGHT, PLAYER_BOX_TYPE);
        }

        // spawn caterpillar
        {
            int direction = randsign();
            // make sure we are at least one block away from the border so that we will turn in time
            const int min_x = 1;
            const int max_x = main_width - 2;
            int x_offset;
            // start in the center so that we can be more likely to get a full length caterpillar
            if (direction == -1) {
                x_offset = rand_gen.randint(min_x, max_x + 1 - CATERPILLAR_SEGMENTS);
            } else {
                x_offset = rand_gen.randint(min_x + CATERPILLAR_SEGMENTS, max_x + 1);
            }
            std::set<int> unused;
            for (int x = min_x; x <= max_x; x++) {
                unused.insert(x);
            }
            for (int i = 0; i < CATERPILLAR_SEGMENTS; i++) {
                int x = x_offset;
                int y = main_height - 1;

                // make sure we are always in bounds and that we don't stack caterpillars on each other
                if (x_offset < min_x || x_offset > max_x || !set_contains(unused, x_offset) || rand_gen.rand01() < SPLIT_PROBABILITY) {
                    direction = randsign();
                    int index = rand_gen.randint(0, unused.size());
                    x_offset = *std::next(unused.begin(), index);
                }
                unused.erase(x_offset);
                // entity locations are offset from the grid
                // that is, a grid square at (0, 0) ranges from (0, 0) to (1, 1) in entity space
                // while an entity of radius 0.5 at (0, 0) ranges from (-0.5, -0.5) to (0.5, 0.5)
                auto e = std::make_shared<CaterpillarSegmentEntity>(x + 0.5, y + 0.5, direction * caterpillar_speed, 0, 0.5, CATERPILLAR_TYPE);
                match_aspect_ratio(e);
                e->collides_with_entities = true;
                e->auto_erase = false;
                e->grid_x = x;
                e->grid_y = y;
                if (e->vx > 0) {
                    e->rotation = -PI / 2;
                } else {
                    e->rotation = PI / 2;
                }
                entities.push_back(e);
                x_offset -= direction;
            }
        }
    }

    float get_agent_acceleration_scale() override {
        return 2.0;
    }

    void game_step() override {
        BasicAbstractGame::game_step();

        agent->image_theme = (agent->image_theme + 1) % NUM_ANIMATION_FRAMES;

        int num_spiders = 0;
        int num_player_bullets = 0;
        int num_caterpillar_segments = 0;
        for (const auto &e : entities) {
            if (e->type == PLAYER_BULLET_TYPE) {
                num_player_bullets++;
                // see if the bullet has hit a mushroom
                check_grid_collisions(e);
                e->image_theme = (e->image_theme + 1) % NUM_ANIMATION_FRAMES;
            } else if (e->type == SPIDER_TYPE) {
                num_spiders++;
                if ((cur_time - e->spawn_time) % SPIDER_CHANGE_VELOCITY_PERIOD == 0) {
                    e->vx = SPIDER_SPEED * rand_gen.randint(-1, 2);
                    if (e->y > PLAYER_BOX_HEIGHT) {
                        e->vy = -SPIDER_SPEED;
                    } else {
                        e->vy = SPIDER_SPEED * rand_gen.randint(-1, 2);
                    }
                }
                e->image_theme = (e->image_theme + 1) % NUM_ANIMATION_FRAMES;
            } else if (e->type == CATERPILLAR_TYPE) {
                num_caterpillar_segments++;
                auto c = std::static_pointer_cast<CaterpillarSegmentEntity>(e);
                // once we have passed the midpoint of the square in the direction
                // of travel, increment the grid location
                if (c->vx > 0) {
                    if (c->x >= c->grid_x + 1.5) {
                        c->grid_x++;
                    }
                } else {
                    if (c->x <= c->grid_x - 0.5) {
                        c->grid_x--;
                    }
                }
                if (c->vy > 0) {
                    if (c->y >= c->grid_y + 1.5) {
                        c->grid_y++;
                    }
                } else {
                    if (c->y <= c->grid_y - 0.5) {
                        c->grid_y--;
                    }
                }

                // update caterpillar state
                if (c->grid_y < 0) {
                    // caterpillar got to the bottom of the screen, game over
                    step_data.done = true;
                } else if (c->grid_x < 0 || c->grid_x >= main_width) {
                    fassert(false);
                } else {
                    // if it's a caterpillar, figure out if we should turn or are currently turning
                    if (c->turn_y != -1) {
                        // if we have a pending turn, execute it
                        if (c->grid_y == c->turn_y) {
                            c->vx = c->turn_vx;
                            c->vy = 0;
                            c->turn_y = -1;
                        }
                    } else {
                        int x_offset = 0;
                        if (c->vx > 0) {
                            x_offset = 1;
                        } else {
                            x_offset = -1;
                        }
                        int obj = get_obj(c->grid_x + x_offset, c->grid_y);
                        if (obj == MUSHROOM_TYPE1 || obj == MUSHROOM_TYPE2 || obj == MUSHROOM_TYPE3 || obj == MUSHROOM_TYPE4 || obj == WALL_OBJ) {
                            c->vx = 0;
                            c->vy = -caterpillar_speed;
                            c->turn_y = c->grid_y - 1;
                            c->turn_vx = -x_offset * caterpillar_speed;
                        }
                    }
                }

                // set rotation of sprite to match movement of caterpillar
                if (c->vx > 0) {
                    c->rotation = -PI / 2;
                } else if (c->vx < 0) {
                    c->rotation = PI / 2;
                } else {
                    c->rotation = 0;
                }
                c->image_theme = (c->image_theme + 1) % NUM_ANIMATION_FRAMES;
            }
        }

        // handle player firing
        bool is_firing = special_action != 0;
        if (is_firing && num_player_bullets < MAX_BULLETS && cur_time - last_fire_time > PLAYER_FIRE_PERIOD) {
            last_fire_time = cur_time;
            auto bullet = std::make_shared<Entity>(agent->x, agent->y + agent->ry, 0, PLAYER_BULLET_SPEED, 0.25, PLAYER_BULLET_TYPE);
            bullet->collision_margin = -0.2;
            match_aspect_ratio(bullet);
            bullet->collides_with_entities = true;
            entities.push_back(bullet);
        }

        float spider_spawn_probability = 0;
        if (options.distribution_mode == EasyMode) {
            spider_spawn_probability = SPIDER_SPAWN_PROBABILITY_EASY;
        } else {
            spider_spawn_probability = SPIDER_SPAWN_PROBABILITY;
        }

        if (num_spiders == 0 && rand_gen.rand01() < spider_spawn_probability) {
            int x = 0;
            float vx = SPIDER_SPEED;
            float vy = SPIDER_SPEED;
            int y = rand_gen.randint(0, PLAYER_BOX_HEIGHT);
            if (rand_gen.rand01() < 0.5) {
                x = main_width - 1;
                vx = -SPIDER_SPEED;
                vy = SPIDER_SPEED;
            }
            auto e = std::make_shared<Entity>(x + 0.5, y + 0.5, vx, vy, 0.75, SPIDER_TYPE);
            match_aspect_ratio(e);
            e->collides_with_entities = true;
            e->spawn_time = cur_time;
            entities.push_back(e);
        }

        // check if we won
        if (num_caterpillar_segments == 0) {
            step_data.done = true;
            step_data.reward += COMPLETION_BONUS;
            step_data.level_complete = true;
        }
    }

    void serialize(WriteBuffer *b) override {
        BasicAbstractGame::serialize(b);
        b->write_int(last_fire_time);
        b->write_float(caterpillar_speed);
    }

    void deserialize(ReadBuffer *b) override {
        BasicAbstractGame::deserialize(b);
        last_fire_time = b->read_int();
        caterpillar_speed = b->read_float();
    }
};

REGISTER_GAME(NAME, Caterpillar);
REGISTER_ENTITY(CaterpillarSegmentEntity);
