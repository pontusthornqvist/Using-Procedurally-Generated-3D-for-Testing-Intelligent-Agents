# Godot Voxel Terrain Plugin

![Banner Image](https://github.com/JorisAR/GDVoxelTerrain/blob/main/banner.png?raw=true)

This project adds a smooth voxel terrain system to godot. 
More precisely, it uses an octree to store an SDF, which is then meshed using a custom version of surface nets. Level of detail systems are in place for large viewing distances.


## Getting Started

### Installation

- Note, it has only been tested for Windows 11.
- Build the source using scons to your target platforms.
- move the contents of the addons folder to the addons folder in your project.

## Usage

- Please refer to the demo scene to see how to use the terrain system.

## Contributing

Contributions are welcome! Please fork the repository and submit a pull request.

In particular, here are some areas of interest, in no particular order of importance:
- Use TBB concurrent(priority)queue for the mesh schedular to ensure greater compatibility.
- Fix build/release pipeline on github.
- Optimizing LOD connections, i.e. remove the overdraw/duplicate triangle generation.
- Multi-material support.
- Multithreaded octree generation.
- Optimize generating colliders, it causes quite a performance impact.
- Optimize chunks, we likely want to use some sort of object pool as we're dealing with thousands of nodes.
- Improve thread management.
- Add more interesting SDF options:
    - Combine multiple existing SDFs based on boolean operation (e.g. OperationSDF, that takes 2 SDF references and an enum to select the operation).
    - Better noise based SDFs for more realistic terrain.
    - SDF based on an arbitrary triangle mesh.
- Documentation & icons.
- Navigation system.
- Dual contouring extension.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
