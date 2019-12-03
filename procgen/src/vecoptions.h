#pragma once

/*

Utility class to parse options provided through the libenv interface

There is no error if an option is missing, the passed value is not changed in that case.

*/

#include "libenv.h"
#include <string>
#include <vector>

class VecOptions {
  public:
    VecOptions(const struct libenv_options options);
    void consume_string(std::string name, std::string *value);
    void consume_int(std::string name, int32_t *value);
    void consume_bool(std::string name, bool *value);
    void ensure_empty();

  private:
    std::vector<libenv_option> m_options;
    libenv_option find_option(std::string name, enum libenv_dtype dtype);
};