cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
project(codegen)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

option(PROCGEN_PACKAGE "Set if the python package is being built" OFF)

# print commands used, useful for debugging build
set(CMAKE_VERBOSE_MAKEFILE ${PROCGEN_PACKAGE})


if(APPLE)
  # clang defaults to 20 errors, but usually only the first one is useful
  add_compile_options(-ferror-limit=1)
  # only produce errors on mac where development is done
  add_compile_options(-Werror -Wextra -Wshadow -Wall -Wformat=2 -Wundef -Wvla -Wmissing-include-dirs -Wnon-virtual-dtor -Wno-unused-parameter)
endif()

if(MSVC)
  # allow strcpy
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

if (APPLE OR UNIX)
  if(PROCGEN_PACKAGE)
    # compile for the minimum spec processor
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -march=ivybridge")
  else()
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -march=native")
  endif()

  # leave frame pointers so that profiling tools will still work
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -fno-omit-frame-pointer")
endif()

# include qt5
find_package(Qt5 COMPONENTS Gui REQUIRED)

add_library(env
  SHARED
  src/assetgen.cpp
  src/basic-abstract-game.cpp
  src/cpp-utils.cpp
  src/entity.cpp
  src/game.cpp
  src/game-registry.cpp
  src/games/dodgeball.cpp
  src/games/bigfish.cpp
  src/games/bossfight.cpp
  src/games/caveflyer.cpp
  src/games/climber.cpp
  src/games/coinrun_old.cpp
  src/games/coinrun.cpp
  src/games/fruitbot.cpp
  src/games/heist.cpp
  src/games/jumper.cpp
  src/games/leaper.cpp
  src/games/maze.cpp
  src/games/miner.cpp
  src/games/ninja.cpp
  src/games/chaser.cpp
  src/games/plunder.cpp
  src/games/starpilot.cpp
  src/mazegen.cpp
  src/randgen.cpp
  src/roomgen.cpp
  src/resources.cpp
  src/vecgame.cpp
  src/vecoptions.cpp
)

# find libenv.h header
target_include_directories(env PUBLIC ${LIBENV_DIR})

target_link_libraries(env Qt5::Gui)