#include "voxel_chunk.h"
#include "voxel_terrain/voxel_terrain.h"
#include "voxel_terrain/voxel_octree_node.h"
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>

void JarVoxelChunk::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("get_mesh_instance"), &JarVoxelChunk::get_mesh_instance);
    ClassDB::bind_method(D_METHOD("set_mesh_instance", "mesh_instance"), &JarVoxelChunk::set_mesh_instance);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesh_instance", PROPERTY_HINT_NODE_TYPE, "MeshInstance3D"),
                 "set_mesh_instance", "get_mesh_instance");

    ClassDB::bind_method(D_METHOD("get_collision_shape"), &JarVoxelChunk::get_collision_shape);
    ClassDB::bind_method(D_METHOD("set_collision_shape", "collision_shape"), &JarVoxelChunk::set_collision_shape);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "collision_shape", PROPERTY_HINT_NODE_TYPE, "CollisionShape3D"),
                 "set_collision_shape", "get_collision_shape");

    ClassDB::bind_method(D_METHOD("get_static_body"), &JarVoxelChunk::get_static_body);
    ClassDB::bind_method(D_METHOD("set_static_body", "static_body"), &JarVoxelChunk::set_static_body);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "static_body", PROPERTY_HINT_NODE_TYPE, "StaticBody3D"),
                 "set_static_body", "get_static_body");

    ClassDB::bind_method(D_METHOD("get_collider_lod_threshold"), &JarVoxelChunk::get_collider_lod_threshold);
    ClassDB::bind_method(D_METHOD("set_collider_lod_threshold", "collider_lod_threshold"),
                         &JarVoxelChunk::set_collider_lod_threshold);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "collider_lod_threshold"), "set_collider_lod_threshold",
                 "get_collider_lod_threshold");
}

JarVoxelChunk::JarVoxelChunk() : lod(0), edge_chunk(false)
{
}

JarVoxelChunk::~JarVoxelChunk()
{
}

int JarVoxelChunk::get_lod() const
{
    return lod;
}

void JarVoxelChunk::set_lod(int p_lod)
{
    lod = p_lod;
}

int JarVoxelChunk::get_collider_lod_threshold() const
{
    return collider_lod_threshold;
}

void JarVoxelChunk::set_collider_lod_threshold(int p_collider_lod_threshold)
{
    collider_lod_threshold = p_collider_lod_threshold;
}

uint8_t JarVoxelChunk::get_boundaries() const
{
    return boundaries;
}

void JarVoxelChunk::set_boundaries(uint8_t p_h2lboundaries)
{
    boundaries = p_h2lboundaries;
}

bool JarVoxelChunk::is_edge_chunk() const
{
    return edge_chunk;
}

void JarVoxelChunk::set_edge_chunk(bool p_edge_chunk)
{
    edge_chunk = p_edge_chunk;
}

MeshInstance3D *JarVoxelChunk::get_mesh_instance() const
{
    return mesh_instance;
}

void JarVoxelChunk::set_mesh_instance(MeshInstance3D *p_mesh_instance)
{
    mesh_instance = p_mesh_instance;
}

CollisionShape3D *JarVoxelChunk::get_collision_shape() const
{
    return collision_shape;
}

void JarVoxelChunk::set_collision_shape(CollisionShape3D *p_collision_shape)
{
    collision_shape = p_collision_shape;
}

StaticBody3D *JarVoxelChunk::get_static_body() const
{
    return static_body;
}

void JarVoxelChunk::set_static_body(StaticBody3D *p_static_body)
{
    static_body = p_static_body;
}

Ref<ArrayMesh> JarVoxelChunk::get_array_mesh() const
{
    return array_mesh;
}

void JarVoxelChunk::set_array_mesh(Ref<ArrayMesh> p_array_mesh)
{
    array_mesh = p_array_mesh;
}

Ref<ConcavePolygonShape3D> JarVoxelChunk::get_concave_polygon_shape() const
{
    return concave_polygon_shape;
}

void JarVoxelChunk::set_concave_polygon_shape(Ref<ConcavePolygonShape3D> p_concave_polygon_shape)
{
    concave_polygon_shape = p_concave_polygon_shape;
}

Ref<ShaderMaterial> JarVoxelChunk::get_material() const
{
    return material;
}

void JarVoxelChunk::set_material(Ref<ShaderMaterial> p_material)
{
    material = p_material;
}


void JarVoxelChunk::update_chunk(JarVoxelTerrain &terrain, VoxelOctreeNode *node, ChunkMeshData *chunk_mesh_data)
{
    _chunk_mesh_data = chunk_mesh_data;
    array_mesh = Ref<ArrayMesh>(Object::cast_to<ArrayMesh>(*mesh_instance->get_mesh()));
    concave_polygon_shape =
        Ref<ConcavePolygonShape3D>(Object::cast_to<ConcavePolygonShape3D>(*collision_shape->get_shape()));
    material = Ref<ShaderMaterial>(Object::cast_to<ShaderMaterial>(*mesh_instance->get_material_override()));
    lod = chunk_mesh_data->lod;
    boundaries = chunk_mesh_data->boundaries;
    edge_chunk = chunk_mesh_data->edge_chunk;
    auto old_bounds = bounds;
    bounds = chunk_mesh_data->bounds;
    auto position = bounds.get_center();
    // auto position = bounds.get_center() * 1.05f;
    set_position({position.x, position.y, position.z});

    array_mesh->clear_surfaces();
    array_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, chunk_mesh_data->mesh_array);

    bool generate_collider = lod <= collider_lod_threshold;

    if (generate_collider)
    {
        // collision_shape->set_disabled(!chunk_mesh_data->has_collision_mesh());
        // concave_polygon_shape->set_faces(chunk_mesh_data.create_collision_mesh());
        terrain.enqueue_chunk_collider(node);
    }
    else
    {
        collision_shape->set_disabled(true);
    }
}

void JarVoxelChunk::update_collision_mesh()
{
    // if(is_queued_for_deletion()) return;
    collision_shape->set_disabled(false);
    concave_polygon_shape->set_faces(_chunk_mesh_data->create_collision_mesh());
}

void JarVoxelChunk::delete_chunk()
{
    // Implementation of UpdateDetailMeshes(0) not shown
}
