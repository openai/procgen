#pragma once

#include "object-ids.h"
#include <memory>

class Entity {
  public:
    float x, y, vx, vy, rx, ry;
    int type;
    int image_type;
    int image_theme;

    // Currently supports only 3 levels
    // 1: render above other entities (at agent level)
    // 0: standard rendering
    // -1: render below grid objects
    int render_z;

    bool will_erase;
    bool collides_with_entities;
    float collision_margin;
    float rotation, vrot;
    bool is_reflected;
    int fire_time, spawn_time, life_time, expire_time;
    bool use_abs_coords;

    float friction;
    bool smart_step, avoids_collisions, auto_erase;

    // often not used
    float alpha;
    float health;
    float theta;
    float grow_rate;
    float alpha_decay;
    float climber_spawn_x;
    std::weak_ptr<Entity> relative;

    Entity(float _x, float _y, float _dx, float _dy, float _rx, float _ry, int _type);
    Entity(float _x, float _y, float _dx, float _dy, float _r, int _type);

    void step();
    bool should_erase();
    void face_direction(float dx, float dy, float rotation_offset = 0);
};