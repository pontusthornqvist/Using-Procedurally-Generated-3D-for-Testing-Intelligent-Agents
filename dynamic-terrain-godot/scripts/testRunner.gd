extends Node3D

@onready var generator = $TerrainGenerator # Make sure your generator node is named exactly this

func _ready():
	# 1. Generate the data for Chunk (0,0)
	var data = generator.generate_chunk(Vector2i(0, 0))
	
	# 2. Create the Mesh Instance
	var mesh_inst = MeshInstance3D.new()
	mesh_inst.mesh = data["mesh"]
	add_child(mesh_inst)
	
	# 3. Create a DEBUG Material (Standard, not the Grass Shader yet)
	var debug_mat = StandardMaterial3D.new()
	
	# Use the generated color map (The green/brown texture) as the Albedo
	var tex_color = ImageTexture.create_from_image(data["colormap"])
	debug_mat.albedo_texture = tex_color
	
	# Set roughness to 1.0 so it looks like dirt/grass, not plastic
	debug_mat.roughness = 1.0 
	
	mesh_inst.material_override = debug_mat
	
	# 4. Position Camera (Optional: Moves the camera you added to a good spot)
	var cam = get_viewport().get_camera_3d()
	if cam:
		cam.global_position = Vector3(32, 200, 32) # Center of the chunk, high up
		cam.look_at(Vector3(32, 0, 32))
