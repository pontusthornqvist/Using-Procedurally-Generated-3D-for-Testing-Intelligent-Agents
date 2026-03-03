#ifndef TERRAIN_SDF_H
#define TERRAIN_SDF_H

#include "signed_distance_field.h"
#include "biome/jar_biome.h"
#include "biome/jar_meadow_biome.h"
#include "biome/jar_forest_biome.h"
#include "biome/jar_mountain_biome.h"
#include "biome/jar_ridge_map.h"
#include "biome/jar_forest_map.h"

#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <algorithm>
#include <cmath>

using namespace godot;

class JarTerrainSdf : public JarSignedDistanceField
{
    GDCLASS(JarTerrainSdf, JarSignedDistanceField);

private:
    Ref<FastNoiseLite> _biomeNoise;
    Ref<JarRidgeMap> _ridgeMap;
    Ref<JarForestMap> _forestMap;
    Ref<FastNoiseLite> _shapeNoise;

    float _heightScale = 256.0f;

    // Biome array was removed from members but still referenced in sample_height
    TypedArray<JarBiome> _biomes;

    float _gradientEpsilon = 1.0f;
    //const float InvEps = 1.0f / _gradientEpsilon;

protected:

    Vector2 sample_gradient(const Vector2& pos, float height) const
    {
        float invEps = 1.0f / _gradientEpsilon;
        float heightX = sample_height(pos + Vector2(_gradientEpsilon, 0));
        float heightZ = sample_height(pos + Vector2(0, _gradientEpsilon));
        return Vector2((heightX - height) * invEps, (heightZ - height) * invEps);
    }

    virtual float distance(const glm::vec3& pos) const override
    {
        Vector2 samplePos(pos.x, pos.z);
        float height = sample_height(samplePos);
        Vector2 gradient = sample_gradient(samplePos, height);

        float grad_length_sq = gradient.x * gradient.x + gradient.y * gradient.y;

        // Cap at 0.25 (~26 degrees) rather than 2.0 (~55 degrees).
        // Above this angle the gradient correction does more harm than good
        // for octree subdivision accuracy - better to slightly overestimate
        // distance on gentle slopes than to underestimate it on steep ones.
        grad_length_sq = std::min(grad_length_sq, 0.25f);

        return (pos.y - height) / std::sqrt(1.0f + grad_length_sq);
    }

    static void _bind_methods()
    {
        ClassDB::bind_method(D_METHOD("sample_height", "pos"), &JarTerrainSdf::sample_height);

        ClassDB::bind_method(D_METHOD("set_biome_noise", "noise"), &JarTerrainSdf::set_biome_noise);
        ClassDB::bind_method(D_METHOD("get_biome_noise"), &JarTerrainSdf::get_biome_noise);
        ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "biome_noise",
            PROPERTY_HINT_RESOURCE_TYPE, "FastNoiseLite"),
            "set_biome_noise", "get_biome_noise");

        ClassDB::bind_method(D_METHOD("set_shape_noise", "noise"), &JarTerrainSdf::set_shape_noise);
        ClassDB::bind_method(D_METHOD("get_shape_noise"), &JarTerrainSdf::get_shape_noise);
        ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "shape_noise",
            PROPERTY_HINT_RESOURCE_TYPE, "FastNoiseLite"),
            "set_shape_noise", "get_shape_noise");

        ClassDB::bind_method(D_METHOD("set_height_scale", "v"), &JarTerrainSdf::set_height_scale);
        ClassDB::bind_method(D_METHOD("get_height_scale"), &JarTerrainSdf::get_height_scale);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height_scale"),
            "set_height_scale", "get_height_scale");

        ClassDB::bind_method(D_METHOD("set_biomes", "biomes"), &JarTerrainSdf::set_biomes);
        ClassDB::bind_method(D_METHOD("get_biomes"), &JarTerrainSdf::get_biomes);
        ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "biomes",
            PROPERTY_HINT_TYPE_STRING,
            String::num(Variant::OBJECT) + "/" +
            String::num(PROPERTY_HINT_RESOURCE_TYPE) + ":JarBiome"),
            "set_biomes", "get_biomes");

        ClassDB::bind_method(D_METHOD("set_gradient_epsilon", "v"), &JarTerrainSdf::set_gradient_epsilon);
        ClassDB::bind_method(D_METHOD("get_gradient_epsilon"), &JarTerrainSdf::get_gradient_epsilon);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "gradient_epsilon",
            PROPERTY_HINT_RANGE, "0.01,10.0,0.01"),
            "set_gradient_epsilon", "get_gradient_epsilon");

        ClassDB::bind_method(D_METHOD("set_ridge_map", "r"), &JarTerrainSdf::set_ridge_map);
        ClassDB::bind_method(D_METHOD("get_ridge_map"), &JarTerrainSdf::get_ridge_map);
        ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "ridge_map",
            PROPERTY_HINT_RESOURCE_TYPE, "JarRidgeMap"),
            "set_ridge_map", "get_ridge_map");

        ClassDB::bind_method(D_METHOD("set_forest_map", "f"), &JarTerrainSdf::set_forest_map);
        ClassDB::bind_method(D_METHOD("get_forest_map"), &JarTerrainSdf::get_forest_map);
        ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "forest_map",
            PROPERTY_HINT_RESOURCE_TYPE, "JarForestMap"),
            "set_forest_map", "get_forest_map");
    }

public:
    JarTerrainSdf() { }

    float sample_height(const Vector2& pos) const
    {
        if (_biomeNoise.is_null() || _shapeNoise.is_null()) return 0.0f;
        if (_biomes.size() < 3) return 0.0f;

        Ref<JarBiome> meadow = _biomes[0];
        Ref<JarBiome> forest = _biomes[1];
        Ref<JarBiome> mountain = _biomes[2];

        if (meadow.is_null() || forest.is_null() || mountain.is_null()) return 0.0f;

        float cell_value = _biomeNoise->get_noise_2d(pos.x, pos.y);
        float cell_edge_dist = std::abs(_shapeNoise->get_noise_2d(pos.x, pos.y));

        float ridge_influence = _ridgeMap.is_valid() ? _ridgeMap->sample_influence(pos.x, pos.y) : 0.0f;
        float forest_influence = _forestMap.is_valid() ? _forestMap->sample_influence(pos.x, pos.y) : 0.0f;

        // Base is pure meadow
        float base_h = meadow->get_height(pos, cell_value, cell_edge_dist);

        // Forest lifts from meadow base - uses forest_influence as blend weight
        float with_forest_h = glm::mix(base_h, forest->get_height(pos, cell_value, cell_edge_dist), forest_influence);

        // Ridge lifts from forest-blended base toward mountain height
        float final_h = glm::mix(with_forest_h, mountain->get_height(pos, cell_value, cell_edge_dist), ridge_influence);

        return _heightScale * final_h;
    }

    // Called by JarVoxelTerrain::initialize() before the octree builds
    void initialize()
    {
        if (_ridgeMap.is_valid())
            _ridgeMap->bake();
        if (_forestMap.is_valid())
            _forestMap->bake();
    }

    void set_biome_noise(Ref<FastNoiseLite> n) { _biomeNoise = n; }
    Ref<FastNoiseLite> get_biome_noise() const { return _biomeNoise; }

    void set_shape_noise(Ref<FastNoiseLite> n) { _shapeNoise = n; }
    Ref<FastNoiseLite> get_shape_noise() const { return _shapeNoise; }

    void set_height_scale(float v) { _heightScale = v; }
    float get_height_scale() const { return _heightScale; }

    void set_biomes(const TypedArray<JarBiome>& biomes) { _biomes = biomes; }
    TypedArray<JarBiome> get_biomes() const { return _biomes; }

    void set_gradient_epsilon(float v) { _gradientEpsilon = v; }
    float get_gradient_epsilon() const { return _gradientEpsilon; }

    void set_ridge_map(Ref<JarRidgeMap> r) { _ridgeMap = r; }
    Ref<JarRidgeMap> get_ridge_map() const { return _ridgeMap; }

    void set_forest_map(Ref<JarForestMap> f) { _forestMap = f; }
    Ref<JarForestMap> get_forest_map() const { return _forestMap; }
};

#endif // TERRAIN_SDF_H