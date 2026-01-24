class_name TerrainGenerator
extends Node

# --- SETTINGS ---
@export_group("Dimensions") 
@export var chunk_vertex_count: int = 65 # Keep this Power of 2 + 1 (e.g., 65, 129, 257)
@export var terrain_scale: float = 1.0

@export_group("Generation")
@export var height_noise: FastNoiseLite
@export var color_noise: FastNoiseLite # perlin at .01 frequency with ping-pong fractals
@export var max_height: float = 40.0 # Keep in mind chunk width is 64 (65 verts)

@export_group("Biomes & Coloring")
@export var grass_color_a: Color = Color("598e45") # Base Green
@export var grass_color_b: Color = Color("50895e") # Lighter green
@export var grass_color_c: Color = Color("517737") # Deep green
@export var grasspatch_noise_scale: float = 0.05
@export var cliff_color: Color = Color("5a4d41")   # Brown/Grey rock
@export var path_color: Color = Color("4f3e2e")    # Dirt path
@export var slope_threshold: float = 0.98           # 0.0 to 1.0 (Dot product)

@export_group("Erosion")
@export var erosion_iterations: int = 50000 
@export var erosion_shader: RDShaderFile # ASSIGN res://scripts/erosion.glsl HERE
@export var mesh_gen_shader: RDShaderFile # ASSIGN res://scripts/mesh_gen.glsl HERE

@export_group("Advanced Generation")
@export var warp_strength: float = 5.0 # How much we twist the terrain
@export var ridge_noise: FastNoiseLite # Assign a FastNoiseLite here (Fractal: Ridged)
@export var continent_noise: FastNoiseLite # For the Biome Mask (Plains vs Mountains)
@export var river_noise: FastNoiseLite    # Explicit noise for river paths

# --- INTERNALS ---
var _rd: RenderingDevice
var _erosion_pipeline: RID
var _mesh_pipeline: RID
var _erosion_shader_rid: RID
var _mesh_shader_rid: RID

# We calculate these ONCE and reuse them for every chunk
var _cached_indices: PackedInt32Array 
var _cached_uvs: PackedVector2Array

func _ready():
	_init_gpu()
	_precalculate_mesh_data()

func _init_gpu():
	_rd = RenderingServer.create_local_rendering_device()
	
	# Compile Erosion Shader
	var e_spirv = erosion_shader.get_spirv()
	_erosion_shader_rid = _rd.shader_create_from_spirv(e_spirv)
	_erosion_pipeline = _rd.compute_pipeline_create(_erosion_shader_rid)
	
	# Compile Mesh Gen Shader
	var m_spirv = mesh_gen_shader.get_spirv()
	_mesh_shader_rid = _rd.shader_create_from_spirv(m_spirv)
	_mesh_pipeline = _rd.compute_pipeline_create(_mesh_shader_rid)

func _precalculate_mesh_data():
	# 1. Generate Indices (The triangles never change structure)
	var indices = PackedInt32Array()
	var uvs = PackedVector2Array()
	var size = chunk_vertex_count
	
	for y in range(size):
		for x in range(size):
			# UVs
			uvs.append(Vector2(x, y) / float(size - 1))
			
			# Indices (Skip last row/col)
			if x < size - 1 and y < size - 1:
				var top_left = y * size + x
				var top_right = top_left + 1
				var bottom_left = (y + 1) * size + x
				var bottom_right = bottom_left + 1
				
				# Triangle 1 (flipped winding)
				indices.append(top_left)
				indices.append(top_right)  
				indices.append(bottom_left)
				
				# Triangle 2 (flipped winding)
				indices.append(top_right)
				indices.append(bottom_right) 
				indices.append(bottom_left)
				
	_cached_indices = indices
	_cached_uvs = uvs

func _generate_height_map_cpu(offset: Vector2) -> Image:
	var img = Image.create(chunk_vertex_count, chunk_vertex_count, false, Image.FORMAT_RF)
	
	for y in range(chunk_vertex_count):
		for x in range(chunk_vertex_count):
			var global_x = x + offset.x
			var global_y = y + offset.y
			
			# --- 1. BIOME MASK (The "Where are the mountains?" check) ---
			# Frequency of the noise should be LOW (e.g., 0.002) to create huge continents.
			var continent_val = continent_noise.get_noise_2d(global_x, global_y)
			
			# Normalize (-1..1 -> 0..1)
			var mask = (continent_val + 1.0) / 2.0
			
			# Sharpen the mask: Make flats flatter and mountains steeper
			# "smoothstep" creates a defined transition zone between plains and mountains
			mask = smoothstep(0.3, 0.6, mask) 
			
			# --- 2. DOMAIN WARPING (The "Flow" Look) ---
			var warp_x = color_noise.get_noise_2d(global_x, global_y) * warp_strength
			var warp_y = color_noise.get_noise_2d(global_x + 500, global_y + 500) * warp_strength
			
			var warped_x = global_x + warp_x
			var warped_y = global_y + warp_y
			
			# --- 3. MOUNTAIN SHAPE (Ridged Noise) ---
			# We use the warped coordinates here for that "crushed rock" look
			var mountain_shape = ridge_noise.get_noise_2d(warped_x, warped_y)
			
			# Ridge Math: Abs(noise) creates sharp creases. Invert to make them peaks.
			# Standard FastNoise Ridged is -1..1.
			mountain_shape = abs(mountain_shape) 
			mountain_shape = 1.0 - mountain_shape # Flip so 1.0 is the peak
			mountain_shape = mountain_shape * mountain_shape # Square it to make peaks sharper, valleys wider
			
			# --- NEW PLATEAU LOGIC ---
			# If the shape goes above 0.8 (80% max height), flatten it out.
			# This turns sharp peaks into flat-topped mesas/plateaus.
			var plateau_threshold = 0.2 # Setting this low and max height high seems to work well enough.
			if mountain_shape > plateau_threshold:
				# Smoothly blend into a flat top
				mountain_shape = lerp(mountain_shape, plateau_threshold, 0.8)
				mountain_shape = min(mountain_shape, 0.8) # Optional hard clamp
			
			# --- 4. COMBINE (Apply the Mask) ---
			# If mask is 0 (Plains), height is 0. If mask is 1 (Mountain zone), we get full height.
			# We add a little base noise so the plains aren't perfectly flat glass.
			var base_ground = height_noise.get_noise_2d(global_x, global_y) * 0.05
			var final_h = (mountain_shape * mask) + base_ground
			
			# --- 5. RIVER CARVING (Explicit Valleys) ---
			# We use a "Snake" noise. Values near 0.0 are rivers.
			if river_noise:
				var river_val = abs(river_noise.get_noise_2d(global_x, global_y))
				var river_width = 0.05
				
				if river_val < river_width:
					# 0.0 (Center of river) to 1.0 (Bank)
					var dig_factor = smoothstep(river_width, 0.0, river_val)
					
					# Smoothly subtract height to create a bed
					final_h -= dig_factor * 0.5 # Dig down relative to max height
			
			# Final Scale
			final_h *= max_height
			
			# Clamp to prevent going below 0 (into the void)
			final_h = max(0.0, final_h)
			
			img.set_pixel(x, y, Color(final_h, 0, 0, 1))
			
	return img

func _generate_color_map(height_map: Image, flux_map: Image, offset: Vector2) -> Dictionary:
	var color_img = Image.create(chunk_vertex_count, chunk_vertex_count, false, Image.FORMAT_RGBA8)
	# New image for controls (Red = Grass Density)
	var control_img = Image.create(chunk_vertex_count, chunk_vertex_count, false, Image.FORMAT_R8)
	
	var water_color = Color(0.1, 0.35, 0.8) 
	#var shore_color = Color(0.2, 0.25, 0.2)
	
	for y in range(chunk_vertex_count):
		for x in range(chunk_vertex_count):
			var global_x = x + offset.x
			var global_y = y + offset.y
			
			# 1. Terrain Calculations 
			var height_l = height_map.get_pixel(max(x - 1, 0), y).r
			var height_r = height_map.get_pixel(min(x + 1, chunk_vertex_count - 1), y).r
			var height_d = height_map.get_pixel(x, max(y - 1, 0)).r
			var height_u = height_map.get_pixel(x, min(y + 1, chunk_vertex_count - 1)).r
			
			var vec_x = Vector3(2.0 * terrain_scale, height_r - height_l, 0)
			var vec_z = Vector3(0, height_u - height_d, 2.0 * terrain_scale)
			var normal = vec_z.cross(vec_x).normalized()
			var up_dot = normal.dot(Vector3.UP)

			# --- 2-COLOR PATCH LOGIC  ---
			var patch_x = global_x * grasspatch_noise_scale
			var patch_y = global_y * grasspatch_noise_scale
			
			# Sample Noise (-1.0 to 1.0 usually)
			var patch_val_1 = color_noise.get_noise_2d(patch_x, patch_y)
			var patch_val_2 = color_noise.get_noise_2d(patch_x + 123.4, patch_y + 567.8)
			
			# Start with Base A
			var base_col = grass_color_a
			
			# Apply Patch B (Soft Blend)
			# Instead of "if > 0.0", we blend smoothly from -0.2 to 0.2
			# Values below -0.2 are pure A. Values above 0.2 are pure B.
			# The range -0.2 to 0.2 is the gradient.
			var blend_b = smoothstep(-0.2, 0.2, patch_val_1)
			base_col = base_col.lerp(grass_color_b, blend_b)
			
			# Apply Patch C (Soft Blend on top)
			var blend_c = smoothstep(-0.2, 0.2, patch_val_2)
			base_col = base_col.lerp(grass_color_c, blend_c)
			
			# Track if this pixel is allowed to have grass
			var grass_allowed = true
			
			# Cliff Check
			if up_dot < slope_threshold:
				var slope_blend = remap(up_dot, slope_threshold, slope_threshold - 0.2, 0.0, 1.0)
				slope_blend = clamp(slope_blend, 0.0, 1.0)
				base_col = base_col.lerp(cliff_color, slope_blend)
				# If it's mostly cliff, no grass
				if slope_blend > 0.5: grass_allowed = false

			# 3. FLUX LOGIC (Sediment)
			var flux = flux_map.get_pixel(x, y).r
			if flux > 10.0:
				#base_col = base_col.lerp(shore_color, 0.5)
				# Optional: reduce grass on very muddy spots?
				if flux > 50.0: grass_allowed = false

			# 4. RIVER LOGIC (Fixes rivers on slopes)
			if river_noise:
				var river_val = abs(river_noise.get_noise_2d(global_x, global_y))
				#var river_width = 0.05 
				var water_width = 0.04 
				
				if river_val < water_width:
					# --- FIX: ONLY PAINT WATER IF FLAT ---
					if up_dot > 0.85: # Almost completely flat
						base_col = water_color
						grass_allowed = false # No grass underwater
					else:
						# It's a steep river path (waterfall/rapids). 
						# Keep it cliff color, don't paint blue water.
						grass_allowed = false # No grass on waterfalls either
						
				#elif river_val < river_width:
					# Bank
					#base_col = shore_color
					

			color_img.set_pixel(x, y, base_col)
			
			# Set grass density based on previous checks
			var density = 1.0 if grass_allowed else 0.0
			control_img.set_pixel(x, y, Color(density, 0, 0)) # Only using Red channel (easily readable by grass shader)
			
	return { "colormap": color_img, "controlmap": control_img }

# --- MAIN GENERATION FUNCTION ---
func generate_chunk(chunk_coord: Vector2i) -> Dictionary:
	var offset = Vector2(chunk_coord.x * (chunk_vertex_count-1), chunk_coord.y * (chunk_vertex_count-1))
	
	# 1. CPU Noise (Ideally move to GPU later, but this is fast enough for now)
	var img = _generate_height_map_cpu(offset)
	
	# 2. RUN GPU PIPELINE (Erosion + Mesh Gen)
	var gpu_results = _run_gpu_pipeline(img)
	
	# 3. Coloring (CPU for now, needs the height image)
	# We use the eroded height image returned from GPU
	var eroded_img = gpu_results.height_image
	var flux_img = gpu_results.flux_image
	var maps = _generate_color_map(eroded_img, flux_img, offset)
	var color_img = maps["colormap"]
	var control_img = maps["controlmap"]
	
	# 4. Construct ArrayMesh 
	var mesh = ArrayMesh.new()
	var arrays = []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = gpu_results.vertices
	arrays[Mesh.ARRAY_NORMAL] = gpu_results.normals
	arrays[Mesh.ARRAY_TEX_UV] = _cached_uvs
	arrays[Mesh.ARRAY_INDEX] = _cached_indices
	
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	
	return {
		"mesh": mesh,
		"colormap": color_img,
		"heightmap": eroded_img,
		"controlmap": control_img 
	}

func _run_gpu_pipeline(input_img: Image) -> Dictionary:
	# --- STEP A: PREPARE DATA ---
	var input_floats = PackedFloat32Array()
	# optimized extraction
	input_floats = input_img.get_data().to_float32_array() 
	
	var input_bytes = input_floats.to_byte_array()
	var height_buffer_rid = _rd.storage_buffer_create(input_bytes.size(), input_bytes)
	
	# Create Output Buffers for Mesh Gen
	var vertex_count = chunk_vertex_count * chunk_vertex_count
	# Vec3 = 3 floats * 4 bytes = 12 bytes per vertex
	var mesh_buffer_size = vertex_count * 3 * 4 
	var vertex_buffer_rid = _rd.storage_buffer_create(mesh_buffer_size)
	var normal_buffer_rid = _rd.storage_buffer_create(mesh_buffer_size)
	
	# --- NEW: CREATE FLUX BUFFER ---
	# Initialize with zeros (same size as height map)
	var flux_bytes = PackedFloat32Array()
	flux_bytes.resize(input_floats.size())
	flux_bytes.fill(0.0)
	var flux_bytes_raw = flux_bytes.to_byte_array()
	var flux_buffer_rid = _rd.storage_buffer_create(flux_bytes_raw.size(), flux_bytes_raw)
	
	# --- STEP B: EROSION DISPATCH ---
	# (Setup Uniforms for Erosion)
	var erosion_uniform_h = RDUniform.new()
	erosion_uniform_h.binding = 0
	erosion_uniform_h.uniform_type = RenderingDevice.UNIFORM_TYPE_STORAGE_BUFFER
	erosion_uniform_h.add_id(height_buffer_rid)
	
	var erosion_uniform_f = RDUniform.new()
	erosion_uniform_f.binding = 1 # Matches 'binding = 1' in GLSL
	erosion_uniform_f.uniform_type = RenderingDevice.UNIFORM_TYPE_STORAGE_BUFFER
	erosion_uniform_f.add_id(flux_buffer_rid)
	
	# Create the set with BOTH uniforms
	var erosion_set = _rd.uniform_set_create([erosion_uniform_h, erosion_uniform_f], _erosion_shader_rid, 0)
	
	# Push Constants (Using the padding fix to get power of 2)
	var e_consts = PackedFloat32Array([
		float(chunk_vertex_count), 
		float(20000),   # REDUCED: 50k -> 20k droplets
		3.0,            # brush_radius
		0.5,            # inertia
		4.0,            # sediment_capacity
		0.01,           # min_slope
		0.05,           # REDUCED: Erosion Speed 0.3 -> 0.05 (More gentle)
		0.1,            # REDUCED: Deposition Speed 0.3 -> 0.1
		4.0,            # gravity
		0.1,            # evaporation
		0.0, 0.0        # Padding
	])
	
	# --- STEP C: MESH GEN DISPATCH ---
	# (Setup Uniforms for Mesh Gen)
	# Binding 0: Height (Read), Binding 1: Verts (Write), Binding 2: Norms (Write)
	var mesh_uniform_h = RDUniform.new()
	mesh_uniform_h.binding = 0
	mesh_uniform_h.uniform_type = RenderingDevice.UNIFORM_TYPE_STORAGE_BUFFER
	mesh_uniform_h.add_id(height_buffer_rid)
	
	var mesh_uniform_v = RDUniform.new()
	mesh_uniform_v.binding = 1
	mesh_uniform_v.uniform_type = RenderingDevice.UNIFORM_TYPE_STORAGE_BUFFER
	mesh_uniform_v.add_id(vertex_buffer_rid)
	
	var mesh_uniform_n = RDUniform.new()
	mesh_uniform_n.binding = 2
	mesh_uniform_n.uniform_type = RenderingDevice.UNIFORM_TYPE_STORAGE_BUFFER
	mesh_uniform_n.add_id(normal_buffer_rid)
	
	var mesh_set = _rd.uniform_set_create([mesh_uniform_h, mesh_uniform_v, mesh_uniform_n], _mesh_shader_rid, 0)
	
	var m_consts = PackedFloat32Array([float(chunk_vertex_count), terrain_scale, 0.0, 0.0])
	
	# --- STEP D: EXECUTE COMMANDS ---
	var compute_list = _rd.compute_list_begin()
	
	# 1. Run Erosion
	_rd.compute_list_bind_compute_pipeline(compute_list, _erosion_pipeline)
	_rd.compute_list_bind_uniform_set(compute_list, erosion_set, 0)
	_rd.compute_list_set_push_constant(compute_list, e_consts.to_byte_array(), e_consts.to_byte_array().size())
	var e_groups = int(ceil(erosion_iterations / 64.0))
	_rd.compute_list_dispatch(compute_list, e_groups, 1, 1)
	
	# BARRIER: Wait for Erosion to finish writing to height_buffer before reading it
	_rd.compute_list_add_barrier(compute_list)
	
	# 2. Run Mesh Gen
	_rd.compute_list_bind_compute_pipeline(compute_list, _mesh_pipeline)
	_rd.compute_list_bind_uniform_set(compute_list, mesh_set, 0)
	_rd.compute_list_set_push_constant(compute_list, m_consts.to_byte_array(), m_consts.to_byte_array().size())
	var m_groups = int(ceil(chunk_vertex_count / 8.0))
	_rd.compute_list_dispatch(compute_list, m_groups, m_groups, 1)
	
	_rd.compute_list_end()
	_rd.submit()
	_rd.sync()
	
	# --- STEP E: RETRIEVE DATA ---
	# Get Vertices
	var vert_bytes = _rd.buffer_get_data(vertex_buffer_rid)
	var norm_bytes = _rd.buffer_get_data(normal_buffer_rid)
	var height_bytes = _rd.buffer_get_data(height_buffer_rid)
	
	# Get Flux Data
	var flux_output_bytes = _rd.buffer_get_data(flux_buffer_rid)
	#var flux_floats = flux_output_bytes.to_float32_array()
	
	# Convert to standard PackedArrays for ArrayMesh
	var verts = vert_bytes.to_vector3_array()
	var norms = norm_bytes.to_vector3_array()
	
	# Convert Height buffer back to Image (for Coloring Logic)
	var h_img = Image.create_from_data(chunk_vertex_count, chunk_vertex_count, false, Image.FORMAT_RF, height_bytes)
	
	# Convert Flux to Image (Store in Red channel)
	var flux_img = Image.create_from_data(chunk_vertex_count, chunk_vertex_count, false, Image.FORMAT_RF, flux_output_bytes)
	
	# Cleanup RIDs
	_rd.free_rid(flux_buffer_rid)
	_rd.free_rid(height_buffer_rid)
	_rd.free_rid(vertex_buffer_rid)
	_rd.free_rid(normal_buffer_rid)
	
	return { "vertices": verts, "normals": norms, "height_image": h_img, "flux_image": flux_img }
