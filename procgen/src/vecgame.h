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
    std::vector<struct libenv_tensortype> observation_types;
    std::vector<struct libenv_tensortype> action_types;
    std::vector<struct libenv_tensortype> info_types;

    int num_envs;
    int num_joint_games;
    int num_actions;
    bool render_human;

    std::vector<std::shared_ptr<Game>> games;

    VecGame(int _nenvs, VecOptions opt_vec);
    ~VecGame();

    void set_buffers(const std::vector<std::vector<void *>> &ac, const std::vector<std::vector<void *>> &ob, const std::vector<std::vector<void *>> &info, float *rew, uint8_t *first);
    void observe();
    void act();
    void wait_for_stepping_threads();

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
};
