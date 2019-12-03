#pragma once

/*

This implements the libenv interface and manages a vector of Game instances

*/

#include <memory>
#include <vector>
#include <mutex>
#include <string>
#include <condition_variable>
#include <thread>
#include <list>

class VecOptions;
class Game;

class VecGame {
  public:
    std::vector<struct libenv_space> observation_spaces;
    std::vector<struct libenv_space> action_spaces;
    std::vector<struct libenv_space> render_spaces;
    std::vector<struct libenv_space> info_spaces;

    int num_envs;
    int num_joint_games;
    int num_actions;

    std::vector<std::shared_ptr<Game>> games;

    VecGame(int _nenvs, VecOptions opt_vec);
    ~VecGame();

    void reset(const std::vector<std::vector<void *>> &obs);
    void step_async(const std::vector<int32_t> &acts, const std::vector<std::vector<void *>> &obs, const std::vector<std::vector<void *>> &infos, float *rews, uint8_t *dones);
    void step_wait();
    bool render(const std::string &mode, const std::vector<void *> &arrays);

  private:
    // this mutex synchronizes access to pending_games and game->is_waiting_for_step
    // when game->is_waiting_for_step is set to true
    // ownership of game objects is transferred to the stepping thread until
    // game->is_waiting_for_step is set to false
    std::mutex stepping_thread_mutex;
    std::list<std::shared_ptr<Game>> pending_games;
    std::condition_variable pending_games_added;
    std::condition_variable pending_game_complete;
    std::vector<std::thread> threads;
    bool time_to_die = false;
    bool first_reset = true;
    void wait_for_stepping_threads();
};