#pragma once

#include "object-ids.h"
#include "buffer.h"
#include <memory>

class Entity {
  public:
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float rx = 0.0f;
    float ry = 0.0f; 
    int type = 0;
    int image_type = 0;
    int image_theme = 0;

    // Currently supports only 3 levels
    // 1: render above other entities (at agent level)
    // 0: standard rendering
    // -1: render below grid objects
    int render_z = 0;

    bool will_erase = false;
    bool collides_with_entities = false;
    float collision_margin = 0.0f;
    float rotation = 0.0f;
    float vrot = 0.0f;
    bool is_reflected = false;
    int fire_time = 0;
    int spawn_time = 0;
    int life_time = 0;
    int expire_time = 0;
    bool use_abs_coords = false;

    float friction = 0.0f;
    bool smart_step = false;
    bool avoids_collisions = false;
    bool auto_erase = false;

    // often not used
    float alpha = 0.0f;
    float health = 0.0f;
    float theta = 0.0f;
    float grow_rate = 0.0f;
    float alpha_decay = 0.0f;
    float climber_spawn_x = 0.0f;

    Entity();
    Entity(float _x, float _y, float _dx, float _dy, float _rx, float _ry, int _type);
    Entity(float _x, float _y, float _dx, float _dy, float _r, int _type);

    void step();
    bool should_erase();
    void face_direction(float dx, float dy, float rotation_offset = 0);
    void serialize(WriteBuffer *b);
    void deserialize(ReadBuffer *b);
};
