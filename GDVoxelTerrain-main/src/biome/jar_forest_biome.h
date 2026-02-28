// jar_forest_biome.h
#ifndef JAR_FOREST_BIOME_H
#define JAR_FOREST_BIOME_H

#include "jar_biome.h"
#include <algorithm>

class JarForestBiome : public JarBiome {
    GDCLASS(JarForestBiome, JarBiome);

private:
    float max_height = 60.0f;   // rolling hill ceiling
    float hill_scale = 0.6f;    // how strongly the ridge profile shapes hills
    float detail_strength = 0.25f;

public:
    //virtual float get_height(const Vector2& global_pos, float cell_center_val, float cell_edge_dist) const override
    //{
    //    float detail = 0.0f;
    //    if (get_detail_noise().is_valid())
    //        detail = (get_detail_noise()->get_noise_2d(global_pos.x, global_pos.y) + 1.0f) * 0.5f;

        // Same structure as meadow but taller ceiling - no Voronoi shaping
        // so there are zero cell-edge terracing artifacts
    //    return 0.48f * (0.3f + 0.7f * detail);
    //}
    virtual float get_height(const Vector2& global_pos, float cell_center_val, float cell_edge_dist) const override
    {
        float detail = 0.0f;
        if (get_detail_noise().is_valid())
            detail = (get_detail_noise()->get_noise_2d(global_pos.x, global_pos.y) + 1.0f) * 0.5f;

        // Use the inspector variables so you can tweak the deformations live!
        // 'max_height' scales the overall altitude.
        // 'hill_scale' controls the baseline swelling.
        // 'detail_strength' controls how violently the noise deforms the surface.
        float base_shape = (1.0f - detail_strength) * hill_scale;
        float noise_shape = detail_strength * detail;

        // A slightly more aggressive curve to make the forest hills "pop"
        return (max_height / 256.0f) * (base_shape + noise_shape);
    }
protected:
    static void _bind_methods()
    {
        ClassDB::bind_method(D_METHOD("set_max_height", "max_height"), &JarForestBiome::set_max_height);
        ClassDB::bind_method(D_METHOD("get_max_height"), &JarForestBiome::get_max_height);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_height", PROPERTY_HINT_RANGE, "0.0,200.0,0.5"),
            "set_max_height", "get_max_height");

        ClassDB::bind_method(D_METHOD("set_hill_scale", "hill_scale"), &JarForestBiome::set_hill_scale);
        ClassDB::bind_method(D_METHOD("get_hill_scale"), &JarForestBiome::get_hill_scale);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "hill_scale", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"),
            "set_hill_scale", "get_hill_scale");

        ClassDB::bind_method(D_METHOD("set_detail_strength", "detail_strength"), &JarForestBiome::set_detail_strength);
        ClassDB::bind_method(D_METHOD("get_detail_strength"), &JarForestBiome::get_detail_strength);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "detail_strength", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"),
            "set_detail_strength", "get_detail_strength");
    }

public:
    void set_max_height(float v) { max_height = v; }
    float get_max_height() const { return max_height; }
    void set_hill_scale(float v) { hill_scale = std::clamp(v, 0.0f, 1.0f); }
    float get_hill_scale() const { return hill_scale; }
    void set_detail_strength(float v) { detail_strength = std::clamp(v, 0.0f, 1.0f); }
    float get_detail_strength() const { return detail_strength; }
};

#endif // JAR_FOREST_BIOME_H