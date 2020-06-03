#pragma once

/*

Base game class used by all currently existing games

*/

#include <string>
#include <set>
#include <queue>
#include "game.h"
#include "grid.h"
#include "cpp-utils.h"

class BasicAbstractGame : public Game {
  public:
    int grid_size = 0;

    BasicAbstractGame(std::string name);
    ~BasicAbstractGame();

    // Game methods
    void game_step() override;
    void game_reset() override;
    void game_draw(QPainter &p, const QRect &rect) override;
    void game_init() override;
    void serialize(WriteBuffer *b) override;
    void deserialize(ReadBuffer *b) override;

    void write_entities(WriteBuffer *b, std::vector<std::shared_ptr<Entity>> &ents);
    void read_entities(ReadBuffer *b, std::vector<std::shared_ptr<Entity>> &ents);

    virtual bool is_blocked(const std::shared_ptr<Entity> &src, int target, bool is_horizontal);
    virtual bool is_blocked_ents(const std::shared_ptr<Entity> &src, const std::shared_ptr<Entity> &target, bool is_horizontal);
    virtual bool will_reflect(int src, int target);
    virtual void handle_agent_collision(const std::shared_ptr<Entity> &obj);
    virtual void handle_grid_collision(const std::shared_ptr<Entity> &obj, int type, int i, int j);
    virtual void handle_collision(const std::shared_ptr<Entity> &src, const std::shared_ptr<Entity> &target);
    virtual float get_agent_acceleration_scale();
    virtual bool use_block_asset(int type);
    virtual float get_tile_aspect_ratio(const std::shared_ptr<Entity> &type);
    virtual void asset_for_type(int type, std::vector<std::string> &names);
    virtual void load_background_images();
    virtual int image_for_type(int grid_obj);
    virtual int theme_for_grid_obj(int type);
    virtual bool should_preserve_type_themes(int type);
    virtual QColor color_for_type(int type, int theme);
    virtual void draw_grid_obj(QPainter &p, const QRectF &rect, int type, int theme);
    virtual void choose_world_dim();
    virtual bool should_draw_entity(const std::shared_ptr<Entity> &entity);
    virtual void set_action_xy(int move_action);
    virtual void choose_center(float &cx, float &cy);
    virtual void update_agent_velocity();
    virtual QRectF get_adjusted_image_rect(int type, const QRectF &rect);

    void reserved_asset_for_type(int type, std::vector<std::string> &names);
    void choose_step_random_theme(const std::shared_ptr<Entity> &ent);
    bool use_procgen_asset(int type);
    void decay_agent_velocity();
    void tile_image(QPainter &p, std::shared_ptr<QImage> image, QRectF &rect, float tile_ratio);
    void set_pen_brush_color(QPainter &p, QColor color, int thickness = 1);
    void basic_step_object(const std::shared_ptr<Entity> &obj);
    std::shared_ptr<Entity> spawn_entity_rxy(float rx, float ry, int type, float x, float y, float w, float h, bool check_collisions = true);
    std::shared_ptr<Entity> spawn_entity(float r, int type, float x, float y, float w, float h, bool check_collisions = true);
    std::shared_ptr<Entity> spawn_entity_at_idx(int idx, float r, int type);
    std::shared_ptr<Entity> add_entity(float x, float y, float vx, float vy, float r, int type);
    std::shared_ptr<Entity> add_entity_rxy(float x, float y, float vx, float vy, float rx, float ry, int type);
    std::shared_ptr<Entity> spawn_child(const std::shared_ptr<Entity> &src, int type, float obj_r, bool match_vel = false);
    void spawn_entities(int num_objects, float r, int type, float x, float y, float w, float h);
    void reposition(const std::shared_ptr<Entity> &ent, float x, float y, float w, float h, bool check_collisions);
    int get_obj(int i, int j);
    int get_obj(int idx);
    void set_obj(int i, int j, int obj);
    void set_obj(int idx, int elem);
    int to_grid_idx(int x, int y);
    void to_grid_xy(int idx, int *x, int *y);
    void fill_elem(int x, int y, int dx, int dy, char elem);
    int get_obj_from_floats(float i, float j);
    int get_agent_index();
    std::vector<int> get_cells_with_type(int type);

    void check_grid_collisions(const std::shared_ptr<Entity> &src);
    float get_distance(const std::shared_ptr<Entity> &p0, const std::shared_ptr<Entity> &p1);
    void match_aspect_ratio(const std::shared_ptr<Entity> &ent, bool match_width = true);
    void fit_aspect_ratio(const std::shared_ptr<Entity> &ent);
    void choose_random_theme(const std::shared_ptr<Entity> &ent);
    int mask_theme_if_necessary(int theme, int type);
    void tile_image(QPainter &p, QImage *image, const QRectF &rect, float tile_ratio);

    float rand_pos(float r, float max);
    float rand_pos(float r, float min, float max);
    bool has_any_collision(const std::shared_ptr<Entity> &e1, float margin = 0);
    bool has_agent_collision(const std::shared_ptr<Entity> &e1);
    bool has_collision(const std::shared_ptr<Entity> &e1, const std::shared_ptr<Entity> &e2, float margin = 0);
    bool is_out_of_bounds(const std::shared_ptr<Entity> &e1);
    bool push_obj(const std::shared_ptr<Entity> &src, const std::shared_ptr<Entity> &target, bool is_horizontal, int depth);
    float get_theta(const std::shared_ptr<Entity> &src, const std::shared_ptr<Entity> &target);
    int find_entity_index(int type);

    QRectF get_screen_rect(float x, float y, float dx, float dy, float render_eps = 0);
    QRectF get_abs_rect(float x, float y, float dx, float dy);
    QRectF get_object_rect(const std::shared_ptr<Entity> &obj);

    void draw_foreground(QPainter &p, const QRect &rect);

    void step_entities(const std::vector<std::shared_ptr<Entity>> &given);

    void erase_if_needed();

    bool agent_has_collision();
    void reposition_agent();

  protected:
    std::shared_ptr<Entity> agent;
    std::vector<std::shared_ptr<Entity>> entities;
    std::vector<std::shared_ptr<QImage>> basic_assets;
    std::vector<std::shared_ptr<QImage>> basic_reflections;
    std::vector<std::shared_ptr<QImage>> *main_bg_images_ptr;

    std::vector<float> asset_aspect_ratios;
    std::vector<int> asset_num_themes;
    
    bool use_procgen_background = false;
    int background_index = 0;
    float bg_tile_ratio = 0.0f;
    float bg_pct_x = 0.0f;

    float char_dim = 0.0f;
    int last_move_action = 0;
    int move_action = 0;
    int special_action = 0;
    float mixrate = 0.0f;
    float maxspeed = 0.0f;
    float max_jump = 0.0f;

    float action_vx = 0.0f;
    float action_vy = 0.0f;
    float action_vrot = 0.0f;

    float center_x = 0.0f;
    float center_y = 0.0f;

    bool random_agent_start = true;
    bool has_useful_vel_info = false;
    int step_rand_int = 0;

    RandGen asset_rand_gen;

    int main_width = 0;
    int main_height = 0;
    int out_of_bounds_object = 0;

    float unit = 0.0f;
    float view_dim = 0.0f;
    float x_off = 0.0f;
    float y_off = 0.0f;
    float visibility = 0.0f;
    float min_visibility = 0.0f;

  private:
    Grid<int> grid;

    QImage *lookup_asset(int img_idx, bool is_reflected = false);
    void initialize_asset_if_necessary(int img_idx);
    void prepare_for_drawing(float rect_height);
    void draw_background(QPainter &p, const QRect &rect);
    void draw_entity(QPainter &p, const std::shared_ptr<Entity> &to_draw);
    void draw_entities(QPainter &p, const std::vector<std::shared_ptr<Entity>> &to_draw, int render_z = 0);
    void draw_image(QPainter &p, QRectF &rect, float rotation, bool is_reflected, int img_idx, int theme, float alpha, float tile_ratio);

    bool sub_step(const std::shared_ptr<Entity> &obj, float _vx, float _vy, int depth);
    bool should_erase(const std::shared_ptr<Entity> &e1);
};
