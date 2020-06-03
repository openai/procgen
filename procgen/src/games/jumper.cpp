#include "../basic-abstract-game.h"
#include "../assetgen.h"
#include "../roomgen.h"
#include "../mazegen.h"
#include <set>
#include <queue>
#include <memory>

const std::string NAME = "jumper";

const float GOAL_REWARD = 10.0f;

const int GOAL = 1;
const int SPIKE = 2;
const int CAVEWALL = 6;
const int CAVEWALL_TOP = 7;

const int PLAYER_JUMP = 9;
const int PLAYER_LEFT1 = 10;
const int PLAYER_LEFT2 = 11;
const int PLAYER_RIGHT1 = 12;
const int PLAYER_RIGHT2 = 13;

const int MAZE_SCALE = 3;

const int JUMP_COOLDOWN = 3;
const int NUM_WALL_THEMES = 4;

class Jumper : public BasicAbstractGame {
  public:
    std::shared_ptr<Entity> goal;
    int jump_count = 0;
    int jump_delta = 0;
    int jump_time = 0;
    bool has_support = false;
    bool facing_right = false;
    int wall_theme = 0;
    float compass_dim = 0.0f;
    std::unique_ptr<RoomGenerator> room_manager;

    Jumper()
        : BasicAbstractGame(NAME) {
        room_manager = std::make_unique<RoomGenerator>(this);
    }

    void load_background_images() override {
        main_bg_images_ptr = &platform_backgrounds;
    }

    void asset_for_type(int type, std::vector<std::string> &names) override {
        if (type == PLAYER) {
            names.push_back("misc_assets/bunny2_ready.png");
        } else if (type == SPIKE) {
            names.push_back("misc_assets/spikeMan_stand.png");
        } else if (type == GOAL) {
            names.push_back("misc_assets/carrot.png");
        } else if (type == PLAYER_JUMP) {
            names.push_back("misc_assets/bunny2_jump.png");
        } else if (type == PLAYER_RIGHT1) {
            names.push_back("misc_assets/bunny2_walk1.png");
        } else if (type == PLAYER_RIGHT2) {
            names.push_back("misc_assets/bunny2_walk2.png");
        } else if (type == PLAYER_LEFT1) {
            names.push_back("misc_assets/bunny2_walk1.png");
        } else if (type == PLAYER_LEFT2) {
            names.push_back("misc_assets/bunny2_walk2.png");
        } else if (type == CAVEWALL_TOP) {
            names.push_back("platformer/tileBlue_05.png");
            names.push_back("platformer/tileGreen_05.png");
            names.push_back("platformer/tileYellow_06.png");
            names.push_back("platformer/tileBrown_06.png");
        } else if (type == CAVEWALL) {
            names.push_back("platformer/tileBlue_08.png");
            names.push_back("platformer/tileGreen_08.png");
            names.push_back("platformer/tileYellow_09.png");
            names.push_back("platformer/tileBrown_09.png");
        }
    }

    void handle_agent_collision(const std::shared_ptr<Entity> &obj) override {
        BasicAbstractGame::handle_agent_collision(obj);

        if (obj->type == GOAL) {
            step_data.reward += GOAL_REWARD;
            step_data.level_complete = true;
            step_data.done = true;
        } else if (obj->type == SPIKE) {
            step_data.done = true;
        }
    }

    void update_agent_velocity() override {
        float v_scale = get_agent_acceleration_scale();

        agent->vx = (1 - mixrate) * agent->vx + mixrate * maxspeed * action_vx * v_scale;
        if (action_vy != 0)
            agent->vy = maxspeed * action_vy * 2;
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
        if (BasicAbstractGame::is_blocked(src, target, is_horizontal))
            return true;
        if (src->type == PLAYER && is_wall(target))
            return true;

        return false;
    }

    int image_for_type(int type) override {
        if (type == PLAYER) {
            // if (jump_delta < 0) {
            //     return PLAYER_JUMP;
            // } else
            if (fabs(agent->vx) < .01 && action_vx == 0 && has_support) {
                return PLAYER;
            } else if (facing_right) {
                return (cur_time / 5 % 2 == 0 || !has_support) ? PLAYER_RIGHT1 : PLAYER_RIGHT2;
            } else {
                return (cur_time / 5 % 2 == 0 || !has_support) ? PLAYER_LEFT1 : PLAYER_LEFT2;
            }
        }

        return BasicAbstractGame::image_for_type(type);
    }

    void draw_compass(QPainter &p, const QRect &rect) {
        QRectF compass_rect = get_abs_rect(view_dim - compass_dim - .25, .25, compass_dim, compass_dim);
        QColor clock_color = QColor(168, 166, 158);

        set_pen_brush_color(p, clock_color);
        p.drawEllipse(compass_rect);
        QColor highlight_color = QColor(252, 186, 3);

        float pen_thickness = rect.width() / (256.0 / compass_dim);

        set_pen_brush_color(p, highlight_color, pen_thickness);
        float cx = compass_rect.center().x();
        float cy = compass_rect.center().y();
        float cr = compass_rect.width() / 2 * .95;
        float theta = get_theta(agent, goal);

        p.drawLine(cx, cy, cx + cr * cos(theta), cy - cr * sin(theta));

        float dist = get_distance(agent, goal);
        float dist_pct = dist / (main_width * sqrt(2));

        float bar_thickness = compass_dim / 8;

        QRectF dist_rect = get_abs_rect(view_dim - compass_dim - .25, .25 + compass_dim, compass_dim * dist_pct, bar_thickness);
        p.fillRect(dist_rect, highlight_color);

        if (jump_delta < 0 && !has_support) {
            QRectF r1 = get_object_rect(agent);
            p.setBrush(QColor(255, 255, 255, 120));
            p.setPen(Qt::NoPen);
            p.drawEllipse(QRect(r1.x(), r1.y() + r1.height() * (5.0 / 6), r1.width(), r1.height() / 3));
        }
    }

    void game_draw(QPainter &p, const QRect &rect) override {
        BasicAbstractGame::game_draw(p, rect);

        if (options.distribution_mode != MemoryMode) {
            draw_compass(p, rect);
        }
    }

    bool will_reflect(int src, int target) override {
        return BasicAbstractGame::will_reflect(src, target);
    }

    bool is_space_on_ground(int x, int y) {
        if (get_obj(x, y) != SPACE)
            return false;
        if (get_obj(x, y + 1) != SPACE)
            return false;
        int below_obj = get_obj(x, y - 1);
        return below_obj == CAVEWALL || below_obj == out_of_bounds_object;
    }

    bool is_top_wall(int x, int y) {
        return get_obj(x, y) == CAVEWALL && get_obj(x, y + 1) == SPACE;
    }

    bool is_left_wall(int x, int y) {
        return get_obj(x, y) == CAVEWALL && get_obj(x + 1, y) == SPACE;
    }

    bool is_right_wall(int x, int y) {
        return get_obj(x, y) == CAVEWALL && get_obj(x - 1, y) == SPACE;
    }

    void choose_world_dim() override {
        int dist_diff = options.distribution_mode;

        int world_dim = 20;

        if (dist_diff == EasyMode) {
            world_dim = 20;
        } else if (dist_diff == HardMode) {
            world_dim = 40;
        } else if (dist_diff == MemoryMode) {
            world_dim = 45;
        }

        main_width = world_dim;
        main_height = world_dim;
    }

    void game_reset() override {
        if (options.distribution_mode == EasyMode) {
            visibility = 12;
            compass_dim = 3;
        } else {
            visibility = 16;
            compass_dim = 2;
        }

        if (options.distribution_mode == MemoryMode) {
            timeout = 2000;
        }

        BasicAbstractGame::game_reset();

        out_of_bounds_object = WALL_OBJ;

        wall_theme = rand_gen.randn(NUM_WALL_THEMES);
        jump_count = 0;
        jump_delta = 0;
        jump_time = 0;
        has_support = false;
        facing_right = true;

        int maze_dim = main_width / MAZE_SCALE;

        std::shared_ptr<MazeGen> maze_gen(new MazeGen(&rand_gen, maze_dim));
        maze_gen->generate_maze_no_dead_ends();

        for (int i = 0; i < grid_size; i++) {
            int obj = maze_gen->grid.get((i % main_width) / MAZE_SCALE + 1, (i / main_width) / MAZE_SCALE + 1);

            float prob = obj == WALL_OBJ ? .8 : .2;
            if (rand_gen.rand01() < prob) {
                set_obj(i, WALL_OBJ);
            } else {
                set_obj(i, SPACE);
            }
        }

        for (int iteration = 0; iteration < 2; iteration++) {
            room_manager->update();
        }

        // add border cells. needed for helping with solvability and proper rendering of bottommost floor tiles
        for (int i = 0; i < main_width; i++) {
            set_obj(i, 0, CAVEWALL);
            set_obj(i, main_height - 1, CAVEWALL);
        }

        for (int i = 0; i < main_height; i++) {
            set_obj(0, i, CAVEWALL);
            set_obj(main_width - 1, i, CAVEWALL);
        }

        std::set<int> best_room;
        room_manager->find_best_room(best_room);
        fassert(best_room.size() > 0);

        for (int i = 0; i < grid_size; i++) {
            set_obj(i, CAVEWALL);
        }

        std::vector<int> free_cells;

        for (int i : best_room) {
            set_obj(i, SPACE);
            free_cells.push_back(i);
        }

        int goal_cell = rand_gen.choose_one(free_cells);

        std::vector<int> agent_candidates;

        for (int i = 0; i < grid_size; i++) {
            int x = i % main_width;
            int y = i / main_width;

            if (is_space_on_ground(x, y)) {
                agent_candidates.push_back(i);
            }
        }

        int agent_cell = rand_gen.choose_one(agent_candidates);

        std::vector<int> goal_path;
        room_manager->find_path(agent_cell, goal_cell, goal_path);

        bool should_prune = options.distribution_mode != MemoryMode;

        if (should_prune) {
            std::set<int> wide_path;
            wide_path.insert(goal_path.begin(), goal_path.end());
            room_manager->expand_room(wide_path, 4);

            for (int i = 0; i < grid_size; i++) {
                set_obj(i, CAVEWALL);
            }

            for (int i : wide_path) {
                set_obj(i, SPACE);
            }
        }

        goal = spawn_entity_at_idx(goal_cell, .5, GOAL);
        float spike_prob = options.distribution_mode == MemoryMode ? 0 : .2;

        for (int i = 0; i < grid_size; i++) {
            int x = i % main_width;
            int y = i / main_width;

            if (is_space_on_ground(x, y) && (is_space_on_ground(x - 1, y) && is_space_on_ground(x + 1, y))) {
                if (rand_gen.rand01() < spike_prob) {
                    set_obj(x, y, SPIKE);
                }
            }
        }

        // We prevent log vertical walls to improve solvability
        for (int i = 0; i < grid_size; i++) {
            int x = i % main_width;
            int y = i / main_width;

            if (is_left_wall(x, y) && is_left_wall(x, y + 1) && is_left_wall(x, y + 2)) {
                set_obj(x, y + rand_gen.randn(3), SPACE);
            }

            if (is_right_wall(x, y) && is_right_wall(x, y + 1) && is_right_wall(x, y + 2)) {
                set_obj(x, y + rand_gen.randn(3), SPACE);
            }
        }

        agent->x = (agent_cell % main_width) + .5;
        agent->y = (agent_cell / main_width) + agent->ry;

        std::vector<int> spike_cells = get_cells_with_type(SPIKE);

        for (int spike_cell : spike_cells) {
            set_obj(spike_cell, SPACE);
            float spike_ry = 0.4f;
            float spike_rx = 0.23f;

            add_entity_rxy((spike_cell % main_width) + .5, (spike_cell / main_width) + spike_ry, 0, 0, spike_rx, spike_ry, SPIKE);
        }

        for (int i = 0; i < grid_size; i++) {
            int x = i % main_width;
            int y = i / main_width;

            if (is_top_wall(x, y)) {
                set_obj(x, y, CAVEWALL_TOP);
            }
        }

        agent->rx = 0.254f;
        agent->ry = 0.4f;

        out_of_bounds_object = CAVEWALL;
    }

    bool is_wall(int obj) {
        return obj == CAVEWALL || obj == CAVEWALL_TOP;
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

        jump_delta = 0;

        has_support = can_support(obj_below_1) || can_support(obj_below_2);

        if (has_support) {
            jump_count = 2;
        }

        if (action_vy == 1 && jump_count > 0 && (cur_time - jump_time > JUMP_COOLDOWN)) {
            jump_count -= 1;
            jump_delta = -1;
        } else {
            action_vy = 0;
        }

        if (action_vy > 0) {
            jump_time = cur_time;
        }

        action_vrot = 0;
    }

    void game_step() override {
        BasicAbstractGame::game_step();

        if (action_vx > 0)
            agent->is_reflected = false;
        if (action_vx < 0)
            agent->is_reflected = true;

        if (fabs(agent->vx) + fabs(agent->vy) > .05) {
            auto trail = add_entity_rxy(agent->x, agent->y - agent->ry * .5, 0, 0.01f, 0.3f, 0.2f, TRAIL);
            trail->expire_time = 8;
            trail->alpha = .5;
        }

        if (agent->vy > -2) {
            agent->vy -= 0.15f;
        }
    }

    void serialize(WriteBuffer *b) override {
        BasicAbstractGame::serialize(b);
        b->write_int(jump_count);
        b->write_int(jump_delta);
        b->write_int(jump_time);
        b->write_bool(has_support);
        b->write_bool(facing_right);
        b->write_int(wall_theme);
        b->write_float(compass_dim);
    }

    void deserialize(ReadBuffer *b) override {
        BasicAbstractGame::deserialize(b);
        jump_count = b->read_int();
        jump_delta = b->read_int();
        jump_time = b->read_int();
        has_support = b->read_bool();
        facing_right = b->read_bool();
        wall_theme = b->read_int();
        compass_dim = b->read_float();

        int goal_idx = find_entity_index(GOAL);
        fassert(goal_idx >= 0);
        goal = entities[goal_idx];
    }
};

REGISTER_GAME(NAME, Jumper);
