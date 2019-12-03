#pragma once

/*

Qt utility functions

*/

#include <QRect>
#include <QColor>

inline QRectF adjust_rect(const QRectF &base_rect, const QRectF &adjusting_rect) {
    QRectF rect = QRectF(base_rect.x() + base_rect.width() * adjusting_rect.x(),
                         base_rect.y() + base_rect.height() * adjusting_rect.y(),
                         base_rect.width() * adjusting_rect.width(),
                         base_rect.height() * adjusting_rect.height());

    return rect;
}

inline int to_shade(float f) {
    int shade = int(f * 255);
    if (shade < 0)
        shade = 0;
    if (shade > 255)
        shade = 255;
    return shade;
}