#ifndef JAR_FOREST_MAP_H
#define JAR_FOREST_MAP_H

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <random>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include "jar_ridge_map.h"

using namespace godot;

class JarForestMap : public Resource
{
    GDCLASS(JarForestMap, Resource);

private:
    Ref<FastNoiseLite> _voronoi_noise;  // same noise + seed as biome_noise and ridge_map
    Ref<JarRidgeMap>   _ridge_map;      // used to exclude already-claimed mountain cells

    int   _blob_count = 6;
    int   _blob_size = 4;    // cells per blob (ungrouped - no directionality)
    float _forest_falloff_radius = 500.0f;
    int   _texture_resolution = 512;
    float _world_size = 4096.0f;
    int   _random_seed = 43;   // different default from ridge map

    Ref<ImageTexture>  _forest_texture;
    std::vector<float> _cpu_buffer;

    // Reuse the same cell-center-finding logic as JarRidgeMap
    glm::vec2 find_cell_center(const glm::vec2& world_pos) const
    {
        if (_voronoi_noise.is_null()) return world_pos;
        glm::vec2 p = world_pos;
        const float step = _world_size / _texture_resolution;
        for (int i = 0; i < 32; i++)
        {
            float v = _voronoi_noise->get_noise_2d(p.x, p.y);
            float vx = _voronoi_noise->get_noise_2d(p.x + step, p.y);
            float vy = _voronoi_noise->get_noise_2d(p.x, p.y + step);
            glm::vec2 grad(vx - v, vy - v);
            float len = glm::length(grad);
            if (len < 1e-5f) break;
            p += (grad / len) * step * 0.5f;
        }
        return p;
    }

    std::vector<glm::vec2> find_adjacent_cells(const glm::vec2& cell_center, float search_radius) const
    {
        std::vector<glm::vec2> candidates;
        const float step = search_radius / 3.0f;

        for (float dx = -search_radius; dx <= search_radius; dx += step)
            for (float dy = -search_radius; dy <= search_radius; dy += step)
            {
                glm::vec2 candidate = find_cell_center(cell_center + glm::vec2(dx, dy));
                bool duplicate = false;
                for (auto& existing : candidates)
                    if (glm::distance(candidate, existing) < step * 0.5f)
                    {
                        duplicate = true; break;
                    }
                if (!duplicate && glm::distance(candidate, cell_center) > step * 0.5f)
                    candidates.push_back(candidate);
            }
        return candidates;
    }

    // Check if a candidate cell center is too close to any mountain ridge cell.
    // Uses the ridge map's cpu_buffer to check influence at the candidate position.
    bool is_claimed_by_ridge(const glm::vec2& cell_center) const
    {
        if (_ridge_map.is_null()) return false;
        // If ridge influence at this position is above a small threshold, cell is taken
        float influence = _ridge_map->sample_influence(cell_center.x, cell_center.y);
        return influence > 0.05f;
    }

    // Grow a blob of adjacent cells from a seed, without directionality.
    // All adjacent cells are candidates equally - picks by proximity to seed.
    void grow_blob(const glm::vec2& seed, std::vector<glm::vec2>& blob_cells,
        const std::vector<glm::vec2>& all_existing_cells) const
    {
        const float cell_search_radius = _world_size / 8.0f;
        std::vector<glm::vec2> frontier;
        frontier.push_back(seed);
        blob_cells.push_back(seed);

        while ((int)blob_cells.size() < _blob_size && !frontier.empty())
        {
            // Expand from the last added cell
            glm::vec2 current = frontier.back();
            frontier.pop_back();

            auto adjacent = find_adjacent_cells(current, cell_search_radius);

            for (auto& candidate : adjacent)
            {
                if ((int)blob_cells.size() >= _blob_size) break;

                // Skip if already in this blob
                bool in_blob = false;
                for (auto& existing : blob_cells)
                    if (glm::distance(candidate, existing) < cell_search_radius * 0.3f)
                    {
                        in_blob = true; break;
                    }
                if (in_blob) continue;

                // Skip if claimed by another blob
                bool in_other_blob = false;
                for (auto& existing : all_existing_cells)
                    if (glm::distance(candidate, existing) < cell_search_radius * 0.3f)
                    {
                        in_other_blob = true; break;
                    }
                if (in_other_blob) continue;

                // Skip if claimed by a mountain ridge
                if (is_claimed_by_ridge(candidate)) continue;

                blob_cells.push_back(candidate);
                frontier.push_back(candidate);
            }
        }
    }

public:
    JarForestMap() {}

    void bake()
    {
        if (_voronoi_noise.is_null())
        {
            UtilityFunctions::printerr("JarForestMap: voronoi_noise is not set.");
            return;
        }
        if (_ridge_map.is_null())
        {
            UtilityFunctions::printerr("JarForestMap: ridge_map is not set. Forest cells will not avoid mountains.");
        }
        if (_forest_falloff_radius < 400.0f)
            UtilityFunctions::print("JarForestMap: forest_falloff_radius may be too small for smooth SDF gradients. Consider >= 500.");

        std::mt19937 rng(_random_seed);
        std::uniform_real_distribution<float> world_dist(-_world_size * 0.4f, _world_size * 0.4f);

        // --- Step 1: Grow forest blobs ---
        std::vector<glm::vec2> all_forest_cells;

        for (int b = 0; b < _blob_count; b++)
        {
            // Keep trying seed positions until we find one not claimed by a ridge
            glm::vec2 seed_center;
            int attempts = 0;
            do {
                glm::vec2 seed_pos(world_dist(rng), world_dist(rng));
                seed_center = find_cell_center(seed_pos);
                attempts++;
            } while (is_claimed_by_ridge(seed_center) && attempts < 20);

            if (attempts >= 20)
            {
                UtilityFunctions::print("JarForestMap: could not find unclaimed seed for blob ", b, " after 20 attempts.");
                continue;
            }

            // Also skip if too close to an existing forest blob seed
            bool too_close = false;
            for (auto& existing : all_forest_cells)
                if (glm::distance(seed_center, existing) < _forest_falloff_radius * 0.5f)
                {
                    too_close = true; break;
                }
            if (too_close) continue;

            std::vector<glm::vec2> blob_cells;
            grow_blob(seed_center, blob_cells, all_forest_cells);
            all_forest_cells.insert(all_forest_cells.end(), blob_cells.begin(), blob_cells.end());
        }

        // --- Step 2: Bake distance field ---
        int res = _texture_resolution;
        _cpu_buffer.resize(res * res, 0.0f);
        float half_world = _world_size * 0.5f;

        for (int py = 0; py < res; py++)
        {
            for (int px = 0; px < res; px++)
            {
                float wx = (px / float(res)) * _world_size - half_world;
                float wy = (py / float(res)) * _world_size - half_world;
                glm::vec2 world_pos(wx, wy);

                float min_dist = std::numeric_limits<float>::max();
                for (auto& cell : all_forest_cells)
                    min_dist = std::min(min_dist, glm::distance(world_pos, cell));

                float t = std::clamp(min_dist / _forest_falloff_radius, 0.0f, 1.0f);
                float influence = 1.0f - (t * t * (3.0f - 2.0f * t));

                // Zero out any influence that overlaps with mountain ridge influence.
                // This gives a hard exclusion rather than relying on cell-level checks alone.
                if (!_ridge_map.is_null())
                {
                    float ridge_influence = _ridge_map->sample_influence(wx, wy);
                    influence = influence * (1.0f - ridge_influence);
                }

                _cpu_buffer[py * res + px] = influence;
            }
        }

        // --- Step 3: Upload to ImageTexture ---
        PackedByteArray bytes;
        bytes.resize(res * res * sizeof(float));
        memcpy(bytes.ptrw(), _cpu_buffer.data(), bytes.size());

        Ref<Image> img = Image::create_from_data(res, res, false, Image::FORMAT_RF, bytes);
        _forest_texture = ImageTexture::create_from_image(img);

        UtilityFunctions::print("JarForestMap: baked ", all_forest_cells.size(), " forest cells into ", res, "x", res, " texture.");
    }

    float sample_influence(float world_x, float world_z) const
    {
        if (_cpu_buffer.empty()) return 0.0f;

        int res = _texture_resolution;
        float half_world = _world_size * 0.5f;
        float u = (world_x + half_world) / _world_size;
        float v = (world_z + half_world) / _world_size;
        float px = u * res - 0.5f;
        float py = v * res - 0.5f;

        int x0 = std::clamp((int)std::floor(px), 0, res - 1);
        int y0 = std::clamp((int)std::floor(py), 0, res - 1);
        int x1 = std::min(x0 + 1, res - 1);
        int y1 = std::min(y0 + 1, res - 1);
        float fx = px - std::floor(px);
        float fy = py - std::floor(py);

        float v00 = _cpu_buffer[y0 * res + x0];
        float v10 = _cpu_buffer[y0 * res + x1];
        float v01 = _cpu_buffer[y1 * res + x0];
        float v11 = _cpu_buffer[y1 * res + x1];

        return glm::mix(glm::mix(v00, v10, fx), glm::mix(v01, v11, fx), fy);
    }

    Ref<ImageTexture> get_forest_texture() const { return _forest_texture; }

    void set_voronoi_noise(Ref<FastNoiseLite> n) { _voronoi_noise = n; }
    Ref<FastNoiseLite> get_voronoi_noise() const { return _voronoi_noise; }
    void set_ridge_map(Ref<JarRidgeMap> r) { _ridge_map = r; }
    Ref<JarRidgeMap> get_ridge_map() const { return _ridge_map; }
    void set_blob_count(int v) { _blob_count = v; }
    int  get_blob_count() const { return _blob_count; }
    void set_blob_size(int v) { _blob_size = v; }
    int  get_blob_size() const { return _blob_size; }
    void set_forest_falloff_radius(float v) { _forest_falloff_radius = v; }
    float get_forest_falloff_radius() const { return _forest_falloff_radius; }
    void set_texture_resolution(int v) { _texture_resolution = v; }
    int  get_texture_resolution() const { return _texture_resolution; }
    void set_world_size(float v) { _world_size = v; }
    float get_world_size() const { return _world_size; }
    void set_random_seed(int v) { _random_seed = v; }
    int  get_random_seed() const { return _random_seed; }

protected:
    static void _bind_methods()
    {
        ClassDB::bind_method(D_METHOD("sample_influence", "world_x", "world_z"), &JarForestMap::sample_influence);

        ClassDB::bind_method(D_METHOD("bake"), &JarForestMap::bake);
        ClassDB::bind_method(D_METHOD("get_forest_texture"), &JarForestMap::get_forest_texture);

        ClassDB::bind_method(D_METHOD("set_voronoi_noise", "n"), &JarForestMap::set_voronoi_noise);
        ClassDB::bind_method(D_METHOD("get_voronoi_noise"), &JarForestMap::get_voronoi_noise);
        ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "voronoi_noise",
            PROPERTY_HINT_RESOURCE_TYPE, "FastNoiseLite"),
            "set_voronoi_noise", "get_voronoi_noise");

        ClassDB::bind_method(D_METHOD("set_ridge_map", "r"), &JarForestMap::set_ridge_map);
        ClassDB::bind_method(D_METHOD("get_ridge_map"), &JarForestMap::get_ridge_map);
        ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "ridge_map",
            PROPERTY_HINT_RESOURCE_TYPE, "JarRidgeMap"),
            "set_ridge_map", "get_ridge_map");

        ClassDB::bind_method(D_METHOD("set_blob_count", "v"), &JarForestMap::set_blob_count);
        ClassDB::bind_method(D_METHOD("get_blob_count"), &JarForestMap::get_blob_count);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "blob_count",
            PROPERTY_HINT_RANGE, "1,20,1"),
            "set_blob_count", "get_blob_count");

        ClassDB::bind_method(D_METHOD("set_blob_size", "v"), &JarForestMap::set_blob_size);
        ClassDB::bind_method(D_METHOD("get_blob_size"), &JarForestMap::get_blob_size);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "blob_size",
            PROPERTY_HINT_RANGE, "1,20,1"),
            "set_blob_size", "get_blob_size");

        ClassDB::bind_method(D_METHOD("set_forest_falloff_radius", "v"), &JarForestMap::set_forest_falloff_radius);
        ClassDB::bind_method(D_METHOD("get_forest_falloff_radius"), &JarForestMap::get_forest_falloff_radius);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "forest_falloff_radius",
            PROPERTY_HINT_RANGE, "100.0,2000.0,10.0"),
            "set_forest_falloff_radius", "get_forest_falloff_radius");

        ClassDB::bind_method(D_METHOD("set_texture_resolution", "v"), &JarForestMap::set_texture_resolution);
        ClassDB::bind_method(D_METHOD("get_texture_resolution"), &JarForestMap::get_texture_resolution);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_resolution",
            PROPERTY_HINT_RANGE, "64,2048,64"),
            "set_texture_resolution", "get_texture_resolution");

        ClassDB::bind_method(D_METHOD("set_world_size", "v"), &JarForestMap::set_world_size);
        ClassDB::bind_method(D_METHOD("get_world_size"), &JarForestMap::get_world_size);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "world_size"),
            "set_world_size", "get_world_size");

        ClassDB::bind_method(D_METHOD("set_random_seed", "v"), &JarForestMap::set_random_seed);
        ClassDB::bind_method(D_METHOD("get_random_seed"), &JarForestMap::get_random_seed);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "random_seed"),
            "set_random_seed", "get_random_seed");
    }
};

#endif // JAR_FOREST_MAP_H