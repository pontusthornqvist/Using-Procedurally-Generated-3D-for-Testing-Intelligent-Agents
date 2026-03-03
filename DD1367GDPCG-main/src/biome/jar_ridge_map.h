#ifndef JAR_RIDGE_MAP_H
#define JAR_RIDGE_MAP_H

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <unordered_set>
#include <random>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

using namespace godot;

class JarRidgeMap : public Resource
{
    GDCLASS(JarRidgeMap, Resource);

private:
    // Inspector properties
    Ref<FastNoiseLite> _voronoi_noise;  // same noise as biome_noise in TerrainSdf
    int    _ridge_count = 4;    // number of separate ridge chains
    int    _ridge_length = 6;    // cells per ridge chain
    float  _ridge_falloff_radius = 600.0f; // world units - must satisfy SDF slope constraint
    int    _texture_resolution = 512;
    float  _world_size = 4096.0f; // must match octree world size
    int    _random_seed = 42;

    // Runtime data
    Ref<ImageTexture> _ridge_texture;
    std::vector<float> _cpu_buffer; // kept for fast CPU sampling in sample_height()

    // Finds the Voronoi cell center nearest to a world position by hill-climbing
    // the voronoi noise gradient. Returns cell center in world space.
    glm::vec2 find_cell_center(const glm::vec2& world_pos) const
    {
        if (_voronoi_noise.is_null()) return world_pos;
        glm::vec2 p = world_pos;
        const float step = _world_size / _texture_resolution;
        // Gradient ascent toward cell center (cell value peaks at center)
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

    // Returns all unique cell centers within a search radius of a given point
    std::vector<glm::vec2> find_adjacent_cells(const glm::vec2& cell_center, float search_radius) const
    {
        std::vector<glm::vec2> result;
        const float step = search_radius / 3.0f;
        std::vector<glm::vec2> candidates;

        // Sample on a grid around the cell center
        for (float dx = -search_radius; dx <= search_radius; dx += step)
            for (float dy = -search_radius; dy <= search_radius; dy += step)
            {
                glm::vec2 candidate = find_cell_center(cell_center + glm::vec2(dx, dy));
                // Deduplicate - if this center is close to an existing one, skip
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

    void grow_ridge(const glm::vec2& seed, const glm::vec2& direction,
        std::vector<glm::vec2>& ridge_cells) const
    {
        glm::vec2 current = seed;
        glm::vec2 dir = glm::normalize(direction);
        const float cell_search_radius = _world_size / 8.0f;

        ridge_cells.push_back(current);

        for (int i = 1; i < _ridge_length; i++)
        {
            auto adjacent = find_adjacent_cells(current, cell_search_radius);
            if (adjacent.empty()) break;

            // Pick the adjacent cell whose direction from current best matches dir
            // with a dot product threshold to allow slight curves
            glm::vec2 best_cell = current;
            float best_dot = -1.0f;
            for (auto& candidate : adjacent)
            {
                // Skip cells already in ridge
                bool already_in_ridge = false;
                for (auto& existing : ridge_cells)
                    if (glm::distance(candidate, existing) < cell_search_radius * 0.3f)
                    {
                        already_in_ridge = true; break;
                    }
                if (already_in_ridge) continue;

                glm::vec2 to_candidate = glm::normalize(candidate - current);
                float dot = glm::dot(to_candidate, dir);
                if (dot > 0.3f && dot > best_dot) // must be roughly forward
                {
                    best_dot = dot;
                    best_cell = candidate;
                    // Update direction slightly toward new cell for gentle curves
                    dir = glm::normalize(dir * 0.7f + to_candidate * 0.3f);
                }
            }
            if (best_dot < 0.0f) break; // no valid continuation found
            current = best_cell;
            ridge_cells.push_back(current);
        }
    }

public:
    JarRidgeMap() {}

    void bake()
    {
        if (_voronoi_noise.is_null())
        {
            UtilityFunctions::printerr("JarRidgeMap: voronoi_noise is not set.");
            return;
        }

        // Validate SDF slope constraint
        // Max gradient from texture must not exceed our cap of sqrt(0.25) = 0.5
        // height change over falloff = _heightScale * 1.0 (full range)
        // slope = heightScale / falloff_radius, must be <= 0.5
        // => falloff_radius >= heightScale / 0.5 = heightScale * 2
        // We don't have heightScale here so just warn if radius seems too small
        if (_ridge_falloff_radius < 400.0f)
            UtilityFunctions::print("JarRidgeMap: ridge_falloff_radius may be too small for smooth SDF gradients. Consider >= 500.");

        std::mt19937 rng(_random_seed);
        std::uniform_real_distribution<float> world_dist(-_world_size * 0.4f, _world_size * 0.4f);
        std::uniform_real_distribution<float> angle_dist(0.0f, glm::pi<float>() * 2.0f);

        // --- Step 1: Grow ridge chains ---
        std::vector<glm::vec2> all_ridge_cells;

        for (int r = 0; r < _ridge_count; r++)
        {
            // Pick random seed position and snap to its Voronoi cell center
            glm::vec2 seed_pos(world_dist(rng), world_dist(rng));
            glm::vec2 seed_center = find_cell_center(seed_pos);

            // Random direction for this ridge
            float angle = angle_dist(rng);
            glm::vec2 direction(std::cos(angle), std::sin(angle));

            std::vector<glm::vec2> ridge_cells;
            grow_ridge(seed_center, direction, ridge_cells);
            all_ridge_cells.insert(all_ridge_cells.end(), ridge_cells.begin(), ridge_cells.end());
        }

        // --- Step 2: Bake distance field into CPU buffer ---
        int res = _texture_resolution;
        _cpu_buffer.resize(res * res, 0.0f);

        float half_world = _world_size * 0.5f;

        for (int py = 0; py < res; py++)
        {
            for (int px = 0; px < res; px++)
            {
                // World position for this pixel
                float wx = (px / float(res)) * _world_size - half_world;
                float wy = (py / float(res)) * _world_size - half_world;
                glm::vec2 world_pos(wx, wy);

                // Find closest ridge cell center
                float min_dist = std::numeric_limits<float>::max();
                for (auto& cell : all_ridge_cells)
                    min_dist = std::min(min_dist, glm::distance(world_pos, cell));

                // Convert distance to influence [0,1] with smooth falloff
                // At dist=0 (cell center): influence=1. At dist=falloff_radius: influence=0.
                float t = std::clamp(min_dist / _ridge_falloff_radius, 0.0f, 1.0f);

                // Smoothstep for C1 continuity - no kinks at the boundary
                // This guarantees the gradient is zero at both ends
                float influence = 1.0f - (t * t * (3.0f - 2.0f * t));

                _cpu_buffer[py * res + px] = influence;
            }
        }

        // --- Step 3: Upload to ImageTexture ---
        // Use RF (single channel float) format for precision
        PackedByteArray bytes;
        bytes.resize(res * res * sizeof(float));
        memcpy(bytes.ptrw(), _cpu_buffer.data(), bytes.size());

        Ref<Image> img = Image::create_from_data(res, res, false, Image::FORMAT_RF, bytes);
        _ridge_texture = ImageTexture::create_from_image(img);

        UtilityFunctions::print("JarRidgeMap: baked ", all_ridge_cells.size(), " ridge cells into ", res, "x", res, " texture.");
    }

    // Called from JarTerrainSdf::sample_height() - fast CPU bilinear sample
    float sample_influence(float world_x, float world_z) const
    {
        if (_cpu_buffer.empty()) return 0.0f;

        int res = _texture_resolution;
        float half_world = _world_size * 0.5f;

        // World to UV
        float u = (world_x + half_world) / _world_size;
        float v = (world_z + half_world) / _world_size;

        // UV to pixel coords
        float px = u * res - 0.5f;
        float py = v * res - 0.5f;

        int x0 = std::clamp((int)std::floor(px), 0, res - 1);
        int y0 = std::clamp((int)std::floor(py), 0, res - 1);
        int x1 = std::min(x0 + 1, res - 1);
        int y1 = std::min(y0 + 1, res - 1);

        float fx = px - std::floor(px);
        float fy = py - std::floor(py);

        // Bilinear interpolation
        float v00 = _cpu_buffer[y0 * res + x0];
        float v10 = _cpu_buffer[y0 * res + x1];
        float v01 = _cpu_buffer[y1 * res + x0];
        float v11 = _cpu_buffer[y1 * res + x1];

        return glm::mix(glm::mix(v00, v10, fx), glm::mix(v01, v11, fx), fy);
    }

    Ref<ImageTexture> get_ridge_texture() const { return _ridge_texture; }

    // Getters/setters
    void set_voronoi_noise(Ref<FastNoiseLite> n) { _voronoi_noise = n; }
    Ref<FastNoiseLite> get_voronoi_noise() const { return _voronoi_noise; }
    void set_ridge_count(int v) { _ridge_count = v; }
    int  get_ridge_count() const { return _ridge_count; }
    void set_ridge_length(int v) { _ridge_length = v; }
    int  get_ridge_length() const { return _ridge_length; }
    void set_ridge_falloff_radius(float v) { _ridge_falloff_radius = v; }
    float get_ridge_falloff_radius() const { return _ridge_falloff_radius; }
    void set_texture_resolution(int v) { _texture_resolution = v; }
    int  get_texture_resolution() const { return _texture_resolution; }
    void set_world_size(float v) { _world_size = v; }
    float get_world_size() const { return _world_size; }
    void set_random_seed(int v) { _random_seed = v; }
    int  get_random_seed() const { return _random_seed; }

protected:
    static void _bind_methods()
    {
        ClassDB::bind_method(D_METHOD("sample_influence", "world_x", "world_z"), &JarRidgeMap::sample_influence);

        ClassDB::bind_method(D_METHOD("bake"), &JarRidgeMap::bake);
        ClassDB::bind_method(D_METHOD("get_ridge_texture"), &JarRidgeMap::get_ridge_texture);

        ClassDB::bind_method(D_METHOD("set_voronoi_noise", "n"), &JarRidgeMap::set_voronoi_noise);
        ClassDB::bind_method(D_METHOD("get_voronoi_noise"), &JarRidgeMap::get_voronoi_noise);
        ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "voronoi_noise",
            PROPERTY_HINT_RESOURCE_TYPE, "FastNoiseLite"),
            "set_voronoi_noise", "get_voronoi_noise");

        ClassDB::bind_method(D_METHOD("set_ridge_count", "v"), &JarRidgeMap::set_ridge_count);
        ClassDB::bind_method(D_METHOD("get_ridge_count"), &JarRidgeMap::get_ridge_count);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "ridge_count",
            PROPERTY_HINT_RANGE, "1,20,1"),
            "set_ridge_count", "get_ridge_count");

        ClassDB::bind_method(D_METHOD("set_ridge_length", "v"), &JarRidgeMap::set_ridge_length);
        ClassDB::bind_method(D_METHOD("get_ridge_length"), &JarRidgeMap::get_ridge_length);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "ridge_length",
            PROPERTY_HINT_RANGE, "2,20,1"),
            "set_ridge_length", "get_ridge_length");

        ClassDB::bind_method(D_METHOD("set_ridge_falloff_radius", "v"), &JarRidgeMap::set_ridge_falloff_radius);
        ClassDB::bind_method(D_METHOD("get_ridge_falloff_radius"), &JarRidgeMap::get_ridge_falloff_radius);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ridge_falloff_radius",
            PROPERTY_HINT_RANGE, "100.0,2000.0,10.0"),
            "set_ridge_falloff_radius", "get_ridge_falloff_radius");

        ClassDB::bind_method(D_METHOD("set_texture_resolution", "v"), &JarRidgeMap::set_texture_resolution);
        ClassDB::bind_method(D_METHOD("get_texture_resolution"), &JarRidgeMap::get_texture_resolution);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_resolution",
            PROPERTY_HINT_RANGE, "64,2048,64"),
            "set_texture_resolution", "get_texture_resolution");

        ClassDB::bind_method(D_METHOD("set_world_size", "v"), &JarRidgeMap::set_world_size);
        ClassDB::bind_method(D_METHOD("get_world_size"), &JarRidgeMap::get_world_size);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "world_size"),
            "set_world_size", "get_world_size");

        ClassDB::bind_method(D_METHOD("set_random_seed", "v"), &JarRidgeMap::set_random_seed);
        ClassDB::bind_method(D_METHOD("get_random_seed"), &JarRidgeMap::get_random_seed);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "random_seed"),
            "set_random_seed", "get_random_seed");
    }
};

#endif // JAR_RIDGE_MAP_H