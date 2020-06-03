#include "entity.h"

#include <math.h>

Entity::Entity() {
}

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

void Entity::serialize(WriteBuffer *b) {
    b->write_float(x);
    b->write_float(y);

    b->write_float(vx);
    b->write_float(vy);

    b->write_float(rx);
    b->write_float(ry);

    b->write_int(type);
    b->write_int(image_type);
    b->write_int(image_theme);

    b->write_int(render_z);

    b->write_int(will_erase);
    b->write_int(collides_with_entities);

    b->write_float(collision_margin);
    b->write_float(rotation);
    b->write_float(vrot);

    b->write_int(is_reflected);
    b->write_int(fire_time);
    b->write_int(spawn_time);
    b->write_int(life_time);
    b->write_int(expire_time);
    b->write_int(use_abs_coords);

    b->write_float(friction);
    b->write_int(smart_step);
    b->write_int(avoids_collisions);
    b->write_int(auto_erase);

    b->write_float(alpha);
    b->write_float(health);
    b->write_float(theta);
    b->write_float(grow_rate);
    b->write_float(alpha_decay);
    b->write_float(climber_spawn_x);

    // not implemented, only used by fruitbot
    // std::weak_ptr<Entity> relative;
}

void Entity::deserialize(ReadBuffer *b) {
    x = b->read_float();
    y = b->read_float();

    vx = b->read_float();
    vy = b->read_float();

    rx = b->read_float();
    ry = b->read_float();

    type = b->read_int();
    image_type = b->read_int();
    image_theme = b->read_int();

    render_z = b->read_int();

    will_erase = b->read_int();
    collides_with_entities = b->read_int();

    collision_margin = b->read_float();
    rotation = b->read_float();
    vrot = b->read_float();

    is_reflected = b->read_int();
    fire_time = b->read_int();
    spawn_time = b->read_int();
    life_time = b->read_int();
    expire_time = b->read_int();
    use_abs_coords = b->read_int();

    friction = b->read_float();
    smart_step = b->read_int();
    avoids_collisions = b->read_int();
    auto_erase = b->read_int();

    alpha = b->read_float();
    health = b->read_float();
    theta = b->read_float();
    grow_rate = b->read_float();
    alpha_decay = b->read_float();
    climber_spawn_x = b->read_float();
}
