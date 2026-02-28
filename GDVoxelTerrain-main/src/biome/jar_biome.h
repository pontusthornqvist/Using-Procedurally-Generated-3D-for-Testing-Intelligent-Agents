#ifndef JAR_BIOME_H
#define JAR_BIOME_H

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

class JarBiome : public Resource {
    GDCLASS(JarBiome, Resource);

private:
    float weight = 1.0f; // The inspector slider (0.0 means disabled)
    Ref<FastNoiseLite> detail_noise;

public:
    JarBiome() {}
    virtual ~JarBiome() = default;

    void set_weight(float p_weight) { weight = p_weight; }
    float get_weight() const { return weight; }

    void set_detail_noise(Ref<FastNoiseLite> p_noise) { detail_noise = p_noise; }
    Ref<FastNoiseLite> get_detail_noise() const { return detail_noise; }

    // Virtual function where the specific heuristic magic happens
    // global_pos: actual world coordinates
    // cell_center, cell_edge_dist: data provided by the master biome manager
    virtual float get_height(const Vector2& global_pos, float cell_center_val, float cell_edge_dist) const {
        return 0.0f; // Base implementation
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_weight", "weight"), &JarBiome::set_weight);
        ClassDB::bind_method(D_METHOD("get_weight"), &JarBiome::get_weight);
        // Hint range 0 to 1 allows for the slider you requested
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "weight", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_weight", "get_weight");

        ClassDB::bind_method(D_METHOD("set_detail_noise", "noise"), &JarBiome::set_detail_noise);
        ClassDB::bind_method(D_METHOD("get_detail_noise"), &JarBiome::get_detail_noise);
        ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "detail_noise", PROPERTY_HINT_RESOURCE_TYPE, "FastNoiseLite"), "set_detail_noise", "get_detail_noise");
    }
};

#endif // JAR_BIOME_H