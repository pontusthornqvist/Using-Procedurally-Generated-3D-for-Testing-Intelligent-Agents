#[compute]
#version 450

// 64 threads per group
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0, std430) restrict buffer Heightmap {
    float data[];
} heightmap;

layout(set = 0, binding = 1, std430) restrict buffer FluxMap {
    float data[]; // Stores total water volume passing through this node
} fluxmap;

layout(push_constant) uniform Params {
    float map_size_f;      
    float num_droplets_f;  
    float brush_radius;
    float inertia;
    float sediment_capacity_factor;
    float min_slope;
    float erosion_speed;
    float deposition_speed;
    float gravity;
    float evaporation;
} params;

// --- FIX: SAFE SAMPLING ---
// Instead of returning 0.0 for out-of-bounds, we clamp the coordinate.
// This acts like "Texture Repeat: Clamp to Edge".
// If we ask for pixel -1, we get pixel 0. The slope becomes 0 (flat), not a cliff.
float get_height(vec2 pos, int map_size) {
    // Use floor() to handle negative numbers correctly (int(-0.5) is 0, floor is -1)
    int x = int(floor(pos.x));
    int y = int(floor(pos.y));
    
    // Clamp coordinates to valid range [0, map_size - 1]
    x = clamp(x, 0, map_size - 1);
    y = clamp(y, 0, map_size - 1);
    
    return heightmap.data[y * map_size + x];
}

// Simple pseudo-randomness
float rand(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main() {
    int map_size = int(params.map_size_f);
    int num_droplets = int(params.num_droplets_f);

    uint id = gl_GlobalInvocationID.x;
    if (id >= uint(num_droplets)) return;

    // Start Position
    float start_x = rand(vec2(float(id), 1.0)) * float(map_size - 1);
    float start_y = rand(vec2(float(id), 2.0)) * float(map_size - 1);
    vec2 pos = vec2(start_x, start_y);
    
    vec2 dir = vec2(0.0);
    float speed = 1.0;
    float water = 1.0;
    float sediment = 0.0;

    for (int lifetime = 0; lifetime < 30; lifetime++) {
        
        int node_x = int(pos.x);
        int node_y = int(pos.y);
        int node_idx = node_y * map_size + node_x;

        // --- CAPTURE FLOW ---
        // Atomic add is safer for parallelism, but for erosion visuals, 
        // a simple add is "good enough" and faster.
        fluxmap.data[node_idx] += 1.0;

        // Calculate Gradient using the Safe Get Function
        // Using offsets of roughly 1 pixel distance
        float h   = get_height(pos, map_size);
        float h_l = get_height(pos + vec2(-1.0, 0.0), map_size);
        float h_r = get_height(pos + vec2(1.0, 0.0), map_size);
        float h_u = get_height(pos + vec2(0.0, -1.0), map_size);
        float h_d = get_height(pos + vec2(0.0, 1.0), map_size);
        
        // Gradient Vector
        vec2 gradient = vec2(h_r - h_l, h_d - h_u);

        // Update Direction
        dir = (dir * params.inertia) - (gradient * 0.5);
        
        // Normalize Direction
        float len = length(dir);
        if (len != 0.0) {
            dir /= len;
        }
        
        pos += dir;
        
        // Boundary Check (Stop if we actually leave the map area)
        if (pos.x < 0.0 || pos.x >= float(map_size - 1) || 
            pos.y < 0.0 || pos.y >= float(map_size - 1)) {
            break;
        }

        float new_h = get_height(pos, map_size);
        float diff = new_h - h;

        // Sediment Capacity
        float capacity = max(-diff, params.min_slope) * speed * water * params.sediment_capacity_factor;
        
        // --- FIX: SEAM PROTECTION (Edge Masking) ---
        // We calculate how close the droplet is to the edge of the chunk.
        // If it is within 4 pixels of the edge, we force the erosion/deposition to 0.
        // This keeps the edges "pristine" so they match the neighbor chunk perfectly.
        // STILL GETTING CHUNK SEAMS, Needs additional/other FIX
        
        float edge_dist_x = min(pos.x, float(map_size) - 1.0 - pos.x);
        float edge_dist_y = min(pos.y, float(map_size) - 1.0 - pos.y);
        float edge_dist = min(edge_dist_x, edge_dist_y);
        
        // 0.0 at edge, 1.0 at 4 pixels in. Smooth transition.
        float edge_weight = smoothstep(0.0, 4.0, edge_dist);
        
        // Apply weight to the physics limits
        // (We modify the logic inside the IF/ELSE below effectively)

        if (sediment > capacity || diff > 0.0) {
            // DEPOSIT
            float amount_to_deposit = (diff > 0.0) ? min(diff, sediment) : (sediment - capacity) * params.deposition_speed;
            amount_to_deposit = min(amount_to_deposit, 1.0); 
            
            // APPLY MASK:
            amount_to_deposit *= edge_weight; 
            
            sediment -= amount_to_deposit;
            heightmap.data[node_idx] += amount_to_deposit; 
        } else {
            // ERODE
            float amount_to_erode = min((capacity - sediment) * params.erosion_speed, -diff);
            amount_to_erode = min(amount_to_erode, 1.0);
            
            // APPLY MASK
            amount_to_erode *= edge_weight;
            
            sediment += amount_to_erode;
            heightmap.data[node_idx] -= amount_to_erode;
        }

        // Physics Updates
        speed = sqrt(max(0.0, speed * speed + abs(diff) * params.gravity)); // max(0) prevents NaN
        water *= (1.0 - params.evaporation);
        
        if (water < 0.01) break;
    }
}