#include "../basic-abstract-game.h"
#include "../assetgen.h"
#include <set>
#include <queue>

const float COMPLETION_BONUS = 10.0f;
const float POSITIVE_REWARD = 1.0f;

const int PLAYER_BULLET = 1;
const int TARGET_LEGEND = 2;
const int TARGET_BACKGROUND = 3;
const int PANEL = 6;
const int SHIP = 7;

class PlunderGame : public BasicAbstractGame {
  public:
    int last_fire_time;
    std::vector<bool> lane_directions, target_bools;
    std::vector<int> image_permutation;
    std::vector<float> lane_vels;
    int num_lanes, num_current_ship_types;
    int targets_hit, target_quota;
    float juice_left, r_scale, spawn_prob, legend_r, min_agent_x;

    PlunderGame()
        : BasicAbstractGame() {
        timeout = 4000;

        main_width = 20;
        main_height = 20;

        mixrate = .5;
        maxspeed = 0.85f;
        has_useful_vel_info = false;
    }

    void load_background_images() override {
        main_bg_images_ptr = &water_surface_backgrounds;
    }

    void asset_for_type(int type, std::vector<QString> &names) override {
        if (type == SHIP || type == TARGET_LEGEND || type == PLAYER) {
            names.push_back(QString("misc_assets/ship_1"));
            names.push_back(QString("misc_assets/ship_2"));
            names.push_back(QString("misc_assets/ship_3"));
            names.push_back(QString("misc_assets/ship_4"));
            names.push_back(QString("misc_assets/ship_5"));
            names.push_back(QString("misc_assets/ship_6"));
        } else if (type == PLAYER_BULLET) {
            names.push_back("misc_assets/cannonBall.png");
        } else if (type == PANEL) {
            names.push_back("misc_assets/panel_wood.png");
        } else if (type == TARGET_BACKGROUND) {
            names.push_back("misc_assets/target_red2.png");
        }
    }

    void game_draw(QPainter &p, const QRect &rect) override {
        BasicAbstractGame::game_draw(p, rect);

        QColor juice_color = QColor(66, 245, 135);
        QColor progress_color = QColor(245, 66, 144);

        QRectF dist_rect1 = get_abs_rect(.25, .25, main_width * juice_left, .5);
        p.fillRect(dist_rect1, juice_color);

        QRectF dist_rect2 = get_abs_rect(.25, .75, main_width * (targets_hit * 1.0 / target_quota), .5);
        p.fillRect(dist_rect2, progress_color);
    }

    bool is_target(int theme_num) {
        return target_bools[theme_num];
    }

    void handle_collision(const std::shared_ptr<Entity> &src, const std::shared_ptr<Entity> &target) override {
        if (src->type == PLAYER_BULLET) {
            if (target->type == SHIP) {
                target->will_erase = true;
                src->will_erase = true;

                if (is_target(target->image_theme)) {
                    targets_hit += 1;
                    step_data.reward += POSITIVE_REWARD;
                    juice_left += 0.1f;
                } else {
                    juice_left -= 0.1f;
                }
            } else if (target->type == PANEL) {
                src->will_erase = true;
            }

            if (target->will_erase) {
                add_entity(target->x, target->y, target->vx / 2, target->vy / 2, .5 * target->rx, EXPLOSION);
            }
        }
    }

    void set_action_xy(int move_action) override {
        action_vx = move_action / 3 - 1;
        action_vy = 0;
        action_vrot = 0;
    }

    void game_reset() override {
        BasicAbstractGame::game_reset();

        juice_left = 1;
        targets_hit = 0;
        target_quota = 20;
        spawn_prob = 0.06f;
        r_scale = options.distribution_mode == EasyMode ? 1.5f : 1.0f;
        int num_total_ship_types = 6;

        num_lanes = 5;
        lane_directions.clear();
        lane_vels.clear();
        target_bools.clear();

        std::vector<int> image_idxs;

        for (int i = 0; i < num_total_ship_types; i++) {
            image_idxs.push_back(i);
        }

        image_permutation = rand_gen.choose_n(image_idxs, num_total_ship_types);

        num_current_ship_types = 2;

        for (int i = 0; i < num_total_ship_types; i++) {
            target_bools.push_back(false);
        }

        for (int i = 0; i < num_current_ship_types / 2; i++) {
            target_bools[image_permutation[i]] = true;
        }

        for (int i = 0; i < num_lanes; i++) {
            lane_directions.push_back(rand_gen.rand01() < .5);
            lane_vels.push_back(.15 + .1 * rand_gen.rand01());
        }

        int num_panels = options.distribution_mode == EasyMode ? 0 : rand_gen.randn(4);
        float panel_width = 1.2f;

        if (panel_width > 0) {
            for (int i = 0; i < num_panels; i++) {
                spawn_entity_rxy(panel_width, .5, PANEL, 0, .25 * main_height, main_width, .25 * main_height);
            }
        }

        float key_scale = 1.5;
        legend_r = 2;

        add_entity(legend_r, legend_r, 0, 0, legend_r, TARGET_BACKGROUND);

        auto ent = add_entity(legend_r, legend_r, 0, 0, r_scale * key_scale, TARGET_LEGEND);
        ent->image_theme = image_permutation[0];
        match_aspect_ratio(ent);
        ent->rotation = PI / 2;

        last_fire_time = 0;

        options.center_agent = false;

        agent->rx = r_scale;
        agent->rotation = -1 * PI / 2;
        agent->image_theme = image_permutation[rand_gen.randn(num_current_ship_types / 2) + num_current_ship_types / 2];
        match_aspect_ratio(agent);
        reposition_agent();
        agent->y = 1 + agent->ry;

        min_agent_x = 2 * legend_r + agent->rx;

        if (agent->x < min_agent_x) {
            agent->x = min_agent_x;
        }
    }

    void game_step() override {
        BasicAbstractGame::game_step();

        juice_left -= 0.0015f;

        if (rand_gen.rand01() < spawn_prob) {
            float ent_r = r_scale;
            int lane = rand_gen.randn(num_lanes);
            float ent_y = (lane * .11 + .4) * (main_height / 2 - ent_r) + main_height / 2;
            float moves_right = lane_directions[lane];
            float ent_vx = lane_vels[lane] * (moves_right ? 1 : -1);
            auto ent = std::make_shared<Entity>(0, ent_y, ent_vx, 0, ent_r, SHIP);
            ent->image_type = SHIP;
            ent->image_theme = image_permutation[rand_gen.randn(num_current_ship_types)];
            match_aspect_ratio(ent);
            ent->x = moves_right ? -1 * ent_r : (main_width + ent_r);
            ent->is_reflected = !moves_right;

            if (!has_any_collision(ent)) {
                entities.push_back(ent);
            }
        }

        if (special_action == 1 && (cur_time - last_fire_time) >= 3) {
            auto new_bullet = add_entity(agent->x, agent->y, 0, 1, .25, PLAYER_BULLET);
            new_bullet->collides_with_entities = true;
            new_bullet->expire_time = 50;
            last_fire_time = cur_time;
            juice_left -= 0.02f;
        }

        if (juice_left <= 0) {
            step_data.done = true;
        } else if (juice_left >= 1) {
            juice_left = 1;
        }

        if (targets_hit >= target_quota) {
            step_data.done = true;
            step_data.reward += COMPLETION_BONUS;
            step_data.level_complete = true;
        }

        // don't collide with legend
        if (agent->x < min_agent_x) {
            agent->x = min_agent_x;
        }
    }
};

REGISTER_GAME("plunder", PlunderGame);
