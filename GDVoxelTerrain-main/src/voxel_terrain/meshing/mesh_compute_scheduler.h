#ifndef MESH_COMPUTE_SCHEDULER_H
#define MESH_COMPUTE_SCHEDULER_H

#include "voxel_octree_node.h"
#include <atomic>
#include <concurrent_queue.h>
#include <concurrent_priority_queue.h>
#include <functional>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <thread>
#include "utility/thread_pool.h"

using namespace godot;

class JarVoxelTerrain;


struct ChunkComparator {
    bool operator()(const VoxelOctreeNode *a, const VoxelOctreeNode *b) const {
        return a->get_lod() > b->get_lod();
    }
};

class MeshComputeScheduler
{
  private:
    concurrency::concurrent_priority_queue<VoxelOctreeNode*, ChunkComparator> ChunksToAdd;
    concurrency::concurrent_queue<std::pair<VoxelOctreeNode*, ChunkMeshData*>> ChunksToProcess;

    std::atomic<int> _activeTasks;
    std::atomic<int> _pendingUploads;
    int _maxConcurrentTasks;

    ThreadPool threadPool;

    // Debug variables
    int _totalTris;
    int _prevTris;

    void process_queue(JarVoxelTerrain &terrain);
    void run_task(const JarVoxelTerrain &terrain, VoxelOctreeNode &chunk);

  public:
    MeshComputeScheduler(int maxConcurrentTasks);
    void enqueue(VoxelOctreeNode &node);
    void process(JarVoxelTerrain &terrain);
    void clear_queue();

    bool is_meshing()
    {
        // The pipeline is only truly idle if:
        // 1. No chunks are waiting to be processed
        // 2. No background threads are actively computing meshes
        // 3. No finished meshes are waiting in the main-thread queue to be uploaded
        return !ChunksToAdd.empty() || _activeTasks.load() > 0 || !ChunksToProcess.empty();
    }
};

#endif // MESH_COMPUTE_SCHEDULER_H
