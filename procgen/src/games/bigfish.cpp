#include "../basic-abstract-game.h"
#include "../assetgen.h"
#include <set>
#include <queue>

const std::string NAME = "bigfish";

const int COMPLETION_BONUS = 10.0f;
const int POSITIVE_REWARD = 1.0f;

const int FISH = 2;

const float FISH_MIN_R = .25;
const float FISH_MAX_R = 2;

const int FISH_QUOTA = 30;

class BigFish : public BasicAbstractGame {
  public:
    int fish_eaten = 0;
    float r_inc = 0.0;

    BigFish()
        : BasicAbstractGame(NAME) {
        timeout = 6000;

        main_width = 20;
        main_height = 20;
    }

    void load_background_images() override {
        main_bg_images_ptr = &water_backgrounds;
    }

    void asset_for_type(int type, std::vector<std::string> &names) override {
        if (type == PLAYER) {
            names.push_back("misc_assets/fishTile_072.png");
        } else if (type == FISH) {
            names.push_back("misc_assets/fishTile_074.png");
            names.push_back("misc_assets/fishTile_078.png");
            names.push_back("misc_assets/fishTile_080.png");
        }
    }

    void handle_agent_collision(const std::shared_ptr<Entity> &obj) override {
        BasicAbstractGame::handle_agent_collision(obj);

        if (obj->type == FISH) {
            if (obj->rx > agent->rx) {
                step_data.done = true;
            } else {
                step_data.reward += POSITIVE_REWARD;
                obj->will_erase = true;
                agent->rx += r_inc;
                agent->ry += r_inc;
                fish_eaten += 1;
            }
        }
    }

    void game_reset() override {
        BasicAbstractGame::game_reset();

        options.center_agent = false;
        fish_eaten = 0;

        float start_r = .5;

        if (options.distribution_mode == EasyMode) {
            start_r = 1;
        }

        r_inc = (FISH_MAX_R - start_r) / FISH_QUOTA;

        agent->rx = start_r;
        agent->ry = start_r;
        agent->y = 1 + agent->ry;
    }

    void game_step() override {
        BasicAbstractGame::game_step();

        if (rand_gen.randn(10) == 1) {
            float ent_r = (FISH_MAX_R - FISH_MIN_R) * pow(rand_gen.rand01(), 1.4) + FISH_MIN_R;
            float ent_y = rand_gen.rand01() * (main_height - 2 * ent_r);
            float moves_right = rand_gen.rand01() < .5;
            float ent_vx = (.15 + rand_gen.rand01() * .25) * (moves_right ? 1 : -1);
            float ent_x = moves_right ? -1 * ent_r : main_width + ent_r;
            int type = FISH;
            auto ent = add_entity(ent_x, ent_y, ent_vx, 0, ent_r, type);
            choose_random_theme(ent);
            match_aspect_ratio(ent);
            ent->is_reflected = !moves_right;
        }

        if (fish_eaten >= FISH_QUOTA) {
            step_data.done = true;
            step_data.reward += COMPLETION_BONUS;
            step_data.level_complete = true;
        }

        if (action_vx > 0)
            agent->is_reflected = false;
        if (action_vx < 0)
            agent->is_reflected = true;
    }

    void serialize(WriteBuffer *b) override {
        BasicAbstractGame::serialize(b);
        b->write_int(fish_eaten);
        b->write_float(r_inc);
    }

    void deserialize(ReadBuffer *b) override {
        BasicAbstractGame::deserialize(b);
        fish_eaten = b->read_int();
        r_inc = b->read_float();
    }
};

REGISTER_GAME(NAME, BigFish);
