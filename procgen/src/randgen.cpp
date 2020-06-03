#include "randgen.h"
#include "cpp-utils.h"
#include <set>
#include <sstream>

int RandGen::randint(int low, int high) {
    fassert(is_seeded);
    uint32_t x = stdgen();
    uint32_t range = high - low;
    return low + (x % range);
}

int RandGen::randn(int high) {
    fassert(is_seeded);
    uint32_t x = stdgen();
    return (x % high);
}

float RandGen::rand01() {
    fassert(is_seeded);
    uint32_t x = stdgen();
    return (float)((double)(x) / ((double)(stdgen.max()) + 1));
}

bool RandGen::randbool() {
    return rand01() > .5;
}

float RandGen::randrange(float low, float high) {
    return rand01() * (high - low) + low;
}

std::vector<int> RandGen::partition(int x, int n) {
    std::vector<int> partition(n, 0);

    for (int i = 0; i < x; i++) {
        partition[randn(n)] += 1;
    }

    return partition;
}

int RandGen::choose_one(std::vector<int> &elems) {
    fassert(elems.size() > 0);

    return elems[randn((int)(elems.size()))];
}

std::vector<int> RandGen::choose_n(const std::vector<int> &elems, int n) {
    std::vector<int> chosen;
    std::vector<int> rem_elems;

    for (int elem : elems) {
        rem_elems.push_back(elem);
    }

    if (n > (int)(elems.size())) {
        return rem_elems;
    }

    while (int(chosen.size()) < n) {
        int next_elem_idx = randn((int)(rem_elems.size()));
        chosen.push_back(rem_elems[next_elem_idx]);
        rem_elems.erase(rem_elems.begin() + next_elem_idx);
    }

    return chosen;
}

std::vector<int> RandGen::simple_choose(int n, int k) {
    std::vector<int> chosen(k, 0);
    std::set<int> set;

    fassert(k <= n);

    for (int i = 0; i < k; i++) {
        int next = randn(n);

        while (set.find(next) != set.end()) {
            next = randn(n);
        }

        chosen[i] = next;
        set.insert(next);
    }

    return chosen;
}

int RandGen::randint() {
    fassert(is_seeded);
    return stdgen();
}

void RandGen::seed(int seed) {
    stdgen.seed(seed);
    is_seeded = true;
}

void RandGen::serialize(WriteBuffer *b) {
    b->write_int(is_seeded);
    std::ostringstream ostream;
    ostream << stdgen;
    auto str = ostream.str();
    b->write_string(str);
}

void RandGen::deserialize(ReadBuffer *b) {
    is_seeded = b->read_int();
    auto str = b->read_string();
    std::istringstream istream;
    istream.str(str);
    istream >> stdgen;
}
