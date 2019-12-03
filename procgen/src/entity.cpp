#include "entity.h"

#include <math.h>

Entity::Entity(float _x, float _y, float _vx, float _vy, float _rx, float _ry, int _type) {
    x = _x;
    y = _y;
    vx = _vx;
    vy = _vy;
    rx = _rx;
    ry = _ry;
    type = _type;
    image_type = _type;
    image_theme = 0;
    will_erase = false;
    collides_with_entities = false;
    collision_margin = 0.0;
    rotation = 0.0;
    is_reflected = false;
    vrot = 0.0;
    alpha = 1.0;
    grow_rate = 1.0;
    alpha_decay = 1.0;

    fire_time = -1;
    spawn_time = -1;
    expire_time = -1;
    life_time = 0;

    health = 1;
    theta = -100;

    friction = 1;
    smart_step = false;
    avoids_collisions = false;
    auto_erase = true;
    render_z = 0;
    use_abs_coords = false;
    climber_spawn_x = 0;

    if (type == EXPLOSION) {
        grow_rate = 1.4f;
        expire_time = 4;
    } else if (type == TRAIL) {
        grow_rate = 1.05f;
        alpha_decay = 0.8f;
    }
}

Entity::Entity(float _x, float _y, float _vx, float _vy, float _r, int _type)
    : Entity(_x, _y, _vx, _vy, _r, _r, _type) {
}

void Entity::step() {
    if (!smart_step) {
        x += vx;
        y += vy;
    }

    rotation += vrot;

    vx *= friction;
    vy *= friction;
    life_time += 1;

    if (expire_time > 0 && life_time > expire_time) {
        will_erase = true;
    }

    if (type == EXPLOSION) {
        if (image_type < EXPLOSION5) {
            image_type++;
        }
    }

    rx *= grow_rate;
    ry *= grow_rate;
    alpha = alpha_decay * alpha;
}

void Entity::face_direction(float dx, float dy, float rotation_offset) {
    if (dx != 0 || dy != 0) {
        rotation = -1 * atan2(dy, dx) + rotation_offset;
    }
}