
#include "game.h"
#include "vecoptions.h"

// this should be updated whenever the state format or environments may have changed
const int SERIALIZE_VERSION = 0;

void bgr32_to_rgb888(void *dst_rgb888, void *src_bgr32, int w, int h) {
    uint8_t *src = (uint8_t *)src_bgr32;
    uint8_t *dst = (uint8_t *)dst_rgb888;

    for (int y = 0; y < h; y++) {
        uint8_t *s = src + y * w * 4;
        uint8_t *d = dst + y * w * 3;
        for (int x = 0; x < w; x++) {
            d[0] = s[2];
            d[1] = s[1];
            d[2] = s[0];
            s += 4;
            d += 3;
        }
    }
}

Game::Game(std::string name) : game_name(name) {
    timeout = 1000;
    episodes_remaining = 0;
    last_reward = -1;
    default_action = 0;
    fixed_asset_seed = 0;
    reset_count = 0;
    current_level_seed = 0;

    step_data.reward = 0;
    step_data.done = true;
    step_data.level_complete = false;
}

Game::~Game() {
}

void Game::parse_options(std::string name, VecOptions opts) {
    opts.consume_bool("use_easy_jump", &options.use_easy_jump);
    opts.consume_bool("paint_vel_info", &options.paint_vel_info);
    opts.consume_bool("use_generated_assets", &options.use_generated_assets);
    opts.consume_bool("use_monochrome_assets", &options.use_monochrome_assets);
    opts.consume_bool("restrict_themes", &options.restrict_themes);
    opts.consume_bool("use_backgrounds", &options.use_backgrounds);
    opts.consume_bool("center_agent", &options.center_agent);
    opts.consume_bool("use_sequential_levels", &options.use_sequential_levels);

    int dist_mode = EasyMode;
    opts.consume_int("distribution_mode", &dist_mode);
    options.distribution_mode = static_cast<DistributionMode>(dist_mode);

    if (options.distribution_mode == EasyMode) {
        fassert(name != "coinrun_old");
    } else if (options.distribution_mode == HardMode) {
        // all environments support this mode
    } else if (options.distribution_mode == ExtremeMode) {
        fassert(name == "chaser" || name == "dodgeball" || name == "leaper" || name == "starpilot");
    } else if (options.distribution_mode == MemoryMode) {
        fassert(name == "caveflyer" || name == "dodgeball" || name == "heist" || name == "jumper" || name == "maze" || name == "miner");
    } else {
        fatal("invalid distribution_mode %d\n", options.distribution_mode);
    }

    // coinrun_old
    opts.consume_int("plain_assets", &options.plain_assets);
    opts.consume_int("physics_mode", &options.physics_mode);
    opts.consume_int("debug_mode", &options.debug_mode);
    opts.consume_int("game_type", &game_type);

    opts.ensure_empty();
}

void Game::render_to_buf(void *dst, int w, int h, bool antialias) {
    // Qt focuses on RGB32 performance:
    // https://doc.qt.io/qt-5/qpainter.html#performance
    // so render to an RGB32 buffer and then convert it rather than render to RGB888 directly
    QImage img((uchar *)dst, w, h, w * 4, QImage::Format_RGB32);
    QPainter p(&img);

    if (antialias) {
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    }

    QRect rect = QRect(0, 0, w, h);
    game_draw(p, rect);
}

void Game::reset() {
    reset_count++;

    if (episodes_remaining == 0) {
        if (options.use_sequential_levels && step_data.level_complete) {
            // prevent overflow in seed sequences
            current_level_seed = (int32_t)(current_level_seed + 997);
        } else {
            current_level_seed = level_seed_rand_gen.randint(level_seed_low, level_seed_high);
        }

        episodes_remaining = 1;
    } else {
        step_data.reward = 0;
        step_data.done = false;
        step_data.level_complete = false;
    }

    rand_gen.seed(current_level_seed);
    game_reset();

    cur_time = 0;
    total_reward = 0;
    episodes_remaining -= 1;
    action = default_action;
}

void Game::step() {
    cur_time += 1;
    bool will_force_reset = false;

    if (action == -1) {
        action = default_action;
        will_force_reset = true;
    }

    step_data.reward = 0;
    step_data.done = false;
    step_data.level_complete = false;
    game_step();

    step_data.done = step_data.done || will_force_reset || (cur_time >= timeout);
    total_reward += step_data.reward;

    if (step_data.reward != 0) {
        last_reward_timer = 10;
        last_reward = step_data.reward;
    }

    prev_level_seed = current_level_seed;

    if (step_data.done) {
        reset();
    }

    if (options.use_sequential_levels && step_data.level_complete) {
        step_data.done = false;
    }

    episode_done = step_data.done;

    observe();
}

void Game::observe() {
    render_to_buf(render_buf, RES_W, RES_H, false);
    bgr32_to_rgb888(obs_bufs[0], render_buf, RES_W, RES_H);
    *reward_ptr = step_data.reward;
    *first_ptr = (uint8_t)step_data.done;
    *(int32_t *)(info_bufs[info_name_to_offset.at("prev_level_seed")]) = (int32_t)(prev_level_seed);
    *(uint8_t *)(info_bufs[info_name_to_offset.at("prev_level_complete")]) = (uint8_t)(step_data.level_complete);
    *(int32_t *)(info_bufs[info_name_to_offset.at("level_seed")]) = (int32_t)(current_level_seed);
}

void Game::game_init() {
}

void Game::serialize(WriteBuffer *b) {
    b->write_int(SERIALIZE_VERSION);
    
    b->write_string(game_name);

    b->write_int(options.paint_vel_info);
    b->write_int(options.use_generated_assets);
    b->write_int(options.use_monochrome_assets);
    b->write_int(options.restrict_themes);
    b->write_int(options.use_backgrounds);
    b->write_int(options.center_agent);
    b->write_int(options.debug_mode);
    b->write_int(options.distribution_mode);
    b->write_int(options.use_sequential_levels);

    b->write_int(options.use_easy_jump);
    b->write_int(options.plain_assets);
    b->write_int(options.physics_mode);

    b->write_int(grid_step);
    b->write_int(level_seed_low);
    b->write_int(level_seed_high);
    b->write_int(game_type);
    b->write_int(game_n);

    level_seed_rand_gen.serialize(b);
    rand_gen.serialize(b);

    b->write_float(step_data.reward);
    b->write_int(step_data.done);
    b->write_int(step_data.level_complete);

    b->write_int(action);
    b->write_int(timeout);

    b->write_int(current_level_seed);
    b->write_int(prev_level_seed);
    b->write_int(episodes_remaining);
    b->write_int(episode_done);

    b->write_int(last_reward_timer);
    b->write_float(last_reward);
    b->write_int(default_action);

    b->write_int(fixed_asset_seed);

    // don't save render buf as we will just re-write it on next observation
    // uint32_t render_buf[RES_W * RES_H];

    b->write_int(cur_time);
    b->write_int(is_waiting_for_step);

    // don't serialize these, since they are pointers, and will likely have incorrect values
    // if deserialized into another game object
    // int32_t *action_ptr;
    // std::vector<void *> obs_bufs;
    // std::vector<void *> info_bufs;
    // float *reward_ptr = nullptr;
    // uint8_t *first_ptr = nullptr;
}

void Game::deserialize(ReadBuffer *b) {
    fassert(SERIALIZE_VERSION == b->read_int());
    fassert(game_name == b->read_string());

    options.paint_vel_info = b->read_int();
    options.use_generated_assets = b->read_int();
    options.use_monochrome_assets = b->read_int();
    options.restrict_themes = b->read_int();
    options.use_backgrounds = b->read_int();
    options.center_agent = b->read_int();
    options.debug_mode = b->read_int();
    options.distribution_mode = DistributionMode(b->read_int());
    options.use_sequential_levels = b->read_int();

    options.use_easy_jump = b->read_int();
    options.plain_assets = b->read_int();
    options.physics_mode = b->read_int();

    grid_step = b->read_int();
    level_seed_low = b->read_int();
    level_seed_high = b->read_int();
    game_type = b->read_int();
    game_n = b->read_int();

    level_seed_rand_gen.deserialize(b);
    rand_gen.deserialize(b);

    step_data.reward = b->read_float();
    step_data.done = b->read_int();
    step_data.level_complete = b->read_int();

    action = b->read_int();
    timeout = b->read_int();

    current_level_seed = b->read_int();
    prev_level_seed = b->read_int();
    episodes_remaining = b->read_int();
    episode_done = b->read_int();

    last_reward_timer = b->read_int();
    last_reward = b->read_float();
    default_action = b->read_int();

    fixed_asset_seed = b->read_int();

    cur_time = b->read_int();
    is_waiting_for_step = b->read_int();
}
