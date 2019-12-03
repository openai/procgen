#pragma once

/*

Base class used for all games, all games must inherit from this

*/

#include <QtGui/QPainter>
#include <memory>
#include <functional>
#include <vector>
#include "entity.h"
#include "randgen.h"
#include "resources.h"
#include "object-ids.h"
#include "game-registry.h"

// We want all games to have same observation space. So all these
// constants here related to observation space are constants forever.
const int RES_W = 64;
const int RES_H = 64;

const int RENDER_RES = 512;

void bgr32_to_rgb888(void *dst_rgb888, void *src_bgr32, int w, int h);

class VecOptions;

enum DistributionMode {
    EasyMode = 0,
    HardMode = 1,
    ExtremeMode = 2,
    MemoryMode = 10,
};

struct StepData {
    float reward;
    bool done;
    bool level_complete;
};

struct GameOptions {
    bool paint_vel_info = false;
    bool use_generated_assets = true;
    bool center_agent = false;
    int debug_mode = 0;
    DistributionMode distribution_mode = HardMode;
    bool use_sequential_levels = false;

    // coinrun_old
    bool use_easy_jump = false;
    int plain_assets = 0;
    int physics_mode = 0;
};

class Game {
  public:
    GameOptions options;

    bool grid_step = false;
    int level_seed_low = 0;
    int level_seed_high = 1;
    int game_type;
    int game_n;

    RandGen level_seed_rand_gen;
    RandGen rand_gen;

    StepData step_data;
    int action;

    int timeout;

    int current_level_seed;
    int episodes_remaining;
    bool episode_done;

    float last_ep_reward;
    int last_reward_timer;
    float last_reward;
    int default_action;

    int fixed_asset_seed;

    uint32_t render_buf[RES_W * RES_H];

    int cur_time;

    bool is_waiting_for_step = false;

    // pointers to buffers where we should put step data
    // these are set by step_async
    std::vector<void *> obs_bufs;
    std::vector<void *> info_bufs;
    float *reward_ptr;
    uint8_t *done_ptr;

    Game();
    void step();
    void reset();
    void render_to_buf(void *buf, int w, int h, bool antialias);
    void parse_options(std::string name, VecOptions opt_vec);

    virtual ~Game() = 0;
    virtual void game_init() = 0;
    virtual void game_reset() = 0;
    virtual void game_step() = 0;
    virtual void game_draw(QPainter &p, const QRect &rect) = 0;

  private:
    int reset_count;
    float total_reward;
};
