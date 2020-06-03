#include "vecgame.h"
#include "cpp-utils.h"
#include "vecoptions.h"
#include "game.h"

const int32_t END_OF_BUFFER = 0xCAFECAFE;

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
int libenv_version() {
    return LIBENV_VERSION;
}

libenv_env *libenv_make(int num_envs, const struct libenv_options options) {
    auto venv = new VecGame(num_envs, VecOptions(options));
    return (libenv_env *)(venv);
}

int libenv_get_tensortypes(libenv_env *handle, enum libenv_space_name name, struct libenv_tensortype *out_types) {
    auto venv = (VecGame *)(handle);
    std::vector<struct libenv_tensortype> types;
    if (name == LIBENV_SPACE_OBSERVATION) {
        types = venv->observation_types;
    } else if (name == LIBENV_SPACE_ACTION) {
        types = venv->action_types;
        fassert(types.size() == 1);
        fassert(types[0].dtype == LIBENV_DTYPE_INT32);
    } else if (name == LIBENV_SPACE_INFO) {
        types = venv->info_types;
    } else {
        return 0;
    }
    if (out_types != nullptr) {
        for (size_t i = 0; i < types.size(); i++) {
            out_types[i] = types[i];
        }
    }
    return (int)(types.size());
}

void libenv_set_buffers(libenv_env *handle, struct libenv_buffers *bufs) {
    auto venv = (VecGame *)(handle);
    auto ac = convert_bufs(bufs->ac, venv->num_envs,
                           venv->action_types.size());
    auto ob = convert_bufs(bufs->ob, venv->num_envs,
                           venv->observation_types.size());
    auto info =
        convert_bufs(bufs->info, venv->num_envs, venv->info_types.size());
    venv->set_buffers(ac, ob, info, bufs->rew, bufs->first);
}

void libenv_observe(libenv_env *handle) {
    auto venv = (VecGame *)(handle);
    venv->observe();
}

void libenv_act(libenv_env *handle) {
    auto venv = (VecGame *)(handle);
    venv->act();
}

void libenv_close(libenv_env *handle) {
    auto venv = (VecGame *)(handle);
    delete venv;
}
}

// end libenv api

static void stepping_worker(std::mutex &stepping_thread_mutex,
                            std::list<std::shared_ptr<Game>> &pending_games,
                            std::condition_variable &pending_games_added,
                            std::condition_variable &pending_game_complete, bool &time_to_die) {
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

        // the first time the threads are activated is before any step, just to initialize
        // the environment and produce the initial observation
        if (!game->initial_reset_complete) {
            game->reset();
            game->observe();
            game->initial_reset_complete = true;
        } else{
            game->step();
        }

        {
            std::unique_lock<std::mutex> lock(stepping_thread_mutex);
            game->is_waiting_for_step = false;
            pending_game_complete.notify_all();
        }
    }
}

void global_init(int rand_seed, std::string resource_root) {
    global_resource_root = resource_root;

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
    render_human = false;
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
    opts.consume_bool("render_human", &render_human);

    std::call_once(global_init_flag, global_init, rand_seed,
                   resource_root);

    fassert(num_threads >= 0);
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

    {
        struct libenv_tensortype s;
        strcpy(s.name, "rgb");
        s.scalar_type = LIBENV_SCALAR_TYPE_DISCRETE;
        s.dtype = LIBENV_DTYPE_UINT8;
        s.shape[0] = RES_W;
        s.shape[1] = RES_H;
        s.shape[2] = 3;
        s.ndim = 3;
        s.low.uint8 = 0;
        s.high.uint8 = 255;
        observation_types.push_back(s);
    }

    {
        struct libenv_tensortype s;
        strcpy(s.name, "action");
        s.scalar_type = LIBENV_SCALAR_TYPE_DISCRETE;
        s.dtype = LIBENV_DTYPE_INT32;
        s.ndim = 0;
        s.low.int32 = 0;
        s.high.int32 = num_actions - 1;
        action_types.push_back(s);
    }

    {
        struct libenv_tensortype s;
        strcpy(s.name, "prev_level_seed");
        s.scalar_type = LIBENV_SCALAR_TYPE_DISCRETE;
        s.dtype = LIBENV_DTYPE_INT32;
        s.ndim = 0,
        s.low.int32 = 0;
        s.high.int32 = INT32_MAX;
        info_types.push_back(s);
    }

    {
        struct libenv_tensortype s;
        strcpy(s.name, "prev_level_complete");
        s.scalar_type = LIBENV_SCALAR_TYPE_DISCRETE;
        s.dtype = LIBENV_DTYPE_UINT8;
        s.ndim = 0,
        s.low.uint8 = 0;
        s.high.uint8 = 1;
        info_types.push_back(s);
    }

    {
        struct libenv_tensortype s;
        strcpy(s.name, "level_seed");
        s.scalar_type = LIBENV_SCALAR_TYPE_DISCRETE;
        s.dtype = LIBENV_DTYPE_INT32;
        s.ndim = 0,
        s.low.int32 = 0;
        s.high.int32 = INT32_MAX;
        info_types.push_back(s);
    }
    
    if (render_human) {
        struct libenv_tensortype s;
        strcpy(s.name, "rgb");
        s.scalar_type = LIBENV_SCALAR_TYPE_DISCRETE;
        s.dtype = LIBENV_DTYPE_UINT8;
        s.shape[0] = RENDER_RES;
        s.shape[1] = RENDER_RES;
        s.shape[2] = 3;
        s.ndim = 3,
        s.low.uint8 = 0;
        s.high.uint8 = 255;
        info_types.push_back(s);
    }

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

    std::map<std::string, int> info_name_to_offset;
    for (size_t i = 0; i < info_types.size(); i++) {
        info_name_to_offset[info_types[i].name] = i;
    }

    for (int n = 0; n < num_envs; n++) {
        auto name = env_names[n % num_joint_games];

        games[n] = globalGameRegistry->at(name)();
        fassert(games[n]->game_name == name);
        games[n]->level_seed_rand_gen.seed(game_level_seed_gen.randint());
        games[n]->level_seed_high = level_seed_high;
        games[n]->level_seed_low = level_seed_low;
        games[n]->game_n = n;
        games[n]->is_waiting_for_step = false;
        games[n]->parse_options(name, opts);
        games[n]->info_name_to_offset = info_name_to_offset;

        // Auto-selected a fixed_asset_seed if one wasn't specified on
        // construction
        if (games[n]->fixed_asset_seed == 0) {
            auto hashed = hash_str_uint32(name);
            games[n]->fixed_asset_seed = int(hashed);
        }

        games[n]->game_init();
    }
}

void VecGame::set_buffers(const std::vector<std::vector<void *>> &ac, const std::vector<std::vector<void *>> &ob, const std::vector<std::vector<void *>> &info, float *rew, uint8_t *first) {
    {
        std::unique_lock<std::mutex> lock(stepping_thread_mutex);

        for (int e = 0; e < num_envs; e++) {
            const auto &game = games[e];
            // we only ever have one action
            game->action_ptr = (int32_t *)(ac[e][0]);
            game->obs_bufs = ob[e];
            game->info_bufs = info[e];
            game->reward_ptr = &rew[e];
            game->first_ptr = &first[e];
            
            // render the initial state so we don't see a black screen on the first frame
            fassert(!game->is_waiting_for_step);
            fassert(!game->initial_reset_complete);
            if (threads.size() == 0) {
                // special case for no threads
                game->reset();
                game->observe();
                game->initial_reset_complete = true;
            } else {
                game->is_waiting_for_step = true;
                pending_games.push_back(game);
            }
        }
    }
    pending_games_added.notify_all();
}

void VecGame::observe() {
    wait_for_stepping_threads();
    // at this point all games belong to the python thread

    if (render_human) {
        uint8_t render_hires_buf[RENDER_RES * RENDER_RES * 4];

        for (int e = 0; e < num_envs; e++) {
            const auto &game = games[e];
            game->render_to_buf(render_hires_buf, RENDER_RES, RENDER_RES, true);
            bgr32_to_rgb888(game->info_bufs[game->info_name_to_offset.at("rgb")], render_hires_buf, RENDER_RES, RENDER_RES);
        }
    }
}

void VecGame::act() {
    wait_for_stepping_threads();

    {
        std::unique_lock<std::mutex> lock(stepping_thread_mutex);

        for (int e = 0; e < num_envs; e++) {
            const auto &game = games[e];
            fassert(!game->is_waiting_for_step);
            // save the action since it's only valid for the duration of this call
            game->action = *game->action_ptr;
            if (threads.size() == 0) {
                // special case for no threads
                game->step();
            } else {
                game->is_waiting_for_step = true;
                pending_games.push_back(game);
            }
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
    if (threads.size() == 0) {
        return;
    }

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

extern "C" {
    LIBENV_API int get_state(libenv_env *handle, int env_idx, char *data, int length) {
        auto venv = (VecGame *)(handle);
        venv->wait_for_stepping_threads();
        auto b = WriteBuffer(data, length);
        venv->games.at(env_idx)->serialize(&b);
        b.write_int(END_OF_BUFFER);
        return b.offset;
    }

    LIBENV_API void set_state(libenv_env *handle, int env_idx, char *data, int length) {
        auto venv = (VecGame *)(handle);
        venv->wait_for_stepping_threads();
        auto b = ReadBuffer(data, length);
        venv->games.at(env_idx)->deserialize(&b);
        fassert(b.read_int() == END_OF_BUFFER);
        // after deserializing, we need to update the observation and info buffers so that the
        // next time VecGame::observe() is called, the correct data will be in the buffers
        venv->games.at(env_idx)->observe();
    }
}