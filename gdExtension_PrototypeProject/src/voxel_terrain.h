#ifndef VOXEL_TERRAIN_H
#define VOXEL_TERRAIN_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/templates/list.hpp> 
#include <mutex> 
#include <vector>
#include "terrain_math.h"

using namespace godot;

class VoxelOctreeNode;

// Structure to pass result back to main thread
struct MeshResult {
    VoxelOctreeNode* node;
    Ref<ArrayMesh> mesh;
};

class VoxelTerrain : public Node3D {
    GDCLASS(VoxelTerrain, Node3D)

private:
    TerrainMath* generator;
    VoxelOctreeNode* root;

    float root_size = 2048.0f;
    float min_leaf_size = 32.0f;

    std::vector<PathSegment> global_paths;

    Ref<Material> material;

    // THREADING MAILBOX
    std::mutex completion_mutex;
    std::list<MeshResult> completed_meshes;

protected:
    static void _bind_methods();

public:
    VoxelTerrain();
    ~VoxelTerrain();

    void _process(double delta) override;
    void _ready() override;

    void set_material(const Ref<Material>& p_material) { material = p_material; }
    Ref<Material> get_material() const { return material; }

    void add_path(Vector3 start, Vector3 end, float width);

    TerrainMath* get_generator() { return generator; }

    // Thread calls this when done
    void submit_mesh_result(VoxelOctreeNode* node, Ref<ArrayMesh> mesh);
};

class VoxelOctreeNode {
public:
    Vector3 center;
    float size;
    VoxelOctreeNode* children[8];
    bool is_leaf;
    bool mesh_dirty;

    MeshInstance3D* mesh_instance;

    // Threading flags
    int64_t task_id = -1;
    bool is_generating = false;

    VoxelOctreeNode(Vector3 p_center, float p_size);
    ~VoxelOctreeNode();

    void update_lod(Vector3 player_pos, float min_size, VoxelTerrain* system);

    // The Thread Functions
    void request_mesh_generation(VoxelTerrain* system);
    static void _mesh_task_function(void* p_userdata);
    Ref<ArrayMesh> generate_mesh_data(VoxelTerrain* system);

    // Helper to receive the mesh
    void apply_mesh(Ref<ArrayMesh> mesh, Node3D* parent);
};

#endif