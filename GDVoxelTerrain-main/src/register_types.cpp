#include "register_types.h"
#include "box_sdf.h"
#include "plane_sdf.h"
#include "sphere_sdf.h"
#include "biome/jar_biome.h"
#include "biome/jar_meadow_biome.h"
#include "biome/jar_forest_biome.h"
#include "biome/jar_mountain_biome.h"
#include "biome/jar_ridge_map.h"
#include "biome/jar_forest_map.h"
#include "terrain_sdf.h"
#include "voxel_chunk.h"
#include "voxel_terrain.h"
using namespace godot;

void initialize_jar_voxel_terrain_module(ModuleInitializationLevel p_level)
{
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE)
    {
        // SDFs
        GDREGISTER_ABSTRACT_CLASS(JarSignedDistanceField);
        GDREGISTER_CLASS(JarBoxSdf);
        GDREGISTER_CLASS(JarSphereSdf);
        GDREGISTER_CLASS(JarPlaneSdf);
        GDREGISTER_CLASS(JarTerrainSdf);

        // TERRAIN
        GDREGISTER_CLASS(JarVoxelTerrain);
        GDREGISTER_CLASS(JarVoxelChunk);

        // BIOMES
        GDREGISTER_ABSTRACT_CLASS(JarBiome);
        GDREGISTER_CLASS(JarMeadowBiome);
        GDREGISTER_CLASS(JarForestBiome);
        GDREGISTER_CLASS(JarMountainBiome);
        GDREGISTER_CLASS(JarRidgeMap);
        GDREGISTER_CLASS(JarForestMap);
    }
}

void uninitialize_jar_voxel_terrain_module(ModuleInitializationLevel p_level)
{
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE)
    {
    }
}

extern "C"
{
    // Initialization.
    GDExtensionBool GDE_EXPORT jar_voxel_terrain_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address,
                                                              const GDExtensionClassLibraryPtr p_library,
                                                              GDExtensionInitialization *r_initialization)
    {
        godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

        init_obj.register_initializer(initialize_jar_voxel_terrain_module);
        init_obj.register_terminator(uninitialize_jar_voxel_terrain_module);
        init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

        return init_obj.init();
    }
}