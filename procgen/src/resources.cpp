#include "resources.h"
#include "cpp-utils.h"

QString global_resource_root;

std::vector<std::shared_ptr<QImage>> topdown_backgrounds;
std::vector<std::shared_ptr<QImage>> topdown_simple_backgrounds;
std::vector<std::shared_ptr<QImage>> platform_backgrounds;
std::vector<std::shared_ptr<QImage>> space_backgrounds;
std::vector<std::shared_ptr<QImage>> water_backgrounds;
std::vector<std::shared_ptr<QImage>> water_surface_backgrounds;

std::shared_ptr<QImage> load_resource_ptr(QString relpath, QImage::Format format) {
    auto path = global_resource_root + relpath;
    auto asset = QImage(path).convertToFormat(format);
    auto asset_ptr = std::make_shared<QImage>(asset);

    if (asset_ptr->width() == 0) {
        fatal("failed to load image %s\n", path.toUtf8().constData());
    }
    return asset_ptr;
}

void images_load() {
    auto group_to_vector = std::map<std::string, std::vector<std::shared_ptr<QImage>> *>{
        {"space_backgrounds", &space_backgrounds},
        {"platform_backgrounds", &platform_backgrounds},
        {"topdown_backgrounds", &topdown_backgrounds},
        {"topdown_simple_backgrounds", &topdown_simple_backgrounds},
        {"water_backgrounds", &water_backgrounds},
        {"water_surface_backgrounds", &water_surface_backgrounds},
    };

    auto group_to_paths = std::map<std::string, std::vector<std::string>>{
        {
            "space_backgrounds",
            {
                "space_backgrounds/deep_space_01.png",
                "space_backgrounds/spacegen_01.png",
                "space_backgrounds/milky_way_01.png",
                "space_backgrounds/ez_space_lite_01.png",
                "space_backgrounds/meyespace_v1_01.png",
                "space_backgrounds/eye_nebula_01.png",
                "space_backgrounds/deep_sky_01.png",
                "space_backgrounds/space_nebula_01.png",
                "space_backgrounds/Background-1.png",
                "space_backgrounds/Background-2.png",
                "space_backgrounds/Background-3.png",
                "space_backgrounds/Background-4.png",
                "space_backgrounds/parallax-space-backgound.png",
            },
        },
        {
            "platform_backgrounds",
            {
                "platform_backgrounds/alien_bg.png",
                "platform_backgrounds/another_world_bg.png",
                "platform_backgrounds/back_cave.png",
                "platform_backgrounds/caverns.png",
                "platform_backgrounds/cyberpunk_bg.png",
                "platform_backgrounds/parallax_forest.png",
                "platform_backgrounds/scifi_bg.png",
                "platform_backgrounds/scifi2_bg.png",
                "platform_backgrounds/living_tissue_bg.png",
                "platform_backgrounds/airadventurelevel1.png",
                "platform_backgrounds/airadventurelevel2.png",
                "platform_backgrounds/airadventurelevel3.png",
                "platform_backgrounds/airadventurelevel4.png",
                "platform_backgrounds/cave_background.png",
                "platform_backgrounds/blue_desert.png",
                "platform_backgrounds/blue_grass.png",
                "platform_backgrounds/blue_land.png",
                "platform_backgrounds/blue_shroom.png",
                "platform_backgrounds/colored_desert.png",
                "platform_backgrounds/colored_grass.png",
                "platform_backgrounds/colored_land.png",
                "platform_backgrounds/colored_shroom.png",
                "platform_backgrounds/landscape1.png",
                "platform_backgrounds/landscape2.png",
                "platform_backgrounds/landscape3.png",
                "platform_backgrounds/landscape4.png",
                "platform_backgrounds/battleback1.png",
                "platform_backgrounds/battleback2.png",
                "platform_backgrounds/battleback3.png",
                "platform_backgrounds/battleback4.png",
                "platform_backgrounds/battleback5.png",
                "platform_backgrounds/battleback6.png",
                "platform_backgrounds/battleback7.png",
                "platform_backgrounds/battleback8.png",
                "platform_backgrounds/battleback9.png",
                "platform_backgrounds/battleback10.png",
                "platform_backgrounds/sunrise.png",
                "platform_backgrounds_2/beach1.png",
                "platform_backgrounds_2/beach2.png",
                "platform_backgrounds_2/beach3.png",
                "platform_backgrounds_2/beach4.png",
                "platform_backgrounds_2/fantasy1.png",
                "platform_backgrounds_2/fantasy2.png",
                "platform_backgrounds_2/fantasy3.png",
                "platform_backgrounds_2/fantasy4.png",
                "platform_backgrounds_2/candy1.png",
                "platform_backgrounds_2/candy2.png",
                "platform_backgrounds_2/candy3.png",
                "platform_backgrounds_2/candy4.png",
            },
        },
        {
            "topdown_backgrounds",
            {
                "topdown_backgrounds/floortiles.png",
                "topdown_backgrounds/backgrounddetailed1.png",
                "topdown_backgrounds/backgrounddetailed2.png",
                "topdown_backgrounds/backgrounddetailed3.png",
                "topdown_backgrounds/backgrounddetailed4.png",
                "topdown_backgrounds/backgrounddetailed5.png",
                "topdown_backgrounds/backgrounddetailed6.png",
                "topdown_backgrounds/backgrounddetailed7.png",
                "topdown_backgrounds/backgrounddetailed8.png",
            },
        },
        {
            "topdown_simple_backgrounds",
            {
                "topdown_backgrounds/floortiles.png",
            },
        },
        {
            "water_backgrounds",
            {
                "water_backgrounds/water1.png",
                "water_backgrounds/water2.png",
                "water_backgrounds/water3.png",
                "water_backgrounds/water4.png",
                "water_backgrounds/underwater1.png",
                "water_backgrounds/underwater2.png",
                "water_backgrounds/underwater3.png",
            },
        },
        {
            "water_surface_backgrounds",
            {

                "water_backgrounds/water1.png",
                "water_backgrounds/water2.png",
                "water_backgrounds/water3.png",
                "water_backgrounds/water4.png",
            },
        },
    };

    for (auto const &pair : group_to_paths) {
        auto vec = group_to_vector.at(pair.first);
        for (const auto &path : pair.second) {
            vec->push_back(load_resource_ptr(path.c_str(), QImage::Format_RGB32));
        }
    }

    // also add all space backgrounds as platform backgrounds
    for (auto bg : space_backgrounds) {
        platform_backgrounds.push_back(bg);
    }
}
