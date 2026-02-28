#include "mesh_compute_scheduler.h"
#include "chunk_mesh_data.h"
#include "voxel_terrain.h"
// #include "adaptive_surface_nets/adaptive_surface_nets.h"
#include "stitched_surface_nets/stitched_surface_nets.h"
#include "voxel_octree_node.h"

MeshComputeScheduler::MeshComputeScheduler(int maxConcurrentTasks)
    : _maxConcurrentTasks(maxConcurrentTasks), _activeTasks(0), _pendingUploads(0), _totalTris(0), _prevTris(0),
    threadPool(maxConcurrentTasks)
{
}

void MeshComputeScheduler::enqueue(VoxelOctreeNode &node)
{
    ChunksToAdd.push(&node);
}

void MeshComputeScheduler::process(JarVoxelTerrain& terrain)
{
    _prevTris = _totalTris;

    // Only dispatch new tasks if we aren't actively rebuilding the tree
    if (!terrain.is_building())
    {
        process_queue(terrain);
    }

    // --- ADAPTIVE TIME-SLICING ---
    int pending = _pendingUploads.load();

    // Process 15% of the waiting queue per frame, but never less than 2.
    // Startup (500 chunks) -> processes ~75 chunks the first frame (fast load!).
    // Runtime (5 chunks) -> processes 2 chunks per frame (buttery smooth!).
    // Because is_meshing() now protects this queue, the octree will safely 
    // wait to prune/delete nodes until this queue is completely empty.
    int max_chunks_this_frame = std::max(2, (int)(pending * 0.15f));
    int chunks_processed = 0;

    while (!ChunksToProcess.empty() && chunks_processed < max_chunks_this_frame)
    {
        std::pair<VoxelOctreeNode*, ChunkMeshData*> tuple;
        if (ChunksToProcess.try_pop(tuple))
        {
            _pendingUploads--; // Mark as processed!
            auto [node, chunkMeshData] = tuple;
            node->update_chunk(terrain, chunkMeshData);
            chunks_processed++;
        }
    }
}

void MeshComputeScheduler::process_queue(JarVoxelTerrain &terrain)
{
    while (!ChunksToAdd.empty())
    {
        VoxelOctreeNode *chunk;
        if (ChunksToAdd.try_pop(chunk))
        {
            run_task(terrain, *chunk);
        }
        else
            return;
    }
}

void MeshComputeScheduler::run_task(const JarVoxelTerrain& terrain, VoxelOctreeNode& chunk)
{
    if (!chunk.is_chunk(terrain))
        return;

    // FIX: Actually track the active threads before sending them off!
    _activeTasks++;

    threadPool.enqueue([this, &terrain, &chunk]() {
        auto meshCompute = StitchedSurfaceNets(terrain, chunk);
        ChunkMeshData* chunkMeshData = meshCompute.generate_mesh_data(terrain);

        ChunksToProcess.push(std::make_pair(&(chunk), chunkMeshData));

        _pendingUploads++; // Tell the main thread there's a new mesh ready!
        _activeTasks--; // Thread finished
        });
}

void MeshComputeScheduler::clear_queue()
{
    //if we readd this, ensure to unenqueue all nodes!
    // ChunksToAdd.clear();
    // ChunksToProcess.clear();
}
