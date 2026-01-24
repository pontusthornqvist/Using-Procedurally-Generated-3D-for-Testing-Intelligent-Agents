shader_type spatial;
render_mode cull_disabled, specular_disabled;

// --- UNIFORMS ---
uniform sampler2D u_terrain_heightmap;
uniform sampler2D u_terrain_detailmap;
uniform sampler2D u_terrain_globalmap : source_color; // Using the baked texture
uniform mat4 u_terrain_inverse_transform;

uniform sampler2D u_albedo_alpha : source_color; // Grass blade shape
uniform float u_fray_strength : hint_range(0.0, 1.0) = 0.5;
uniform float u_thickness_boost : hint_range(0.0, 0.5) = 0.1;

// --- ADVANCED WIND SETTINGS ---
uniform float global_wind_speed = 0.15;
uniform vec2 global_wind_direction = vec2(1.0, 0.0);
uniform sampler2D global_wind_noise; // I used perlin at .007 frequency + ping-pong fractals
uniform float wind_divergence_angle = 0.35; // ~20 degrees (Radians)
uniform float wind_scale = 0.05;

// Wind Colors (Tinting)
uniform vec3 wind_shadow_color : source_color = vec3(0.6, 0.6, 0.8);
uniform vec3 wind_highlight_color : source_color = vec3(1.5, 1.5, 1.2);

// --- VARYINGS ---
varying float v_wind_intensity;
varying vec3 v_world_pos;

// HELPERS
float get_hash(vec2 c) { return fract(sin(dot(c.xy, vec2(12.9898,78.233))) * 43758.5453); }
float get_height(sampler2D heightmap, vec2 uv) { return texture(heightmap, uv).r; }

// Function to rotate a vector
vec2 rotate_vector(vec2 v, float angle) {
	float c = cos(angle);
	float s = sin(angle);
	return vec2(v.x * c - v.y * s, v.x * s + v.y * c);
}

void vertex() {
	vec4 obj_pos = MODEL_MATRIX * vec4(0, 1, 0, 1);
	vec3 cell_coords = (u_terrain_inverse_transform * obj_pos).xyz;
	cell_coords.xz += vec2(0.5);
	vec2 map_uv = cell_coords.xz / vec2(textureSize(u_terrain_heightmap, 0));

	float density = texture(u_terrain_detailmap, map_uv).r;
	float hash = get_hash(obj_pos.xz);

	if (density > hash) {
		// 1. RANDOM ROTATION
		float angle = hash * 6.28318;
		float c = cos(angle);
		float s = sin(angle);
		float rot_x = VERTEX.x * c - VERTEX.z * s;
		float rot_z = VERTEX.x * s + VERTEX.z * c;
		VERTEX.x = rot_x;
		VERTEX.z = rot_z;

		// 2. HEIGHT SNAP
		float height = get_height(u_terrain_heightmap, map_uv);
		VERTEX.y += height;

		// --- 3. DUAL-SCROLLING WIND LOGIC ---
		vec4 w_pos = MODEL_MATRIX * vec4(VERTEX, 1.0);
		v_world_pos = w_pos.xyz;

		vec2 base_dir = normalize(global_wind_direction);

		// Generate two vectors diverging from the main wind direction
		vec2 dir_1 = rotate_vector(base_dir, wind_divergence_angle);
		vec2 dir_2 = rotate_vector(base_dir, -wind_divergence_angle);

		float time = TIME * global_wind_speed;

		// Scroll Noise 1 (Normal Speed)
		vec2 uv_1 = (w_pos.xz * wind_scale) + (dir_1 * time);
		float noise_1 = textureLod(global_wind_noise, uv_1, 0.0).r;

		// Scroll Noise 2 (Slightly slower, offset position)
		vec2 uv_2 = (w_pos.xz * (wind_scale * 1.1)) + (dir_2 * (time * 0.8));
		float noise_2 = textureLod(global_wind_noise, uv_2 + vec2(13.5, -5.5), 0.0).r;

		// Multiply them to create turbulence/interference
		float combined_wind = clamp(noise_1 * noise_2 * 1.5, 0.0, 1.0);
		v_wind_intensity = combined_wind;

		// Apply Physical Sway (Move mostly in main wind direction)
		float sway = combined_wind * UV.y * 0.5;
		VERTEX.x += base_dir.x * sway;
		VERTEX.z += base_dir.y * sway;

		NORMAL = vec3(0.0, 1.0, 0.0);

	} else {
		VERTEX = vec3(0, 0, 0);
	}
}

void fragment() {
	vec4 grass_tex = texture(u_albedo_alpha, UV);

	// Shape Cutout
	float puffed_shape = grass_tex.r + u_thickness_boost;
	float noise_grain = get_hash(UV * 50.0);
	float final_shape = puffed_shape - (noise_grain * u_fray_strength * 0.5);
	float shape_mask = step(0.5, final_shape);
	if (shape_mask < 0.1) discard;

	// --- COLOR SAMPLING (READS BAKED PATCHES) ---
	vec3 cell_coords = (u_terrain_inverse_transform * vec4(v_world_pos, 1.0)).xyz;
	vec2 color_map_size = vec2(textureSize(u_terrain_globalmap, 0));
	vec2 rvt_uv = cell_coords.xz / (color_map_size - vec2(1.0));
	rvt_uv = clamp(rvt_uv, 0.01, 0.99);

	vec3 base_color = texture(u_terrain_globalmap, rvt_uv).rgb;

	// Wind Tinting (Subtle)
	vec3 shadow_layer = base_color * 0.6;
	vec3 highlight_layer = base_color * 1.4;
	vec3 wind_layer = mix(shadow_layer, highlight_layer, v_wind_intensity);

	float tip_blend = smoothstep(0.2, 1.0, UV.y);
	vec3 final_color = mix(base_color, wind_layer, tip_blend);

	ALBEDO = final_color;
	ALPHA = shape_mask;
	ROUGHNESS = 1.0;
	SPECULAR = 0.0;
}