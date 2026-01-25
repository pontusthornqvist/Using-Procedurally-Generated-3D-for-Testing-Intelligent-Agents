#include "voxel_terrain.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/engine.hpp>

using namespace godot;

// --- VOXEL TERRAIN IMPLEMENTATION ---

void VoxelTerrain::_bind_methods() {
    ClassDB::bind_method(D_METHOD("add_path", "start", "end", "width"), &VoxelTerrain::add_path);
    ClassDB::bind_method(D_METHOD("set_material", "material"), &VoxelTerrain::set_material);
    ClassDB::bind_method(D_METHOD("get_material"), &VoxelTerrain::get_material);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "BaseMaterial3D,ShaderMaterial"), "set_material", "get_material");
}

VoxelTerrain::VoxelTerrain() {
    generator = new TerrainMath(12345);
    root = new VoxelOctreeNode(Vector3(0, 0, 0), root_size);
}

VoxelTerrain::~VoxelTerrain() {
    delete generator;
    delete root;
}

void VoxelTerrain::_ready() {
    // Ready logic
}

void VoxelTerrain::_process(double delta) {
    if (Engine::get_singleton()->is_editor_hint()) return;

    // 1. Process Finished Mesh Jobs (MAILBOX)
    // We lock, grab all pending meshes, and unlock quickly.
    std::list<MeshResult> results_to_process;
    {
        std::lock_guard<std::mutex> lock(completion_mutex);
        if (!completed_meshes.empty()) {
            results_to_process = completed_meshes;
            completed_meshes.clear();
        }
    }

    // Apply meshes safely on the Main Thread
    for (auto& res : results_to_process) {
        // Pass 'this' as the parent
        res.node->apply_mesh(res.mesh, this);
    }

    // 2. Update LOD (which triggers new threads)
    Vector3 player_pos = Vector3(0, 100, 0); // Placeholder
    root->update_lod(player_pos, min_leaf_size, this);
}

void VoxelTerrain::add_path(Vector3 start, Vector3 end, float width) {
    global_paths.push_back({ start, end, width, 10.0f });
}

void VoxelTerrain::submit_mesh_result(VoxelOctreeNode* node, Ref<ArrayMesh> mesh) {
    std::lock_guard<std::mutex> lock(completion_mutex);
    completed_meshes.push_back({ node, mesh });
}

// --- OCTREE NODE IMPLEMENTATION ---

VoxelOctreeNode::VoxelOctreeNode(Vector3 p_center, float p_size) {
    center = p_center;
    size = p_size;
    is_leaf = true;
    mesh_instance = nullptr;
    mesh_dirty = true;
    for (int i = 0; i < 8; i++) children[i] = nullptr;
}

VoxelOctreeNode::~VoxelOctreeNode() {
    if (mesh_instance) mesh_instance->queue_free();
    if (!is_leaf) {
        for (int i = 0; i < 8; i++) delete children[i];
    }
}

void VoxelOctreeNode::update_lod(Vector3 player_pos, float min_size, VoxelTerrain* system) {
    float dist = center.distance_to(player_pos);
    bool should_split = (dist < size * 1.5f) && (size > min_size);

    if (should_split) {
        if (is_leaf) {
            is_leaf = false;
            if (mesh_instance) {
                mesh_instance->queue_free();
                mesh_instance = nullptr;
            }

            float q = size / 4.0f;
            float half = size / 2.0f;
            int idx = 0;
            for (int z = -1; z <= 1; z += 2) {
                for (int y = -1; y <= 1; y += 2) {
                    for (int x = -1; x <= 1; x += 2) {
                        Vector3 offset = Vector3(x, y, z) * q;
                        children[idx] = new VoxelOctreeNode(center + offset, half);
                        idx++;
                    }
                }
            }
        }
        for (int i = 0; i < 8; i++) children[i]->update_lod(player_pos, min_size, system);
    }
    else {
        if (!is_leaf) {
            for (int i = 0; i < 8; i++) {
                delete children[i];
                children[i] = nullptr;
            }
            is_leaf = true;
            mesh_dirty = true;
        }
        if (is_leaf && mesh_dirty) {
            request_mesh_generation(system);
            mesh_dirty = false;
        }
    }
}

// Helper to linearly interpolate between two points
Vector3 interpolate_edge(Vector3 p1, Vector3 p2, float val1, float val2) {
    if (abs(val2 - val1) < 0.00001f) return p1; // Prevent div by zero
    float t = (0.0f - val1) / (val2 - val1);
    return p1 + (p2 - p1) * t;
}

struct MeshTaskData {
    VoxelOctreeNode* node;
    VoxelTerrain* system;
};

void VoxelOctreeNode::request_mesh_generation(VoxelTerrain* system) {
    if (is_generating) return;
    is_generating = true;

    MeshTaskData* data = new MeshTaskData{ this, system };
    task_id = WorkerThreadPool::get_singleton()->add_native_task(
        &VoxelOctreeNode::_mesh_task_function, (void*)data, true, "VoxelMeshGen"
    );
}

void VoxelOctreeNode::_mesh_task_function(void* p_userdata) {
    MeshTaskData* data = (MeshTaskData*)p_userdata;
    Ref<ArrayMesh> new_mesh = data->node->generate_mesh_data(data->system);

    // FIXED: Instead of call_deferred, submit to thread-safe queue
    data->system->submit_mesh_result(data->node, new_mesh);

    delete data;
}

void VoxelOctreeNode::apply_mesh(Ref<ArrayMesh> mesh, Node3D* parent) {
    is_generating = false;

    if (mesh.is_valid()) {
        // 1. Create if missing
        if (!mesh_instance) {
            mesh_instance = memnew(MeshInstance3D);

            // Important: We cast to VoxelTerrain to maintain strictness, 
            // or just use Node3D parent.
            parent->add_child(mesh_instance);

            // Set position to global zero because your vertices are already World Space
            mesh_instance->set_global_position(Vector3(0, 0, 0));
        }

        // 2. Assign Mesh
        mesh_instance->set_mesh(mesh);

        // 3. Shadow settings
        mesh_instance->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_ON);
    }

    // Apply Material from the Parent VoxelTerrain
    VoxelTerrain* terrain = Object::cast_to<VoxelTerrain>(parent);
    if (terrain && terrain->get_material().is_valid()) {
        mesh_instance->set_material_override(terrain->get_material());
    }
}

// In voxel_terrain.cpp

Ref<ArrayMesh> VoxelOctreeNode::generate_mesh_data(VoxelTerrain* system) {
    // 1. SETTINGS
    const int RES = 32;
    // Padding of 1 on all sides: 0..RES+2
    const int GRID_SIZE = RES + 3;

    float step = size / (float)RES;
    // Start position shifted to account for the padding (starting at -1)
    Vector3 start_pos = center - Vector3(size / 2, size / 2, size / 2);

    // Flattened density array
    std::vector<float> densities(GRID_SIZE * GRID_SIZE * GRID_SIZE);
    TerrainMath* tm = system->get_generator();
    std::vector<PathSegment> paths;

    // --- PASS 0: SAMPLING (Full Grid 0 to GRID_SIZE) ---
    for (int z = 0; z < GRID_SIZE; z++) {
        for (int y = 0; y < GRID_SIZE; y++) {
            for (int x = 0; x < GRID_SIZE; x++) {
                // (x-1) aligns index 1 with the start of the chunk
                Vector3 p = start_pos + Vector3(x - 1, y - 1, z - 1) * step;

                int idx = x + y * GRID_SIZE + z * GRID_SIZE * GRID_SIZE;
                float val = tm->get_density(p, paths);

                if (std::isnan(val)) val = 100.0f;
                densities[idx] = std::clamp(val, -10000.0f, 10000.0f);
            }
        }
    }

    // Map: Cell Index -> Vertex Index
    std::vector<int> cell_vertex_indices(GRID_SIZE * GRID_SIZE * GRID_SIZE, -1);
    SurfaceTool* st = memnew(SurfaceTool);
    st->begin(Mesh::PRIMITIVE_TRIANGLES);

    // --- PASS 1: VERTICES (Full Grid 0 to GRID_SIZE) ---
    // FIXED: We run this loop over the ENTIRE grid so that neighbors exist 
    // when Pass 2 looks for them.
    int current_vertex_count = 0;

    // We stop at GRID_SIZE-1 because Surface Nets looks at (x+1)
    for (int z = 0; z < GRID_SIZE - 1; z++) {
        for (int y = 0; y < GRID_SIZE - 1; y++) {
            for (int x = 0; x < GRID_SIZE - 1; x++) {

                int c_idx = x + y * GRID_SIZE + z * GRID_SIZE * GRID_SIZE;

                // Get 8 corners
                float d[8];
                d[0] = densities[c_idx];
                d[1] = densities[c_idx + 1];
                d[2] = densities[c_idx + GRID_SIZE];
                d[3] = densities[c_idx + 1 + GRID_SIZE];
                d[4] = densities[c_idx + (GRID_SIZE * GRID_SIZE)];
                d[5] = densities[c_idx + 1 + (GRID_SIZE * GRID_SIZE)];
                d[6] = densities[c_idx + GRID_SIZE + (GRID_SIZE * GRID_SIZE)];
                d[7] = densities[c_idx + 1 + GRID_SIZE + (GRID_SIZE * GRID_SIZE)];

                int mask = 0;
                for (int k = 0; k < 8; k++) if (d[k] > 0) mask |= (1 << k);
                if (mask == 0 || mask == 255) continue;

                Vector3 avg_pos = Vector3(0, 0, 0);
                int edge_count = 0;

                Vector3 cell_pos_world = start_pos + Vector3(x - 1, y - 1, z - 1) * step;

                // Edges (X, Y, Z)
                if ((d[0] > 0) != (d[1] > 0)) { avg_pos += interpolate_edge(Vector3(0, 0, 0), Vector3(step, 0, 0), d[0], d[1]); edge_count++; }
                if ((d[2] > 0) != (d[3] > 0)) { avg_pos += interpolate_edge(Vector3(0, step, 0), Vector3(step, step, 0), d[2], d[3]); edge_count++; }
                if ((d[4] > 0) != (d[5] > 0)) { avg_pos += interpolate_edge(Vector3(0, 0, step), Vector3(step, 0, step), d[4], d[5]); edge_count++; }
                if ((d[6] > 0) != (d[7] > 0)) { avg_pos += interpolate_edge(Vector3(0, step, step), Vector3(step, step, step), d[6], d[7]); edge_count++; }

                if ((d[0] > 0) != (d[2] > 0)) { avg_pos += interpolate_edge(Vector3(0, 0, 0), Vector3(0, step, 0), d[0], d[2]); edge_count++; }
                if ((d[1] > 0) != (d[3] > 0)) { avg_pos += interpolate_edge(Vector3(step, 0, 0), Vector3(step, step, 0), d[1], d[3]); edge_count++; }
                if ((d[4] > 0) != (d[6] > 0)) { avg_pos += interpolate_edge(Vector3(0, 0, step), Vector3(0, step, step), d[4], d[6]); edge_count++; }
                if ((d[5] > 0) != (d[7] > 0)) { avg_pos += interpolate_edge(Vector3(step, 0, step), Vector3(step, step, step), d[5], d[7]); edge_count++; }

                if ((d[0] > 0) != (d[4] > 0)) { avg_pos += interpolate_edge(Vector3(0, 0, 0), Vector3(0, 0, step), d[0], d[4]); edge_count++; }
                if ((d[1] > 0) != (d[5] > 0)) { avg_pos += interpolate_edge(Vector3(step, 0, 0), Vector3(step, 0, step), d[1], d[5]); edge_count++; }
                if ((d[2] > 0) != (d[6] > 0)) { avg_pos += interpolate_edge(Vector3(0, step, 0), Vector3(0, step, step), d[2], d[6]); edge_count++; }
                if ((d[3] > 0) != (d[7] > 0)) { avg_pos += interpolate_edge(Vector3(step, step, 0), Vector3(step, step, step), d[3], d[7]); edge_count++; }

                if (edge_count > 0) {
                    avg_pos /= (float)edge_count;
                    Vector3 final_pos = cell_pos_world + avg_pos;

                    st->set_normal(tm->get_normal(final_pos, paths));
                    st->set_uv(Vector2(final_pos.x, final_pos.z) * 0.05f);

                    st->add_vertex(final_pos);
                    cell_vertex_indices[c_idx] = current_vertex_count++;
                }
            }
        }
    }

    // --- PASS 2: FACES (Iterate safe inner region) ---
    // FIXED: Loop starts at 1 and ensures we don't go out of bounds
    // We run up to GRID_SIZE - 1 because we check (x-1).
    for (int z = 1; z < GRID_SIZE - 1; z++) {
        for (int y = 1; y < GRID_SIZE - 1; y++) {
            for (int x = 1; x < GRID_SIZE - 1; x++) {

                int c_idx = x + y * GRID_SIZE + z * GRID_SIZE * GRID_SIZE;
                float d_current = densities[c_idx];

                // Right Face (X+)
                float d_right = densities[c_idx + 1];
                if ((d_current > 0) != (d_right > 0)) {
                    int c1 = cell_vertex_indices[c_idx];
                    int c2 = cell_vertex_indices[c_idx - GRID_SIZE]; // y-1
                    int c3 = cell_vertex_indices[c_idx - GRID_SIZE - (GRID_SIZE * GRID_SIZE)]; // y-1, z-1
                    int c4 = cell_vertex_indices[c_idx - (GRID_SIZE * GRID_SIZE)]; // z-1

                    if (c1 != -1 && c2 != -1 && c3 != -1 && c4 != -1) {
                        if (d_current > 0) { st->add_index(c1); st->add_index(c4); st->add_index(c2); st->add_index(c2); st->add_index(c4); st->add_index(c3); }
                        else { st->add_index(c1); st->add_index(c2); st->add_index(c4); st->add_index(c2); st->add_index(c3); st->add_index(c4); }
                    }
                }

                // Top Face (Y+)
                float d_top = densities[c_idx + GRID_SIZE];
                if ((d_current > 0) != (d_top > 0)) {
                    int c1 = cell_vertex_indices[c_idx];
                    int c2 = cell_vertex_indices[c_idx - 1]; // x-1
                    int c3 = cell_vertex_indices[c_idx - 1 - (GRID_SIZE * GRID_SIZE)]; // x-1, z-1
                    int c4 = cell_vertex_indices[c_idx - (GRID_SIZE * GRID_SIZE)]; // z-1

                    if (c1 != -1 && c2 != -1 && c3 != -1 && c4 != -1) {
                        if (d_current > 0) { st->add_index(c1); st->add_index(c2); st->add_index(c4); st->add_index(c2); st->add_index(c3); st->add_index(c4); }
                        else { st->add_index(c1); st->add_index(c4); st->add_index(c2); st->add_index(c2); st->add_index(c4); st->add_index(c3); }
                    }
                }

                // Front Face (Z+)
                float d_front = densities[c_idx + (GRID_SIZE * GRID_SIZE)];
                if ((d_current > 0) != (d_front > 0)) {
                    int c1 = cell_vertex_indices[c_idx];
                    int c2 = cell_vertex_indices[c_idx - 1]; // x-1
                    int c3 = cell_vertex_indices[c_idx - 1 - GRID_SIZE]; // x-1, y-1
                    int c4 = cell_vertex_indices[c_idx - GRID_SIZE]; // y-1

                    if (c1 != -1 && c2 != -1 && c3 != -1 && c4 != -1) {
                        if (d_current > 0) { st->add_index(c1); st->add_index(c4); st->add_index(c2); st->add_index(c2); st->add_index(c4); st->add_index(c3); }
                        else { st->add_index(c1); st->add_index(c2); st->add_index(c4); st->add_index(c2); st->add_index(c3); st->add_index(c4); }
                    }
                }
            }
        }
    }

    Ref<ArrayMesh> mesh = st->commit();
    memdelete(st);

    if (mesh->get_surface_count() == 0) return Ref<ArrayMesh>();

    return mesh;
}