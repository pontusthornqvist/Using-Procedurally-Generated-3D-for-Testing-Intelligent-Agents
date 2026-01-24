#[compute]
#version 450

// 8x8 threads per group (standard for 2D)
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// INPUT: The eroded height data from the previous step
layout(set = 0, binding = 0, std430) restrict readonly buffer HeightBuffer {
    float heights[];
} height_buffer;

// OUTPUT 1: The calculated vertex positions (Vector3)
layout(set = 0, binding = 1, std430) restrict writeonly buffer VertexBuffer {
    float vertices[]; // stored as x, y, z, x, y, z...
} vertex_buffer;

// OUTPUT 2: The calculated normals (Vector3)
layout(set = 0, binding = 2, std430) restrict writeonly buffer NormalBuffer {
    float normals[]; 
} normal_buffer;

layout(push_constant) uniform Params {
    float map_size;    // e.g., 65 (64 quads = 65 vertices)
    float terrain_scale;
    vec2 _padding; // Dummy vec2 to align to 16 bytes
} params;

float get_height(int x, int y, int size) {
    x = clamp(x, 0, size - 1);
    y = clamp(y, 0, size - 1);
    return height_buffer.heights[y * size + x];
}

void main() {
    int size = int(params.map_size);
    uvec2 id = gl_GlobalInvocationID.xy;
    
    // Don't run if we are outside the requested grid
    if (id.x >= uint(size) || id.y >= uint(size)) return;
    
    int x = int(id.x);
    int y = int(id.y);
    int index = y * size + x;
    
    // 1. Calculate Position
    float h = height_buffer.heights[index];
    
    // Write Vertex (Stride of 3 floats)
    int v_idx = index * 3;
    vertex_buffer.vertices[v_idx + 0] = float(x) * params.terrain_scale;
    vertex_buffer.vertices[v_idx + 1] = h;
    vertex_buffer.vertices[v_idx + 2] = float(y) * params.terrain_scale;
    
    // 2. Calculate Normal (Sobel-ish filter for better quality than simple cross product)
    float h_l = get_height(x - 1, y, size);
    float h_r = get_height(x + 1, y, size);
    float h_u = get_height(x, y - 1, size);
    float h_d = get_height(x, y + 1, size);
    
    // Slope vectors
    vec3 va = vec3(2.0 * params.terrain_scale, h_r - h_l, 0.0);
    vec3 vb = vec3(0.0, h_d - h_u, 2.0 * params.terrain_scale);
    
    vec3 normal = normalize(cross(vb, va));
    
    // Write Normal
    normal_buffer.normals[v_idx + 0] = normal.x;
    normal_buffer.normals[v_idx + 1] = normal.y;
    normal_buffer.normals[v_idx + 2] = normal.z;
}