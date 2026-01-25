#include "register_types.h"

// Include the headers for any class you want to expose to Godot
#include "voxel_terrain.h" 

#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

void initialize_voxel_terrain_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    // REGISTER YOUR CLASSES HERE
    ClassDB::register_class<VoxelTerrain>();

    // Note: We don't register VoxelOctreeNode because it's a helper class 
    // managed internally by C++, not used directly by Godot Nodes.
}

void uninitialize_voxel_terrain_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
}

extern "C" {
    // Initialization.
    // NOTE: The function name here must match the "entry_symbol" in your .gdextension file!
    GDExtensionBool GDE_EXPORT voxel_terrain_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization* r_initialization) {
        godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

        init_obj.register_initializer(initialize_voxel_terrain_module);
        init_obj.register_terminator(uninitialize_voxel_terrain_module);
        init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

        return init_obj.init();
    }
}