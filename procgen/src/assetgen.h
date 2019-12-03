#pragma once

/*

Generate assets procedurally

*/

#include "randgen.h"
#include <QColor>
#include <QImage>
#include <QRectF>
#include <QtGui/QPainter>
#include <memory>

struct ColorGen;

class AssetGen {

  public:
    AssetGen(RandGen *rg);
    void generate_resource(std::shared_ptr<QImage> img, int num_recurse = 1, int blotch_scale = 50, bool is_rect = true);

  private:
    RandGen *rand_gen;

    std::vector<QRectF> split_rect(QRectF rect, int num_splits, bool is_horizontal);
    QRectF choose_sub_rect(QRectF rect, float min_dim, float max_dim);
    QRectF create_bar(QRectF rect, bool is_horizontal);
    void paint_shape(QPainter &p, QRectF rect, ColorGen *cgen);
    void paint_rect_resource(QPainter &p, QRectF rect, int num_recurse, int blotch_scale);
    void paint_shape_resource(QPainter &p, QRectF rect);
};