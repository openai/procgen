#include "vecoptions.h"
#include "cpp-utils.h"

VecOptions::VecOptions(const struct libenv_options options) {
    m_options = std::vector<libenv_option>(options.items, options.items + options.count);
}

void VecOptions::consume_string(std::string name, std::string *value) {
    auto opt = find_option(name, LIBENV_DTYPE_UINT8);
    if (opt.data == nullptr) {
        return;
    }
    *value = std::string((char *)(opt.data), opt.count);
}

void VecOptions::consume_int(std::string name, int32_t *value) {
    auto opt = find_option(name, LIBENV_DTYPE_INT32);
    if (opt.data == nullptr) {
        return;
    }
    *value = *(int32_t *)(opt.data);
}

void VecOptions::consume_bool(std::string name, bool *value) {
    auto opt = find_option(name, LIBENV_DTYPE_UINT8);
    if (opt.data == nullptr) {
        return;
    }
    uint8_t v = *(uint8_t *)(opt.data);
    fassert(v == 0 || v == 1);
    *value = (bool)v;
}

void VecOptions::ensure_empty() {
    if (m_options.size() > 0) {
        fatal("unused options found, first unused option: %s\n", m_options[0].name);
    }
}

libenv_option VecOptions::find_option(std::string name, enum libenv_dtype dtype) {
    for (size_t idx = 0; idx < m_options.size(); idx++) {
        auto opt = m_options[idx];
        const std::string key(opt.name);
        if (key == name) {
            if (opt.dtype != dtype) {
                fatal("invalid dtype for option %s\n", name.c_str());
            }
            m_options.erase(m_options.begin() + idx);
            return opt;
        }
    }
    libenv_option result;
    result.data = nullptr;
    return result;
};