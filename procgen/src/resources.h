#pragma once

/*

Load assets stored as individual image files

*/

#include <QtGui/QPainter>
#include <iostream>
#include <memory>

std::shared_ptr<QImage> load_resource_ptr(QString relpath, QImage::Format format = QImage::Format_ARGB32_Premultiplied);

extern QString global_resource_root;
extern void images_load();
extern std::vector<std::shared_ptr<QImage>> topdown_backgrounds;
extern std::vector<std::shared_ptr<QImage>> topdown_simple_backgrounds;
extern std::vector<std::shared_ptr<QImage>> platform_backgrounds;
extern std::vector<std::shared_ptr<QImage>> space_backgrounds;
extern std::vector<std::shared_ptr<QImage>> water_backgrounds;
extern std::vector<std::shared_ptr<QImage>> water_surface_backgrounds;