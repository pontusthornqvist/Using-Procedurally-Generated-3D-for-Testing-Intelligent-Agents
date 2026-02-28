#ifndef JAR_MOUNTAIN_BIOME_H
#define JAR_MOUNTAIN_BIOME_H

#include "jar_biome.h"
#include <algorithm>

class JarMountainBiome : public JarBiome {
    GDCLASS(JarMountainBiome, JarBiome);

private:
    float peak_height = 150.0f;
    float ridge_width = 0.2f;

public:
	// This is the "heuristic" for mountain height based on biome depth (cell_edge_dist) and detail noise.
    virtual float get_height(const Vector2& global_pos, float cell_center_val, float cell_edge_dist) const override
    {
        // cell_edge_dist is now biome_depth [0,1] - how far into mountain territory we are.
        // High values = deep in mountain region = taller peaks.
        // This creates organic swelling mountains rather than ridge lines.
        float base_swell = cell_edge_dist * cell_edge_dist; // smooth ramp up

        float detail = 0.0f;
        if (get_detail_noise().is_valid())
            detail = (get_detail_noise()->get_noise_2d(global_pos.x, global_pos.y) + 1.0f) * 0.5f;

        // Base swell from biome depth + detail noise for surface roughness
        // Valley floor stays at 0.1 so transition from forest is gradual
        return std::clamp(0.1f + 0.9f * (base_swell * 0.6f + detail * 0.4f * base_swell), 0.0f, 1.0f);
    }
protected:
    static void _bind_methods() {
        // Bind peak_height and ridge_width here to expose to inspector...
    }
};

#endif