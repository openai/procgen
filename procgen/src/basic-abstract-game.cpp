#include "basic-abstract-game.h"
#include "resources.h"
#include "assetgen.h"
#include "qt-utils.h"

const float MAXVTHETA = 15 * PI / 180;
const float MIXRATEROT = 0.5f;

// A small constant buffer for handling collision detction and object pushing
const float POS_EPS = -0.001f;

// When the grid isn't integer aligned, consecutive blocks render with small gaps between them
// This hack closes the gaps
const float RENDER_EPS = 0.02f;

// objects with type lower than this threshold will be rendered with procgen assets
// objects with type higher than this threshold will be rendered with colored grid squares
const int USE_ASSET_THRESHOLD = 100;
const int MAX_ASSETS = USE_ASSET_THRESHOLD;
const int MAX_IMAGE_THEMES = 10;

BasicAbstractGame::BasicAbstractGame(std::string name)
    : Game(name) {
    char_dim = 5;

    main_width = 0;
    main_height = 0;

    visibility = 16;
    min_visibility = 0;

    mixrate = 0.5;
    maxspeed = 0.5;

    // only used by a few games
    max_jump = maxspeed;

    default_action = 4;
    last_move_action = 7;
    bg_tile_ratio = 0;

    main_bg_images_ptr = 0;

    out_of_bounds_object = INVALID_OBJ;
    has_useful_vel_info = true;
}

BasicAbstractGame::~BasicAbstractGame() {
    if (main_bg_images_ptr != nullptr && use_procgen_background) {
        delete main_bg_images_ptr;
    }
}

void BasicAbstractGame::game_init() {
    if (!options.use_generated_assets) {
        load_background_images();
    }

    if (main_bg_images_ptr == nullptr) {
        main_bg_images_ptr = new std::vector<std::shared_ptr<QImage>>();
        use_procgen_background = true;
        auto main_bg_image = std::make_shared<QImage>(500, 500, QImage::Format_RGB32);
        main_bg_images_ptr->push_back(main_bg_image);
    } else {
        use_procgen_background = false;
    }

    basic_assets.clear();
    basic_reflections.clear();
    asset_aspect_ratios.clear();
    asset_num_themes.clear();

    basic_assets.resize(USE_ASSET_THRESHOLD * MAX_IMAGE_THEMES, nullptr);
    basic_reflections.resize(USE_ASSET_THRESHOLD * MAX_IMAGE_THEMES, nullptr);
    asset_aspect_ratios.resize(USE_ASSET_THRESHOLD * MAX_IMAGE_THEMES, 0);
    asset_num_themes.resize(USE_ASSET_THRESHOLD, 0);
}

void BasicAbstractGame::initialize_asset_if_necessary(int img_idx) {
    if (basic_assets.at(img_idx) != nullptr)
        return;

    int type = img_idx % MAX_ASSETS;
    int theme = img_idx / MAX_ASSETS;

    theme = mask_theme_if_necessary(theme, type);

    std::shared_ptr<QImage> asset_ptr = nullptr;
    float aspect_ratio;
    int num_themes;
    std::vector<std::string> names;

    if (!options.use_generated_assets) {
        asset_for_type(type, names);

        if (names.size() == 0) {
            reserved_asset_for_type(type, names);
        }
    }

    if (names.size() == 0) {
        AssetGen pgen(&asset_rand_gen);
        asset_rand_gen.seed(fixed_asset_seed + type);

        std::shared_ptr<QImage> small_image(new QImage(64, 64, QImage::Format_ARGB32));
        asset_ptr = small_image;
        pgen.generate_resource(asset_ptr, 0, 5, use_block_asset(type));

        num_themes = 1;
        aspect_ratio = 1.0;
    } else {
        asset_ptr = get_asset_ptr(names[theme]);
        num_themes = (int)(names.size());
        aspect_ratio = asset_ptr->width() * 1.0 / asset_ptr->height();
    }

    basic_assets[img_idx] = asset_ptr;
    asset_aspect_ratios[img_idx] = aspect_ratio;
    asset_num_themes[type] = num_themes;

    std::shared_ptr<QImage> reflection_ptr(new QImage(asset_ptr->mirrored(true, false)));
    basic_reflections[img_idx] = reflection_ptr;
}

void BasicAbstractGame::fill_elem(int x, int y, int dx, int dy, char elem) {
    for (int j = 0; j < dx; j++) {
        for (int k = 0; k < dy; k++) {
            grid.set(x + j, y + k, elem);
        }
    }
}

float BasicAbstractGame::get_distance(const std::shared_ptr<Entity> &p0, const std::shared_ptr<Entity> &p1) {
    float p0_x = p0->x;
    float p0_y = p0->y;
    float p1_x = p1->x;
    float p1_y = p1->y;

    float dx = p0_x - p1_x;
    float dy = p0_y - p1_y;

    return sqrt(dx * dx + dy * dy);
}

void BasicAbstractGame::check_grid_collisions(const std::shared_ptr<Entity> &ent) {
    float ax = ent->x;
    float ay = ent->y;
    float arx = ent->rx;
    float ary = ent->ry;

    int min_x = int(ax - (arx + POS_EPS));
    int max_x = int(ax + (arx + POS_EPS));
    int min_y = int(ay - (ary + POS_EPS));
    int max_y = int(ay + (ary + POS_EPS));

    for (int x = min_x; x <= max_x; x++) {
        for (int y = min_y; y <= max_y; y++) {
            int grid_type = get_obj_from_floats(x, y);

            if (grid_type != SPACE) {
                handle_grid_collision(ent, grid_type, x, y);
            }
        }
    }
}

int BasicAbstractGame::get_obj_from_floats(float i, float j) {
    if (i < 0)
        return out_of_bounds_object;
    if (j < 0)
        return out_of_bounds_object;

    return get_obj(floor(i), floor(j));
}

int BasicAbstractGame::get_agent_index() {
    return int(agent->y) * main_width + int(agent->x);
}

int BasicAbstractGame::get_obj(int x, int y) {
    if (!grid.contains(x, y)) {
        return out_of_bounds_object;
    }
    return grid.get(x, y);
}

int BasicAbstractGame::to_grid_idx(int x, int y) {
    if (!grid.contains(x, y)) {
        return INVALID_IDX;
    }
    return grid.to_index(x, y);
}

void BasicAbstractGame::to_grid_xy(int idx, int *x, int *y) {
    return grid.to_xy(idx, x, y);
}

int BasicAbstractGame::get_obj(int idx) {
    if (!grid.contains_index(idx)) {
        return out_of_bounds_object;
    }
    return grid.get_index(idx);
}

std::vector<int> BasicAbstractGame::get_cells_with_type(int type) {
    std::vector<int> cells;

    for (int i = 0; i < grid_size; i++) {
        if (grid.get_index(i) == type) {
            cells.push_back(i);
        }
    }

    return cells;
}

void BasicAbstractGame::set_obj(int idx, int elem) {
    grid.set_index(idx, elem);
}

void BasicAbstractGame::set_obj(int x, int y, int elem) {
    grid.set(x, y, elem);
}

std::shared_ptr<Entity> BasicAbstractGame::spawn_child(const std::shared_ptr<Entity> &src, int type, float obj_r, bool match_vel) {
    float vx = match_vel ? src->vx : 0;
    float vy = match_vel ? src->vy : 0;
    auto child = std::make_shared<Entity>(src->x, src->y, vx, vy, obj_r, type);
    entities.push_back(child);
    return child;
}

float BasicAbstractGame::get_theta(const std::shared_ptr<Entity> &src, const std::shared_ptr<Entity> &target) {
    float dx = target->x - src->x;
    float dy = target->y - src->y;

    return atan2(dy, dx);
}

bool BasicAbstractGame::push_obj(const std::shared_ptr<Entity> &src, const std::shared_ptr<Entity> &target, bool is_horizontal, int depth) {
    float rsum = is_horizontal ? (src->rx + target->rx) : (src->ry + target->ry);
    float delx = target->x - src->x;
    float dely = target->y - src->y;
    float t_vx = 0;
    float t_vy = 0;

    if (is_horizontal) {
        t_vx = src->x + sign(delx) * rsum - target->x;
    } else {
        t_vy = src->y + sign(dely) * rsum - target->y;
    }

    bool block = false;

    // Rare numerical conditions (dependent on POS_EPS) could cause infinite loops.
    // For now we break quit after a small depth.
    if (depth < 5) {
        block = sub_step(target, t_vx, t_vy, depth + 1);
    }

    if (is_horizontal) {
        target->vx = 0;
    } else {
        target->vy = 0;
    }

    return block;
}

bool BasicAbstractGame::sub_step(const std::shared_ptr<Entity> &obj, float _vx, float _vy, int depth) {
    if (obj->will_erase)
        return false;

    float ny = obj->y + _vy;
    float nx = obj->x + _vx;

    float margin = 0.98f;

    bool is_horizontal = _vx != 0;

    bool block = false;
    bool reflect = false;

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            int type2 = get_obj_from_floats(nx + obj->rx * margin * (2 * i - 1), ny + obj->ry * margin * (2 * j - 1));
            block = block || is_blocked(obj, type2, is_horizontal);
            reflect = reflect || will_reflect(obj->type, type2);
        }
    }

    if (reflect) {
        if (is_horizontal) {
            float delta;

            if (_vx < 0) {
                delta = ceil(nx - obj->rx) - (nx - obj->rx);
            } else {
                delta = floor(nx + obj->rx) - (nx + obj->rx);
            }

            obj->vx = -1 * obj->vx;
            nx = nx + 2 * delta;
        } else {
            float delta;

            if (_vy < 0) {
                delta = ceil(ny - obj->ry) - (ny - obj->ry);
            } else {
                delta = floor(ny + obj->ry) - (ny + obj->ry);
            }

            obj->vy = -1 * obj->vy;
            ny = ny + 2 * delta;
        }
    } else if (block) {
        if (is_horizontal) {
            if (grid_step) {
                nx = obj->x;
            } else {
                nx = _vx > 0 ? (floor(nx + obj->rx) - obj->rx) : (ceil(nx - obj->rx) + obj->rx);
            }
        } else {
            if (grid_step) {
                ny = obj->y;
            } else {
                ny = _vy > 0 ? (floor(ny + obj->ry) - obj->ry) : (ceil(ny - obj->ry) + obj->ry);
            }
        }
    }

    obj->x = nx;
    obj->y = ny;

    bool block2 = false;

    for (int i = (int)(entities.size()) - 1; i >= 0; i--) {
        auto m = entities[i];

        if (m == obj || m->will_erase) {
            continue;
        }

        bool curr_block = false;

        if (has_collision(obj, m, POS_EPS)) {
            if (is_blocked_ents(obj, m, is_horizontal)) {
                curr_block = true;
            } else if (will_reflect(obj->type, m->type)) {
                if (is_horizontal) {
                    float delx = m->x - obj->x;
                    float rsum = m->rx + obj->rx;
                    obj->x += _vx > 0 ? -2 * (rsum - delx) : 2 * (rsum + delx);
                    obj->vx = -1 * obj->vx;
                } else {
                    float dely = m->y - obj->y;
                    float rsum = m->ry + obj->ry;
                    obj->y += _vy > 0 ? -2 * (rsum - dely) : 2 * (rsum + dely);
                    obj->vy = -1 * obj->vy;
                }
            }

            if (curr_block) {
                push_obj(m, obj, is_horizontal, depth);
            }
        }

        block2 = block2 || curr_block;
    }

    return block || block2;
}

/*
  Can be overridden to specify world dimensions different than the default on level generation.
*/
void BasicAbstractGame::choose_world_dim() {
}

/*
  The agent collided with this obj. This check is always performed.
*/
void BasicAbstractGame::handle_agent_collision(const std::shared_ptr<Entity> &obj) {
}

/*
  The obj collided with an object in the grid at (i, j) with the given type.
  Right now obj is always the agent.
*/
void BasicAbstractGame::handle_grid_collision(const std::shared_ptr<Entity> &obj, int type, int i, int j) {
}

/*
  The object src collided with an object target.
  This check is performed if src->collides_with_entities is set and src->collision_margin is used for the
  collision test.
*/
void BasicAbstractGame::handle_collision(const std::shared_ptr<Entity> &src, const std::shared_ptr<Entity> &target) {
}

/*
  Determines whether assets of this type will be generated as blocks, covering the full canvas.
*/
bool BasicAbstractGame::use_block_asset(int type) {
    return false;
}

// 0 means don't tile, positive value means tile horizontally, negative value means tile vertically
float BasicAbstractGame::get_tile_aspect_ratio(const std::shared_ptr<Entity> &type) {
    return 0;
}

void BasicAbstractGame::asset_for_type(int type, std::vector<std::string> &names) {
}

void BasicAbstractGame::reserved_asset_for_type(int type, std::vector<std::string> &names) {
    if (type == EXPLOSION) {
        names.push_back("misc_assets/explosion1.png");
    } else if (type == EXPLOSION2) {
        names.push_back("misc_assets/explosion2.png");
    } else if (type == EXPLOSION3) {
        names.push_back("misc_assets/explosion3.png");
    } else if (type == EXPLOSION4) {
        names.push_back("misc_assets/explosion4.png");
    } else if (type == EXPLOSION5) {
        names.push_back("misc_assets/explosion5.png");
    } else if (type == TRAIL) {
        names.push_back("misc_assets/iconCircle_white.png");
    }
}

void BasicAbstractGame::load_background_images() {
}

/*
  Use this image index for objects of the given type
*/
int BasicAbstractGame::image_for_type(int type) {
    return abs(type);
}

int BasicAbstractGame::theme_for_grid_obj(int type) {
    return 0;
}

bool BasicAbstractGame::should_preserve_type_themes(int type) {
    return false;
}

int BasicAbstractGame::mask_theme_if_necessary(int theme, int type) {
    if (options.restrict_themes && !should_preserve_type_themes(type)) return 0;
    return theme;
}

QColor BasicAbstractGame::color_for_type(int type, int theme) {
    QColor color;

    if (options.use_monochrome_assets) {
        theme = mask_theme_if_necessary(theme, type);

        int k = 4;
        int kcubed = k * k * k;
        int chunk = 256 / k;
        fassert(type < kcubed);

        int p1 = 29;
        int p2 = 19;
        // kcubed, p1, and p2 should be relatively prime
        // there will be no type collisons for a fixed theme
        // there will be no theme collisions for a fixed type
        // unique (type, theme) pairs might collide, but this is unlikely to be very relevant
        int new_type = (p1 * (type + 1)) % kcubed;
        new_type = (new_type + p2 * theme) % kcubed;

        color = QColor(chunk * (new_type / (k * k) + 1) - 1, chunk * ((new_type / k) % k + 1) - 1, chunk * (new_type % k + 1) - 1);
    } else {
        fassert(false);
    }

    return color;
}
/*
  Determines whether objects of type target block objects of type src.
*/
bool BasicAbstractGame::is_blocked(const std::shared_ptr<Entity> &src, int target, bool is_horizontal) {
    if (target == WALL_OBJ)
        return true;
    if (target == out_of_bounds_object)
        return true;

    return false;
}

bool BasicAbstractGame::is_blocked_ents(const std::shared_ptr<Entity> &src, const std::shared_ptr<Entity> &target, bool is_horizontal) {
    return is_blocked(src, target->type, is_horizontal);
}

bool BasicAbstractGame::will_reflect(int src, int target) {
    return false;
}

float BasicAbstractGame::get_agent_acceleration_scale() {
    return 1.0;
}

/*
  Create an object of given type and radius contained within the specified box.
  check_collisions (true by default) determines whether the object must not collide with existing objects.
*/

std::shared_ptr<Entity> BasicAbstractGame::spawn_entity_rxy(float rx, float ry, int type, float x, float y, float w, float h, bool check_collisions) {
    std::shared_ptr<Entity> ent(new Entity(0, 0, 0, 0, rx, ry, type));

    reposition(ent, x, y, w, h, check_collisions);

    entities.push_back(ent);

    return ent;
}

bool BasicAbstractGame::agent_has_collision() {
    for (auto ent : entities) {
        if (has_agent_collision(ent)) {
            return true;
        }
    }

    return false;
}

void BasicAbstractGame::reposition_agent() {
    int count = 0;

    do {
        agent->x = rand_gen.rand01() * (main_width - 2 * agent->rx) + agent->rx;
        agent->y = rand_gen.rand01() * (main_height - 2 * agent->ry) + agent->ry;
        count++;
    } while (agent_has_collision() && (count < 100));
}

void BasicAbstractGame::reposition(const std::shared_ptr<Entity> &ent, float x, float y, float w, float h, bool check_collisions) {
    float rx = ent->rx;
    float ry = ent->ry;

    ent->x = rand_pos(rx, x, x + w);
    ent->y = rand_pos(ry, y, y + h);

    int count = 0;

    while ((has_agent_collision(ent) || (check_collisions && has_any_collision(ent))) && (count < 100)) {
        ent->x = rand_pos(rx, x, x + w);
        ent->y = rand_pos(ry, y, y + h);
        count++;
    }

    if (count == 100) {
        printf("WARNING: excessive randomization attempts. Game num, type, rx, ry, w, h: %d %d %f_%f %d %d \n", game_n, ent->type, rx, ry, main_width, main_height);
        printf("Agent: %f %f\n", agent->x, agent->y);
    }
}

std::shared_ptr<Entity> BasicAbstractGame::spawn_entity(float r, int type, float x, float y, float w, float h, bool check_collisions) {
    return spawn_entity_rxy(r, r, type, x, y, w, h, check_collisions);
}

std::shared_ptr<Entity> BasicAbstractGame::add_entity(float x, float y, float vx, float vy, float r, int type) {
    std::shared_ptr<Entity> ent(new Entity(x, y, vx, vy, r, r, type));
    entities.push_back(ent);
    return ent;
}

std::shared_ptr<Entity> BasicAbstractGame::add_entity_rxy(float x, float y, float vx, float vy, float rx, float ry, int type) {
    std::shared_ptr<Entity> ent(new Entity(x, y, vx, vy, rx, ry, type));
    entities.push_back(ent);
    return ent;
}

std::shared_ptr<Entity> BasicAbstractGame::spawn_entity_at_idx(int idx, float r, int type) {
    float x = (idx % main_width) + .5;
    float y = (idx / main_width) + .5;

    auto ent = add_entity(x, y, 0, 0, r, type);

    return ent;
}

void BasicAbstractGame::spawn_entities(int num_entities, float r, int type, float x, float y, float w, float h) {
    for (int i = 0; i < num_entities; i++) {
        spawn_entity(r, type, x, y, w, h);
    }
}

void BasicAbstractGame::basic_step_object(const std::shared_ptr<Entity> &obj) {
    if (obj->will_erase)
        return;

    int num_sub_steps;

    if (grid_step) {
        num_sub_steps = 1;
    } else {
        num_sub_steps = int(4 * sqrt(obj->vx * obj->vx + obj->vy * obj->vy));
        if (num_sub_steps < 4)
            num_sub_steps = 4;
    }

    float pct = 1.0 / num_sub_steps;

    float cmp = fabs(obj->vx) - fabs(obj->vy);

    /*
     Resolve ties randomly -- important for randomized movement through tight corridors in orbeater.
     In order to ensure that enemies choose randomly between horizontal/vertical forks in a path,
     this collision detection shouldn't favor one direction over another.
    */
    bool step_x_first = cmp == 0 ? step_rand_int % 2 == 0 : (cmp > 0);

    // edge case -- needed for player movement through tight corridors (e.g. Pacman)
    if (obj->type == PLAYER) {
        if (action_vx != 0)
            step_x_first = true;
        if (action_vy != 0)
            step_x_first = false;
    }

    float vx_pct = 0;
    float vy_pct = 0;

    for (int s = 0; s < num_sub_steps; s++) {
        bool block_x = false;
        bool block_y = false;

        if (step_x_first) {
            block_x = sub_step(obj, obj->vx * pct, 0, 0);
            block_y = sub_step(obj, 0, obj->vy * pct, 0);
        } else {
            block_y = sub_step(obj, 0, obj->vy * pct, 0);
            block_x = sub_step(obj, obj->vx * pct, 0, 0);
        }

        if (!block_x)
            vx_pct += 1;
        if (!block_y)
            vy_pct += 1;

        if (block_x && block_y) {
            break;
        }
    }

    vx_pct = vx_pct / num_sub_steps;
    vy_pct = vy_pct / num_sub_steps;

    obj->vx *= vx_pct;
    obj->vy *= vy_pct;
}

void BasicAbstractGame::set_action_xy(int move_act) {
    action_vx = move_act / 3 - 1;
    action_vy = move_act % 3 - 1;
    action_vrot = 0;
}

void BasicAbstractGame::choose_center(float &cx, float &cy) {
    cx = agent->x;
    cy = agent->y;
}

void BasicAbstractGame::update_agent_velocity() {
    float v_scale = get_agent_acceleration_scale();

    agent->vx = (1 - mixrate) * agent->vx;
    agent->vy = (1 - mixrate) * agent->vy;

    agent->vx += mixrate * maxspeed * action_vx * v_scale;
    agent->vy += mixrate * maxspeed * action_vy * v_scale;

    decay_agent_velocity();
}

void BasicAbstractGame::decay_agent_velocity() {
    agent->vx = .9 * agent->vx;
    agent->vy = .9 * agent->vy;
}

void BasicAbstractGame::game_step() {
    step_rand_int = rand_gen.randint(0, 1000000);
    move_action = action % 9;
    special_action = 0;

    if (action >= 9) {
        special_action = action - 8;
        move_action = 4; // stand still when taking a special action
    }

    if (move_action != 4) {
        last_move_action = move_action;
    }

    // set reasonable defaults
    action_vrot = 0;
    action_vx = 0;
    action_vy = 0;

    set_action_xy(move_action);

    if (grid_step) {
        agent->vx = action_vx;
        agent->vy = action_vy;
    } else {
        update_agent_velocity();

        agent->vrot = MIXRATEROT * agent->vrot;
        agent->vrot += MIXRATEROT * MAXVTHETA * action_vrot;
    }

    step_entities(entities);

    for (int i = (int)(entities.size()) - 1; i >= 0; i--) {
        auto ent = entities[i];

        if (has_agent_collision(ent)) {
            handle_agent_collision(ent);
        }

        if (ent->collides_with_entities) {
            for (int j = (int)(entities.size()) - 1; j >= 0; j--) {
                if (i == j)
                    continue;
                auto ent2 = entities[j];

                if (has_collision(ent, ent2, ent->collision_margin) && !ent->will_erase && !ent2->will_erase) {
                    handle_collision(ent, ent2);
                }
            }
        }

        if (ent->smart_step) {
            check_grid_collisions(ent);
        }
    }

    erase_if_needed();

    step_data.done = step_data.done || is_out_of_bounds(agent);
}

void BasicAbstractGame::erase_if_needed() {
    for (int i = (int)(entities.size()) - 1; i >= 0; i--) {
        auto e = entities[i];

        if (e->will_erase || (e->auto_erase && is_out_of_bounds(e))) {
            entities.erase(entities.begin() + i);
        }
    }
}

void BasicAbstractGame::game_reset() {
    choose_world_dim();
    fassert(main_width > 0 && main_height > 0);

    bg_pct_x = rand_gen.rand01();

    grid_size = main_width * main_height;
    grid.resize(main_width, main_height);

    background_index = rand_gen.randn((int)(main_bg_images_ptr->size()));

    AssetGen bggen(&rand_gen);

    if (use_procgen_background) {
        bggen.generate_resource(main_bg_images_ptr->at(background_index));
    }

    entities.clear();

    float ax, ay;
    float a_r = 0.4f;

    if (random_agent_start) {
        ax = rand_gen.rand01() * (main_width - 2 * a_r) + a_r;
        ay = rand_gen.rand01() * (main_height - 2 * a_r) + a_r;
    } else {
        ax = a_r;
        ay = a_r;
    }

    auto _agent = std::make_shared<Entity>(ax, ay, 0, 0, a_r, PLAYER);
    agent = _agent;
    agent->smart_step = true;
    agent->render_z = 1;
    entities.push_back(agent);

    erase_if_needed();

    fill_elem(0, 0, main_width, main_height, SPACE);
}

QRectF BasicAbstractGame::get_screen_rect(float x, float y, float dx, float dy, float render_eps) {
    return QRectF((x - render_eps) * unit - x_off, (view_dim - y - render_eps) * unit + y_off, (dx + 2 * render_eps) * unit, (dy + 2 * render_eps) * unit);
}

QRectF BasicAbstractGame::get_abs_rect(float x, float y, float dx, float dy) {
    return QRectF(x * unit, y * unit, dx * unit, dy * unit);
}

QRectF BasicAbstractGame::get_adjusted_image_rect(int type, const QRectF &rect) {
    return rect;
}

QRectF BasicAbstractGame::get_object_rect(const std::shared_ptr<Entity> &obj) {
    if (obj->use_abs_coords) {
        return get_abs_rect(view_dim * (obj->x - obj->rx), view_dim * (obj->y + obj->ry), 2 * view_dim * obj->rx, 2 * view_dim * obj->ry);
    }

    return get_screen_rect(obj->x - obj->rx, obj->y + obj->ry, 2 * obj->rx, 2 * obj->ry);
}

void BasicAbstractGame::prepare_for_drawing(float rect_height) {
    center_x = main_width * .5;
    center_y = main_height * .5;

    if (options.center_agent) {
        choose_center(center_x, center_y);
    } else {
        visibility = main_width > main_height ? main_width : main_height;
        if (visibility < min_visibility)
            visibility = min_visibility;
    }

    float raw_unit = 64 / visibility;
    unit = raw_unit * (rect_height / 64.0);

    view_dim = 64.0 / raw_unit;

    x_off = unit * (center_x - view_dim / 2);
    y_off = unit * (center_y - view_dim / 2);
}

void BasicAbstractGame::tile_image(QPainter &p, QImage *image, const QRectF &rect, float tile_ratio) {
    if (tile_ratio != 0) {
        if (tile_ratio < 0) {
            tile_ratio = -1 * tile_ratio;
            int num_tiles = int(rect.height() / (rect.width() * tile_ratio));
            if (num_tiles < 1)
                num_tiles = 1;
            float tile_height = rect.height() / num_tiles;
            float tile_width = rect.width();

            for (int i = 0; i < num_tiles; i++) {
                QRectF tile_rect = QRectF(rect.x(), rect.y() + tile_height * i, tile_width, tile_height);
                p.drawImage(tile_rect, *image);
            }
        } else {
            int num_tiles = int(rect.width() / (rect.height() * tile_ratio));
            if (num_tiles < 1)
                num_tiles = 1;
            float tile_width = rect.width() / num_tiles;
            float tile_height = rect.height();

            for (int i = 0; i < num_tiles; i++) {
                QRectF tile_rect = QRectF(rect.x() + tile_width * i, rect.y(), tile_width, tile_height);
                p.drawImage(tile_rect, *image);
            }
        }
    } else {
        p.drawImage(rect, *image);
    }
}

QImage *BasicAbstractGame::lookup_asset(int img_idx, bool is_reflected) {
    initialize_asset_if_necessary(img_idx);
    auto assets = is_reflected ? &basic_reflections : &basic_assets;
    return assets->at(img_idx).get();
}

void BasicAbstractGame::draw_image(QPainter &p, QRectF &base_rect, float rotation, bool is_reflected, int base_type, int theme, float alpha, float tile_ratio) {
    int img_type = image_for_type(base_type);

    if (img_type < 0) {
        return;
    }

    if (options.use_monochrome_assets || img_type >= USE_ASSET_THRESHOLD) {
        draw_grid_obj(p, base_rect, img_type, theme);
    } else {
        int img_idx = img_type + theme * MAX_ASSETS;
        fassert(theme < MAX_IMAGE_THEMES);

        QRectF adjusted_rect = get_adjusted_image_rect(img_type, base_rect);

        auto asset_ptr = lookup_asset(img_idx, is_reflected);

        if (alpha != 1) {
            p.save();
            p.setOpacity(alpha);
        }

        if (rotation == 0) {
            tile_image(p, asset_ptr, adjusted_rect, tile_ratio);
        } else {
            p.save();
            p.translate(adjusted_rect.x() + adjusted_rect.width() / 2, adjusted_rect.y() + adjusted_rect.height() / 2);
            p.rotate(rotation * 180 / PI);
            p.drawImage(QRectF(-adjusted_rect.width() / 2, -adjusted_rect.height() / 2, adjusted_rect.width(), adjusted_rect.height()), *asset_ptr);
            p.restore();
        }

        if (alpha != 1) {
            p.restore();
        }
    }
}

void BasicAbstractGame::draw_grid_obj(QPainter &p, const QRectF &rect, int type, int theme) {
    if (type == SPACE)
        return;
    p.fillRect(rect, color_for_type(type, theme));
}

void BasicAbstractGame::draw_foreground(QPainter &p, const QRect &rect) {
    prepare_for_drawing(rect.height());

    draw_entities(p, entities, -1);

    int low_x, high_x, low_y, high_y;

    if (options.center_agent) {
        float margin = (visibility / 2.0 + 1);
        low_x = center_x - margin;
        high_x = center_x + margin;
        low_y = center_y - margin;
        high_y = center_y + margin;
    } else {
        low_x = 0;
        high_x = main_width - 1;
        low_y = 0;
        high_y = main_height - 1;
    }

    for (int x = low_x; x <= high_x; x++) {
        for (int y = low_y; y <= high_y; y++) {
            int type = get_obj(x, y);

            if (type == INVALID_OBJ) {
                continue;
            }

            int theme = theme_for_grid_obj(type);

            QRectF r2 = get_screen_rect(x, y + 1, 1, 1, RENDER_EPS);

            draw_image(p, r2, 0, false, type, theme, 1.0, 0.0);
        }
    }

    draw_entities(p, entities, 0);
    draw_entities(p, entities, 1);

    if (has_useful_vel_info && (options.paint_vel_info)) {
        float infodim = rect.height() * .2;
        QRectF dst2 = QRectF(0, 0, infodim, infodim);
        int s1 = to_shade(.5 * agent->vx / maxspeed + .5);
        int s2 = to_shade(.5 * agent->vy / max_jump + .5);
        p.fillRect(dst2, QColor(s1, s1, s1));

        QRectF dst3 = QRectF(infodim, 0, infodim, infodim);
        p.fillRect(dst3, QColor(s2, s2, s2));
    }
}

void BasicAbstractGame::set_pen_brush_color(QPainter &p, QColor color, int thickness) {
    QBrush brush(color);
    QPen pen(color, thickness);
    p.setBrush(brush);
    p.setPen(pen);
}

void BasicAbstractGame::draw_background(QPainter &p, const QRect &rect) {
    p.fillRect(rect, QColor(0, 0, 0));

    prepare_for_drawing(rect.height());

    if (!options.use_backgrounds) {
        return;
    }

    QRectF main_rect = get_screen_rect(0, main_height, main_width, main_height);

    std::shared_ptr<QImage> background_image = main_bg_images_ptr->at(background_index);

    if (bg_tile_ratio < 0) {
        tile_image(p, background_image.get(), main_rect, bg_tile_ratio);
    } else {
        float bgw = background_image->width();
        float bgh = background_image->height();
        float bg_ar = bgw / bgh;

        float world_ar = main_width * 1.0 / main_height;

        float extra_w = bg_ar - world_ar;
        float offset_x = bg_pct_x * extra_w;

        QRectF bg_rect = adjust_rect(main_rect, QRectF(-offset_x, 0, bg_ar / world_ar, 1));
        p.drawImage(bg_rect, *background_image);
    }
}

void BasicAbstractGame::game_draw(QPainter &p, const QRect &rect) {
    draw_background(p, rect);
    draw_foreground(p, rect);
}

void BasicAbstractGame::match_aspect_ratio(const std::shared_ptr<Entity> &ent, bool match_width) {
    int img_idx = ent->image_type + ent->image_theme * MAX_ASSETS;
    initialize_asset_if_necessary(img_idx);

    if (match_width) {
        ent->ry = ent->rx / asset_aspect_ratios[img_idx];
    } else {
        ent->rx = ent->ry * asset_aspect_ratios[img_idx];
    }
}

void BasicAbstractGame::fit_aspect_ratio(const std::shared_ptr<Entity> &ent) {
    int img_idx = ent->image_type + ent->image_theme * MAX_ASSETS;
    initialize_asset_if_necessary(img_idx);

    float ar = asset_aspect_ratios[img_idx];

    if (ar > 1) {
        ent->ry = ent->rx / ar;
    } else {
        ent->rx = ent->ry * ar;
    }
}

void BasicAbstractGame::choose_random_theme(const std::shared_ptr<Entity> &ent) {
    initialize_asset_if_necessary(ent->image_type);
    ent->image_theme = rand_gen.randn(asset_num_themes[ent->image_type]);
}

void BasicAbstractGame::choose_step_random_theme(const std::shared_ptr<Entity> &ent) {
    initialize_asset_if_necessary(ent->image_type);
    ent->image_theme = step_rand_int % asset_num_themes[ent->image_type];
}

bool BasicAbstractGame::should_draw_entity(const std::shared_ptr<Entity> &entity) {
    return true;
}

void BasicAbstractGame::draw_entity(QPainter &p, const std::shared_ptr<Entity> &ent) {
    if (should_draw_entity(ent)) {
        QRectF r1 = get_object_rect(ent);
        float tile_ratio = get_tile_aspect_ratio(ent);
        draw_image(p, r1, ent->rotation, ent->is_reflected, ent->image_type, ent->image_theme, ent->alpha, tile_ratio);
    }
}

void BasicAbstractGame::draw_entities(QPainter &p, const std::vector<std::shared_ptr<Entity>> &to_draw, int render_z) {
    for (const auto &m : to_draw) {
        if (m->render_z == render_z) {
            draw_entity(p, m);
        }
    }
}

bool BasicAbstractGame::is_out_of_bounds(const std::shared_ptr<Entity> &e1) {
    float x = e1->x;
    float y = e1->y;
    float rx = e1->rx;
    float ry = e1->ry;

    if (x + rx < 0)
        return true;
    if (y + ry < 0)
        return true;
    if (x - rx > main_width)
        return true;
    if (y - ry > main_height)
        return true;

    return false;
}

void BasicAbstractGame::step_entities(const std::vector<std::shared_ptr<Entity>> &given) {
    int entities_count = (int)(given.size());

    for (int i = entities_count - 1; i >= 0; i--) {
        auto ent = given.at(i);

        if (ent->smart_step) {
            basic_step_object(ent);
        }

        ent->step();
    }
}

float BasicAbstractGame::rand_pos(float r, float min, float max) {
    fassert(min <= max);

    if (max - min <= 2 * r)
        return (max + min) / 2;
    float range = max - min;
    fassert(range >= 2 * r);
    return (range - 2 * r) * rand_gen.rand01() + r + min;
}

float BasicAbstractGame::rand_pos(float r, float max) {
    return rand_pos(r, 0, max);
}

bool BasicAbstractGame::has_any_collision(const std::shared_ptr<Entity> &e1, float margin) {
    for (int i = (int)(entities.size()) - 1; i >= 0; i--) {
        auto ent = entities.at(i);

        if (!ent->avoids_collisions && has_collision(e1, ent, margin)) {
            return true;
        }
    }

    return false;
}

bool BasicAbstractGame::has_agent_collision(const std::shared_ptr<Entity> &e1) {
    if (e1->type == PLAYER)
        return false;

    return has_collision(e1, agent, e1->collision_margin);
}

int BasicAbstractGame::find_entity_index(int type) {
    int index = -1;

    for (size_t i = 0; i < entities.size(); i++) {
        if (entities[i]->type == type) {
            index = i;
        }
    }

    return index;
}

bool BasicAbstractGame::has_collision(const std::shared_ptr<Entity> &e1, const std::shared_ptr<Entity> &e2, float margin) {
    float threshold_x = (e1->rx + e2->rx) + margin;
    float threshold_y = (e1->ry + e2->ry) + margin;

    return (fabs(e1->x - e2->x) < threshold_x) && (fabs(e1->y - e2->y) < threshold_y);
}

void BasicAbstractGame::write_entities(WriteBuffer *b, std::vector<std::shared_ptr<Entity>> &ents) {
    b->write_int(ents.size());

    for (size_t i = 0; i < ents.size(); i++) {
        ents[i]->serialize(b);
    }
}

void BasicAbstractGame::read_entities(ReadBuffer *b, std::vector<std::shared_ptr<Entity>> &ents) {
    ents.resize(b->read_int());
    for (size_t i = 0; i < ents.size(); i++) {
        auto e = std::make_shared<Entity>();
        e->deserialize(b);
        ents[i] = e;
    }
}

void BasicAbstractGame::serialize(WriteBuffer *b) {
    Game::serialize(b);

    b->write_int(grid_size);

    write_entities(b, entities);

    fassert(!options.use_generated_assets);
    // these will be cleared and re-generated instead of being saved
//     std::vector<std::shared_ptr<QImage>> basic_assets;
//     std::vector<std::shared_ptr<QImage>> basic_reflections;
//     std::vector<std::shared_ptr<QImage>> *main_bg_images_ptr;

    // std::vector<float> asset_aspect_ratios;
    // std::vector<int> asset_num_themes;

    b->write_int(use_procgen_background);
    b->write_int(background_index);
    b->write_float(bg_tile_ratio);
    b->write_float(bg_pct_x);

    b->write_float(char_dim);
    b->write_int(last_move_action);
    b->write_int(move_action);
    b->write_int(special_action);
    b->write_float(mixrate);
    b->write_float(maxspeed);
    b->write_float(max_jump);

    b->write_float(action_vx);
    b->write_float(action_vy);
    b->write_float(action_vrot);

    b->write_float(center_x);
    b->write_float(center_y);

    b->write_int(random_agent_start);
    b->write_int(has_useful_vel_info);
    b->write_int(step_rand_int);

    asset_rand_gen.serialize(b);

    b->write_int(main_width);
    b->write_int(main_height);
    b->write_int(out_of_bounds_object);

    b->write_float(unit);
    b->write_float(view_dim);
    b->write_float(x_off);
    b->write_float(y_off);
    b->write_float(visibility);
    b->write_float(min_visibility);

    grid.serialize(b);
}

void BasicAbstractGame::deserialize(ReadBuffer *b) {
    Game::deserialize(b);

    grid_size = b->read_int();

    read_entities(b, entities);

    int agent_idx = find_entity_index(PLAYER);
    fassert(agent_idx >= 0);
    agent = entities[agent_idx];

    // we don't want to serialize a bunch of QImages
    // for now we only support games that don't require storing these assets
    fassert(!options.use_generated_assets);

    // when restoring state (to the same game type) with generated assets disabled, these data structures contain cached
    // asset data, and missing data will be filled in the same way in all environments
//     std::vector<std::shared_ptr<QImage>> basic_assets;
//     std::vector<std::shared_ptr<QImage>> basic_reflections;
    // main_bg_images_ptr is set in game_init for all supported games, so it should always be the same
//     std::vector<std::shared_ptr<QImage>> *main_bg_images_ptr;

    // std::vector<float> asset_aspect_ratios;
    // std::vector<int> asset_num_themes;

    use_procgen_background = b->read_int();
    background_index = b->read_int();
    bg_tile_ratio = b->read_float();
    bg_pct_x = b->read_float();

    char_dim = b->read_float();
    last_move_action = b->read_int();
    move_action = b->read_int();
    special_action = b->read_int();
    mixrate = b->read_float();
    maxspeed = b->read_float();
    max_jump = b->read_float();

    action_vx = b->read_float();
    action_vy = b->read_float();
    action_vrot = b->read_float();

    center_x = b->read_float();
    center_y = b->read_float();

    random_agent_start = b->read_int();
    has_useful_vel_info = b->read_int();
    step_rand_int = b->read_int();

    asset_rand_gen.deserialize(b);

    main_width = b->read_int();
    main_height = b->read_int();
    out_of_bounds_object = b->read_int();

    unit = b->read_float();
    view_dim = b->read_float();
    x_off = b->read_float();
    y_off = b->read_float();
    visibility = b->read_float();
    min_visibility = b->read_float();

    grid.deserialize(b);
}
