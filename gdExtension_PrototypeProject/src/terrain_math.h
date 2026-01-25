#ifndef TERRAIN_MATH_H
#define TERRAIN_MATH_H

#include <cmath>
#include <algorithm> // FIXED: Required for std::clamp
#include <vector>
#include <godot_cpp/variant/vector3.hpp>
#include "FastNoiseLite.h" 

using namespace godot;

// FIXED: Defined at top so it can be used below
inline float smoothstep(float edge0, float edge1, float x) {
    x = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return x * x * (3 - 2 * x);
}

struct PathSegment {
    Vector3 start;
    Vector3 end;
    float width;
    float depth;
};

class TerrainMath {
private:
    FastNoiseLite mountain_noise;
    FastNoiseLite warp_noise;
    FastNoiseLite detail_noise;

public:
    TerrainMath(int seed) {
        warp_noise.SetSeed(seed);
        warp_noise.SetFrequency(0.002f);
        warp_noise.SetFractalType(FastNoiseLite::FractalType_DomainWarpProgressive);
        warp_noise.SetDomainWarpAmp(50.0f);

        mountain_noise.SetSeed(seed + 1);
        mountain_noise.SetFrequency(0.0005f);
        mountain_noise.SetFractalType(FastNoiseLite::FractalType_Ridged);
        mountain_noise.SetFractalOctaves(5);

        detail_noise.SetSeed(seed + 2);
        detail_noise.SetFrequency(0.002f);
    }

    float get_density(Vector3 pos, const std::vector<PathSegment>& paths) {

        float river_carve = 0.0f;

        for (const auto& path : paths) {
            Vector3 ab = path.end - path.start;
            float t = (pos - path.start).dot(ab) / ab.length_squared();
            t = std::clamp(t, 0.0f, 1.0f); // FIXED: std::clamp
            Vector3 nearest = path.start + ab * t;
            float dist = pos.distance_to(nearest);

            if (dist < path.width) {
                float valley = smoothstep(path.width, 0.0f, dist);
                river_carve = std::max(river_carve, valley * path.depth);
            }
        }

        float wx = pos.x; float wy = pos.y; float wz = pos.z;
        warp_noise.DomainWarp(wx, wz);

        float ridge_val = mountain_noise.GetNoise(wx, wz);
        float terrain_height = ridge_val * 150.0f;

        if (terrain_height > 120.0f) {
            float overflow = terrain_height - 120.0f;
            terrain_height = 120.0f + (overflow * 0.1f);
        }

        float cave_shape = detail_noise.GetNoise(pos.x, pos.y * 2.0f, pos.z);
        float density = (terrain_height - pos.y);

        if (std::abs(density) < 20.0f) {
            density += cave_shape * 10.0f;
        }

        density -= river_carve * 50.0f;

        return density;
    }

    Vector3 get_normal(Vector3 pos, const std::vector<PathSegment>& paths) {
        float e = 0.1f;
        float dx = get_density(Vector3(pos.x + e, pos.y, pos.z), paths) - get_density(Vector3(pos.x - e, pos.y, pos.z), paths);
        float dy = get_density(Vector3(pos.x, pos.y + e, pos.z), paths) - get_density(Vector3(pos.x, pos.y - e, pos.z), paths);
        float dz = get_density(Vector3(pos.x, pos.y, pos.z + e), paths) - get_density(Vector3(pos.x, pos.y, pos.z - e), paths);
        return Vector3(-dx, -dy, -dz).normalized();
    }
};

#endif