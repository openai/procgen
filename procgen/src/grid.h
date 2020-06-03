#pragma once

/*

Simple utility class for managing a grid of objects

*/

#include <vector>
#include "cpp-utils.h"
#include "buffer.h"

template <typename T>
class Grid {
  public:
    int w;
    int h;
    std::vector<T> data;

    Grid() {
        w = 0;
        h = 0;
    }

    void resize(int width, int height) {
        w = width;
        h = height;
        data.clear();
        data.resize(width * height);
    }

    bool contains(int x, int y) const {
        return 0 <= y && y < h && 0 <= x && x < w;
    };

    bool contains_index(int idx) const {
        return 0 <= idx && idx < w * h;
    };

    T get(int x, int y) const {
        fassert(contains(x, y));
        return data[y * w + x];
    };

    T get_index(int index) const {
        fassert(0 <= index && index < w * h);
        return data[index];
    };

    int to_index(int x, int y) const {
        return y * w + x;
    };

    void to_xy(int idx, int *x, int *y) const {
        *x = idx % w;
        *y = idx / w;
    };

    void set(int x, int y, T v) {
        fassert(contains(x, y));
        data[y * w + x] = v;
    };

    void set_index(int index, T v) {
        fassert(index < w * h);
        data[index] = v;
    };

    void serialize(WriteBuffer *b) {
        b->write_int(w);
        b->write_int(h);
        b->write_vector_int(data);
    };

    void deserialize(ReadBuffer *b) {
        w = b->read_int();
        h = b->read_int();
        data = b->read_vector_int();
    };
};
