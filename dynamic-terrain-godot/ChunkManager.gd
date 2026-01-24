class_name ChunkManager
extends Node3D

@export var terrain_generator: TerrainGenerator
@export var target: Node3D 
@export var render_distance: int = 2 
@export var chunk_size: int = 64 # Matches 'chunk_vertex_count' - 1 (65 verts = size 64 | 64 quads)

# SETTINGS FOR GRASS
@export var grass_mesh: Mesh # simple QuadMesh 
@export var grass_material: ShaderMaterial # Grass Shader Material here
@export var grass_density: int = 4000 # Clumps of grass per chunk
@export var grass_texture: Texture2D # Grass texture

var _active_chunks: Dictionary = {} 

func _process(_delta):
	if not target or not terrain_generator:
		return
	_update_chunks()

func _update_chunks():
	var player_pos = target.global_position
	var p_x = int(player_pos.x / chunk_size)
	var p_z = int(player_pos.z / chunk_size)
	
	if player_pos.x < 0: p_x -= 1
	if player_pos.z < 0: p_z -= 1
	
	var center_chunk = Vector2i(p_x, p_z)
	var needed_chunks = []
	
	for x in range(center_chunk.x - render_distance, center_chunk.x + render_distance + 1):
		for z in range(center_chunk.y - render_distance, center_chunk.y + render_distance + 1):
			needed_chunks.append(Vector2i(x, z))
	
	var chunks_to_remove = []
	for coord in _active_chunks.keys():
		if coord not in needed_chunks:
			chunks_to_remove.append(coord)
			
	for coord in chunks_to_remove:
		_despawn_chunk(coord)
		
	for coord in needed_chunks:
		if coord not in _active_chunks:
			_spawn_chunk(coord)

func _spawn_chunk(coord: Vector2i):
	# 1. GENERATE DATA
	var data = terrain_generator.generate_chunk(coord)
	var chunk_world_pos = Vector3(coord.x * chunk_size, 0, coord.y * chunk_size)
	
	# --- PART A: THE TERRAIN MESH ---
	var terrain_mesh_inst = MeshInstance3D.new()
	terrain_mesh_inst.mesh = data["mesh"]
	terrain_mesh_inst.position = chunk_world_pos
	
	# Simple Ground Material
	var terrain_mat = StandardMaterial3D.new()
	var tex_color = ImageTexture.create_from_image(data["colormap"])
	terrain_mat.albedo_texture = tex_color
	terrain_mat.roughness = 1.0
	terrain_mat.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS
	terrain_mesh_inst.material_override = terrain_mat
	
	add_child(terrain_mesh_inst)
	
	# --- PART B: THE GRASS MULTIMESH ---
	var grass_multi = MultiMeshInstance3D.new()
	grass_multi.multimesh = MultiMesh.new()
	grass_multi.multimesh.transform_format = MultiMesh.TRANSFORM_3D
	grass_multi.multimesh.mesh = grass_mesh
	grass_multi.multimesh.instance_count = grass_density
	grass_multi.position = chunk_world_pos
	
	# Create a unique material for this chunk's grass
	# (It needs unique textures for height/color)
	var this_chunk_grass_mat = grass_material.duplicate()
	
	var tex_height = ImageTexture.create_from_image(data["heightmap"])
	var tex_controls = ImageTexture.create_from_image(data["controlmap"])
	
	# Assign the specific textures this grass needs to read
	this_chunk_grass_mat.set_shader_parameter("u_terrain_heightmap", tex_height)
	this_chunk_grass_mat.set_shader_parameter("u_terrain_globalmap", tex_color)
	this_chunk_grass_mat.set_shader_parameter("u_terrain_detailmap", tex_controls)
	this_chunk_grass_mat.set_shader_parameter("u_albedo_alpha", grass_texture)
	
	# IMPORTANT: Tell the shader where this chunk is relative to the world
	# this shader uses "u_terrain_inverse_transform" to map world pos -> UV.
	var transform_rel = Transform3D(Basis(), chunk_world_pos)
	this_chunk_grass_mat.set_shader_parameter("u_terrain_inverse_transform", transform_rel.affine_inverse())
	
	grass_multi.material_override = this_chunk_grass_mat
	
	# Populate MultiMesh with random transforms (The shader handles height snapping)
	for i in range(grass_density):
		var t = Transform3D()
		# Random position within the chunk (0 to chunk_size)
		var r_pos = Vector3(randf() * chunk_size, 0, randf() * chunk_size)
		t.origin = r_pos
		grass_multi.multimesh.set_instance_transform(i, t)
		
	add_child(grass_multi)
	
	# Store references to clean up later
	_active_chunks[coord] = {
		"terrain": terrain_mesh_inst,
		"grass": grass_multi
	}

func _despawn_chunk(coord: Vector2i):
	if _active_chunks.has(coord):
		_active_chunks[coord]["terrain"].queue_free()
		_active_chunks[coord]["grass"].queue_free()
		_active_chunks.erase(coord)
