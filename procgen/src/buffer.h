#pragma once

#include "cpp-utils.h"
#include <vector>
#include <string>

struct ReadBuffer {
    char *data = nullptr;
    size_t offset = 0;
    size_t length = 0;

    ReadBuffer(char *data, size_t length) : data(data), length(length) {
    };

    bool read_bool() {
        return read_int() > 0;
    };

    std::vector<bool> read_vector_bool() {
        std::vector<bool> v;
        v.resize(read_int());
        for (size_t i = 0; i < v.size(); i++) {
            v[i] = read_bool();
        }
        return v;
    };

    int read_int() {
        fassert(offset + sizeof(int) <= length);
        auto d = (int*)(&data[offset]);
        offset += sizeof(int);
        return *d;
    };

    std::vector<int> read_vector_int() {
        std::vector<int> v;
        v.resize(read_int());
        for (size_t i = 0; i < v.size(); i++) {
            v[i] = read_int();
        }
        return v;
    };

    float read_float() {
        fassert(offset + sizeof(float) <= length);
        auto d = (float*)(&data[offset]);
        offset += sizeof(float);
        return *d;
    };

    std::vector<float> read_vector_float() {
        std::vector<float> v;
        v.resize(read_int());
        for (size_t i = 0; i < v.size(); i++) {
            v[i] = read_float();
        }
        return v;
    };

    std::string read_string() {
        int size = read_int();
        std::string s(size, '\x00');
        fassert(offset + size <= length);
        auto c = data + offset;
        for (size_t i = 0; i < s.size(); i++) {
            s[i] = *c;
            c++;
        }
        offset += s.size();
        return s;
    };
};

struct WriteBuffer {
    char *data = nullptr;
    size_t offset = 0;
    size_t length = 0;

    WriteBuffer(char *data, size_t length) :  data(data), length(length) {
    };

    void write_bool(bool b) {
        write_int(b ? 1 : 0);
    };

    void write_vector_bool(const std::vector<bool>& v) {
        write_int(v.size());
        for (auto i : v) {
            write_bool(i);
        }
    };

    void write_int(int i) {
        fassert(offset + sizeof(int) <= length);
        auto d = (int*)(&data[offset]);
        *d = i;
        offset += sizeof(int);
    };


    void write_vector_int(const std::vector<int>& v) {
        write_int(v.size());
        for (auto i : v) {
            write_int(i);
        }
    };

    void write_float(float f) {
        fassert(offset + sizeof(float) <= length);
        auto d = (float*)(&data[offset]);
        *d = f;
        offset += sizeof(float);
    };

    void write_vector_float(const std::vector<float>& v) {
        write_int(v.size());
        for (auto i : v) {
            write_float(i);
        }
    };

    void write_string(std::string s) {
        fassert(offset + s.size() <= length);
        write_int(s.size());
        auto c = data + offset;
        for (size_t i = 0; i < s.size(); i++) {
            *c = s[i];
            c++;
        }
        offset += s.size();
    };
};