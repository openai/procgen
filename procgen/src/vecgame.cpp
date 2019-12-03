#include "vecgame.h"
#include "cpp-utils.h"
#include "vecoptions.h"
#include "game.h"

extern void coinrun_old_init(int rand_seed);

static std::once_flag global_init_flag;

std::vector<std::string> split(std::string s, std::string delimiter) {
    std::vector<std::string> env_names;

    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        env_names.push_back(token);
        s.erase(0, pos + delimiter.length());
    }
    env_names.push_back(s);

    return env_names;
}

// libenv api

// convert_bufs reorganizes buffers so that they are indexed by the environment
std::vector<std::vector<void *>> convert_bufs(void **bufs, int num_envs,
                                              size_t space_count) {
    auto result = std::vector<std::vector<void *>>(num_envs);
    for (int env_idx = 0; env_idx < num_envs; env_idx++) {
        result[env_idx].resize(space_count);
        for (size_t space_idx = 0; space_idx < space_count; space_idx++) {
            result[env_idx][space_idx] = bufs[space_idx * num_envs + env_idx];
        }
    }
    return result;
}

extern "C" {
libenv_venv *libenv_make(int num_envs, const struct libenv_options options) {
    auto venv = new VecGame(num_envs, VecOptions(options));
    return (libenv_venv *)(venv);
}

int libenv_get_spaces(libenv_venv *env, enum libenv_spaces_name name,
                      struct libenv_space *out_spaces) {
    auto venv = (VecGame *)(env);
    std::vector<struct libenv_space> in_spaces;
    if (name == LIBENV_SPACES_OBSERVATION) {
        in_spaces = venv->observation_spaces;
    } else if (name == LIBENV_SPACES_ACTION) {
        in_spaces = venv->action_spaces;
        fassert(in_spaces.size() == 1);
        fassert(in_spaces[0].dtype == LIBENV_DTYPE_INT32);
    } else if (name == LIBENV_SPACES_RENDER) {
        in_spaces = venv->render_spaces;
    } else if (name == LIBENV_SPACES_INFO) {
        in_spaces = venv->info_spaces;
    } else {
        return 0;
    }
    if (out_spaces != nullptr) {
        for (size_t i = 0; i < in_spaces.size(); i++) {
            out_spaces[i] = in_spaces[i];
        }
    }
    return (int)(in_spaces.size());
}

void libenv_reset(libenv_venv *env, struct libenv_step *step) {
    auto venv = (VecGame *)(env);
    auto obs = convert_bufs(step->obs, venv->num_envs,
                            venv->observation_spaces.size());
    venv->reset(obs);
}

void libenv_step_async(libenv_venv *env, const void **acts,
                       struct libenv_step *step) {
    auto venv = (VecGame *)(env);
    // we only ever use a single discrete action per env
    auto vec_acts = std::vector<int32_t>(venv->num_envs);
    for (size_t env_idx = 0; env_idx < vec_acts.size(); env_idx++) {
        vec_acts[env_idx] = *(const int32_t *)(acts[env_idx]);
    }
    auto obs = convert_bufs(step->obs, venv->num_envs,
                            venv->observation_spaces.size());
    auto infos =
        convert_bufs(step->infos, venv->num_envs, venv->info_spaces.size());
    venv->step_async(vec_acts, obs, infos, step->rews, step->dones);
}

void libenv_step_wait(libenv_venv *env) {
    auto venv = (VecGame *)(env);
    venv->step_wait();
}

bool libenv_render(libenv_venv *env, const char *mode, void **frames) {
    auto venv = (VecGame *)(env);
    std::vector<void *> arrays(frames, frames + venv->num_envs);
    return venv->render(std::string(mode), arrays);
}

void libenv_close(libenv_venv *env) {
    auto venv = (VecGame *)(env);
    delete venv;
}
}

// end libenv api

static void stepping_worker(std::mutex& stepping_thread_mutex, 
            std::list<std::shared_ptr<Game>>& pending_games,
            std::condition_variable& pending_games_added,
            std::condition_variable& pending_game_complete, bool& time_to_die) {
    while (1) {
        std::shared_ptr<Game> game;

        {
            std::unique_lock<std::mutex> lock(stepping_thread_mutex);
            while (1) {
                if (time_to_die) {
                    return;
                }
                if (!pending_games.empty()) {
                    game = pending_games.front();
                    pending_games.pop_front();
                    break;
                }

                pending_games_added.wait(lock);
            }
        }

        game->step();

        {
            std::unique_lock<std::mutex> lock(stepping_thread_mutex);
            game->is_waiting_for_step = false;
            pending_game_complete.notify_all();
        }
    }
}

void global_init(int rand_seed, std::string resource_root) {
    global_resource_root = resource_root.c_str();

    try {
        images_load();
        coinrun_old_init(rand_seed);
    } catch (const std::exception &e) {
        fatal("failed to load images %s\n", e.what());
    }
}

// we want system independent hashing. std::hash doesn't give this.
inline uint32_t hash_str_uint32(const std::string &str) {
    uint32_t hash = 0x811c9dc5;
    uint32_t prime = 0x1000193;

    for (size_t i = 0; i < str.size(); i++) {
        uint8_t value = str[i];
        hash = hash ^ value;
        hash *= prime;
    }

    return hash;
}

VecGame::VecGame(int _nenvs, VecOptions opts) {
    num_envs = _nenvs;
    games.resize(num_envs);
    std::string env_name;

    int num_levels = 0;
    int start_level = -1;
    num_actions = -1;

    int rand_seed = 0;
    int num_threads = 4;
    std::string resource_root;

    opts.consume_string("env_name", &env_name);
    opts.consume_int("num_levels", &num_levels);
    opts.consume_int("start_level", &start_level);
    opts.consume_int("num_actions", &num_actions);
    opts.consume_int("rand_seed", &rand_seed);
    opts.consume_int("num_threads", &num_threads);
    opts.consume_string("resource_root", &resource_root);

    std::call_once(global_init_flag, global_init, rand_seed,
                   resource_root);

    threads.resize(num_threads);
    for (int t = 0; t < num_threads; t++) {
        threads[t] = std::thread(
            stepping_worker,
            std::ref(stepping_thread_mutex),
            std::ref(pending_games),
            std::ref(pending_games_added),
            std::ref(pending_game_complete),
            std::ref(time_to_die));
    }

    fassert(env_name != "");
    fassert(num_actions > 0);
    fassert(num_levels >= 0);
    fassert(start_level >= 0);

    int level_seed_low = 0;
    int level_seed_high = 0;

    if (num_levels == 0) {
        level_seed_low = 0;
        level_seed_high = INT32_MAX;
    } else if (num_levels > 0) {
        level_seed_low = start_level;
        level_seed_high = start_level + num_levels;
    }

    std::vector<std::string> env_names = split(env_name, ",");

    num_joint_games = (int)(env_names.size());

    fassert(num_envs % num_joint_games == 0);

    RandGen game_level_seed_gen;
    game_level_seed_gen.seed(rand_seed);

    for (int n = 0; n < num_envs; n++) {
        auto name = env_names[n % num_joint_games];

        games[n] = globalGameRegistry->at(name)();
        games[n]->level_seed_rand_gen.seed(game_level_seed_gen.randint());
        games[n]->level_seed_high = level_seed_high;
        games[n]->level_seed_low = level_seed_low;
        games[n]->game_n = n;
        games[n]->is_waiting_for_step = false;
        games[n]->parse_options(name, opts);

        // Auto-selected a fixed_asset_seed if one wasn't specified on
        // construction
        if (games[n]->fixed_asset_seed == 0) {
            auto hashed = hash_str_uint32(name);
            games[n]->fixed_asset_seed = int(hashed);
        }

        games[n]->game_init();
        games[n]->reset();
    }

    {
        struct libenv_space s;
        strcpy(s.name, "rgb");
        s.type = LIBENV_SPACE_TYPE_BOX;
        s.dtype = LIBENV_DTYPE_UINT8;
        s.shape[0] = RES_W;
        s.shape[1] = RES_H;
        s.shape[2] = 3;
        s.ndim = 3;
        s.low.uint8 = 0;
        s.high.uint8 = 255;
        observation_spaces.push_back(s);
    }

    {
        struct libenv_space s;
        strcpy(s.name, "action");
        s.type = LIBENV_SPACE_TYPE_DISCRETE;
        s.dtype = LIBENV_DTYPE_INT32;
        s.shape[0] = 1;
        s.ndim = 1;
        s.low.int32 = 0;
        s.high.int32 = num_actions - 1;
        action_spaces.push_back(s);
    }

    {
        struct libenv_space s;
        strcpy(s.name, "level_seed");
        s.type = LIBENV_SPACE_TYPE_BOX;
        s.dtype = LIBENV_DTYPE_INT32;
        s.shape[0] = 1;
        s.ndim = 1,
        s.low.int32 = 0;
        s.high.int32 = INT32_MAX;
        info_spaces.push_back(s);
    }

    {
        struct libenv_space s;
        strcpy(s.name, "level_complete");
        s.type = LIBENV_SPACE_TYPE_DISCRETE;
        s.dtype = LIBENV_DTYPE_UINT8;
        s.shape[0] = 1;
        s.ndim = 1,
        s.low.int32 = 0;
        s.high.int32 = 1;
        info_spaces.push_back(s);
    }

    {
        struct libenv_space s;
        strcpy(s.name, "rgb_array");
        s.type = LIBENV_SPACE_TYPE_BOX;
        s.dtype = LIBENV_DTYPE_UINT8;
        s.shape[0] = RENDER_RES;
        s.shape[1] = RENDER_RES;
        s.shape[2] = 3;
        s.ndim = 3,
        s.low.uint8 = 0;
        s.high.uint8 = 255;
        render_spaces.push_back(s);
    }
}

void VecGame::reset(const std::vector<std::vector<void *>> &obs) {
    if (!first_reset) {
        printf("WARNING: Procgen ignores resets, please create a new environment "
               "instead\n");
    }
    first_reset = false;
    wait_for_stepping_threads();
    for (int e = 0; e < num_envs; e++) {
        const auto &game = games[e];
        game->render_to_buf(game->render_buf, RES_W, RES_H, false);
        bgr32_to_rgb888(obs[e][0], game->render_buf, RES_W, RES_H);
    }
}

void VecGame::step_async(const std::vector<int32_t> &acts,
                         const std::vector<std::vector<void *>> &obs,
                         const std::vector<std::vector<void *>> &infos,
                         float *rews, uint8_t *dones) {
    // this function should never be called until after creation/step_wait()
    // so at this point, we can be certain that no games are waiting for steps
    // and that all games belong to the python thread
    {
        std::unique_lock<std::mutex> lock(stepping_thread_mutex);

        for (int e = 0; e < num_envs; e++) {
            const auto &game = games[e];
            game->action = acts[e];
            game->obs_bufs = obs[e];
            game->info_bufs = infos[e];
            game->reward_ptr = &rews[e];
            game->done_ptr = &dones[e];
            fassert(!game->is_waiting_for_step);
            game->is_waiting_for_step = true;
            pending_games.push_back(game);
        }
    }
    // at this point all games belong to the stepping threads

    pending_games_added.notify_all();
}

VecGame::~VecGame() {
    wait_for_stepping_threads();
    {
        std::unique_lock<std::mutex> lock(stepping_thread_mutex);
        time_to_die = true;
    }
    pending_games_added.notify_all();

    for (auto &t : threads) {
        t.join();
    }
}

void VecGame::wait_for_stepping_threads() {
    std::unique_lock<std::mutex> lock(stepping_thread_mutex);
    while (1) {
        bool all_steps_completed = true;

        for (int e = 0; e < num_envs; e++) {
            const auto &game = games[e];
            all_steps_completed &= !game->is_waiting_for_step;
        }

        if (all_steps_completed)
            break;

        pending_game_complete.wait(lock);
    }
}

void VecGame::step_wait() {
    wait_for_stepping_threads();
    // at this point all games belong to the python thread
}

bool VecGame::render(const std::string &mode,
                     const std::vector<void *> &arrays) {
    uint8_t render_hires_buf[RENDER_RES * RENDER_RES * 4];

    for (int e = 0; e < num_envs; e++) {
        const auto &game = games[e];
        game->render_to_buf(render_hires_buf, RENDER_RES, RENDER_RES, true);
        bgr32_to_rgb888(arrays[e], render_hires_buf, RENDER_RES, RENDER_RES);
    }
    return true;
}
