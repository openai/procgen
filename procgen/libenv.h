// C API for reinforcement learning environments

// Environment libraries are normal C shared libraries, providing
// the interface described here.  Each library must implement all
// functions.

#pragma once

// allow this header to be included by a C++ file
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#if defined(__GNUC__)
#define LIBENV_API __attribute__((__dllexport__))
#else
#define LIBENV_API __declspec(dllexport)
#endif
#else
#if defined(__GNUC__)
#define LIBENV_API __attribute__((__visibility__("default")))
#else
#define LIBENV_API
#endif
#endif

// BEGIN_CDEF

#define LIBENV_MAX_NAME_LEN 128
#define LIBENV_MAX_NDIM 16

// opaque type for the environment returned by the library
typedef void libenv_venv;

// data types used by spaces
enum libenv_dtype {
    LIBENV_DTYPE_UNUSED = 0,
    LIBENV_DTYPE_UINT8 = 1,
    LIBENV_DTYPE_INT32 = 2,
    LIBENV_DTYPE_FLOAT32 = 3,
};

// space types corresponding to gym spaces
enum libenv_space_type {
    LIBENV_SPACE_TYPE_UNUSED = 0,
    LIBENV_SPACE_TYPE_BOX = 1,
    LIBENV_SPACE_TYPE_DISCRETE = 2,
};

// different spaces used by the environments
enum libenv_spaces_name {
    LIBENV_SPACES_UNUSED = 0,
    LIBENV_SPACES_OBSERVATION = 1,
    LIBENV_SPACES_ACTION = 2,
    LIBENV_SPACES_INFO = 3,
    LIBENV_SPACES_RENDER = 4,
};

// an instance of one of the above data types, use the field corresponding
// to the dtype that is being used
union libenv_value {
    uint8_t uint8;
    int32_t int32;
    float float32;
};

// libenv_space describes the fixed-size observation, and action data structures of an
// environment
//
// each individual space corresponds to a fixed sized contiguous array of data
// equivalent to a numpy array
struct libenv_space {
    char name[LIBENV_MAX_NAME_LEN];
    enum libenv_space_type type;
    enum libenv_dtype dtype;
    int shape[LIBENV_MAX_NDIM];
    int ndim;
    union libenv_value low;
    union libenv_value high;
};

// libenv_option holds a name-data pair used to configure environment instances
//
// the buffer pointed to by the data pointer will be kept alive by python for the
// duration of the venv or libenv
struct libenv_option {
    char name[LIBENV_MAX_NAME_LEN];
    enum libenv_dtype dtype;
    int count;
    void *data;
};

// libenv_options holds an array of libenv_option instances
struct libenv_options {
    struct libenv_option *items;
    int count;
};

// libenv_step holds the result of a call to step_wait()
// all memory will be allocated by the caller of step_wait()
//
// obs and infos are arrays of buffer pointers, the length is determined
// by the length of the corresponding spaces object times the number of environments
//
// the sizes of obs, rews, and dones in bytes are:
//   obs:     size(observation_spaces) * num_envs
//   rews:    4 * num_envs
//   dones:   1 * num_envs
//
// the spaces are arranged in a structure of arrays fashion, for instance,
// given an observation space with keys, (uint8) shapes of {'a': [1], 'b': [1]},
// and num_envs = 2, the memory layout looks like this:
//  0                   1
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |    env 1 a    |    env 2 a    |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  0                   1
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |    env 1 b    |    env 2 b    |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
struct libenv_step {
    void **obs;
    float *rews;
    uint8_t *dones;
    void **infos;
};

#if !defined(NO_PROTOTYPE)

// libenv_make creates a new environment instance
//
// returns a pointer to an opaque environment instance
// the instance being pointed to belongs to the library, use libenv_close() to destroy it
//
// environments are not required to be thread-safe, but different environments
// must be usable from different threads simultaneously
//
// num_envs is the number of environments to create, these may actually be multiple agents
// in the same environment, but that is up to the individual environment to decide
//
// options is a series of key-value pairs whose meaning is only known to an individual environment library
LIBENV_API libenv_venv *libenv_make(int num_envs, const struct libenv_options options);

// gets a description of the spaces used by this environment instance
// the caller must allocate the correct number of spaces and owns the memory
// if called with a null pointer for spaces, returns the number of spaces that are required
LIBENV_API int libenv_get_spaces(libenv_venv *handle, enum libenv_spaces_name name, struct libenv_space *spaces);

// libenv_reset resets the environment to the starting state
// the step object belongs to the caller, along with the obs buffer which must be allocated by the caller
LIBENV_API void libenv_reset(libenv_venv *handle, struct libenv_step *step);

// libenv_step_async submits an action to the environment, but doesn't wait for it to complete
//
// the environment may apply the action in a thread, or else just store it and apply it when
// libenv_step_wait() is called, but in either case it should not block
//
// the step object belongs to the caller, along with the obs buffer which must be allocated by the caller
// the step object must stay valid until step_wait is called
LIBENV_API void libenv_step_async(libenv_venv *handle, const void **acts, struct libenv_step *step);

// libenv_step_wait observes the environment, along with the reward and if the episode is done
//
// the step object belongs to the caller, along with the obs buffer which must be allocated by the caller
//
// if the episode is done, the `obs` value of the step will be from the new
// episode
LIBENV_API void libenv_step_wait(libenv_venv *handle);

// libenv_render renders the environment
//
// the mode must be defined in render_spaces.  the value of frames will be an array of buffer pointers
// based on the specification in the render_spaces, similar to obs or acts except that only the pointers
// for the current mode are passed in, so the length of the frames array is the same as num_envs.
//
// the return value should be false if the user has closed the window or otherwise indicated
// that they are done with the environment
LIBENV_API bool libenv_render(libenv_venv *handle, const char *mode, void **frames);

// libenv_close closes the environment and frees any resources associated with it
LIBENV_API void libenv_close(libenv_venv *handle);

#endif

// END_CDEF

#ifdef __cplusplus
}
#endif