// jar_meadow_biome.h
#ifndef JAR_MEADOW_BIOME_H
#define JAR_MEADOW_BIOME_H

#include "jar_biome.h"
#include <algorithm>

class JarMeadowBiome : public JarBiome {
    GDCLASS(JarMeadowBiome, JarBiome);

private:
    float max_height = 10.0f;      // gentle undulation ceiling
    float roughness = 0.3f;      // how much detail noise contributes

public:
    virtual float get_height(const Vector2& global_pos, float cell_center_val, float cell_edge_dist) const override
    {
        float detail = 0.0f;
        if (get_detail_noise().is_valid())
            detail = (get_detail_noise()->get_noise_2d(global_pos.x, global_pos.y) + 1.0f) * 0.5f;

        // Purely noise-driven, no Voronoi shaping at all - avoids any cell-edge artifacts
        return 0.06f * (0.4f + 0.6f * detail);
    }

protected:
    static void _bind_methods()
    {
        ClassDB::bind_method(D_METHOD("set_max_height", "max_height"), &JarMeadowBiome::set_max_height);
        ClassDB::bind_method(D_METHOD("get_max_height"), &JarMeadowBiome::get_max_height);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_height", PROPERTY_HINT_RANGE, "0.0,200.0,0.5"),
            "set_max_height", "get_max_height");

        ClassDB::bind_method(D_METHOD("set_roughness", "roughness"), &JarMeadowBiome::set_roughness);
        ClassDB::bind_method(D_METHOD("get_roughness"), &JarMeadowBiome::get_roughness);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "roughness", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"),
            "set_roughness", "get_roughness");
    }

public:
    void set_max_height(float v) { max_height = v; }
    float get_max_height() const { return max_height; }
    void set_roughness(float v) { roughness = std::clamp(v, 0.0f, 1.0f); }
    float get_roughness() const { return roughness; }
};

#endif // JAR_MEADOW_BIOME_H