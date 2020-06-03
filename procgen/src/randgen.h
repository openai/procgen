#pragma once

/*

Random number generator with consistent behavior across platforms

*/

#include "buffer.h"
#include <random>

class RandGen {
  public:
    std::mt19937 stdgen;
    int randint(int low, int high);
    int randn(int high);
    float rand01();
    float randrange(float low, float high);
    int randint();
    bool randbool();
    std::vector<int> partition(int x, int n);
    int choose_one(std::vector<int> &elems);
    std::vector<int> choose_n(const std::vector<int> &elems, int n);
    std::vector<int> simple_choose(int n, int k);
    void seed(int seed);
    void serialize(WriteBuffer *b);
    void deserialize(ReadBuffer *b);
  private:
    bool is_seeded = false;
};
