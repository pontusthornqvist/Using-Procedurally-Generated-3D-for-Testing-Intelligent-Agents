@tool
class_name RiverManager
extends Node3D

@export var terrain: VoxelTerrain
@export var river_curve: Curve3D
@export var river_width: float = 15.0
@export var river_depth: float = 20.0
@export var debug_draw: bool = true :
	set(value):
		debug_draw = value
		_update_debug_mesh()

# Internal node for drawing
var _debug_mesh_instance: MeshInstance3D

func _ready():
	if not river_curve:
		return
		
	# Setup Debug Drawer
	_setup_debug_drawer()
	
	# Connect curve changes to redraw
	if river_curve:
		river_curve.changed.connect(_on_curve_changed)

	# If we are in-game (not editor), apply the river to the terrain
	if not Engine.is_editor_hint():
		# Wait one frame to ensure terrain is initialized
		await get_tree().process_frame
		apply_river_to_terrain()

func _setup_debug_drawer():
	# Clean up old instance if it exists (reloads)
	if _debug_mesh_instance:
		_debug_mesh_instance.queue_free()
		
	_debug_mesh_instance = MeshInstance3D.new()
	_debug_mesh_instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_debug_mesh_instance.mesh = ImmediateMesh.new()
	
	# Create a simple bright blue material that ignores lighting
	var mat = StandardMaterial3D.new()
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mat.albedo_color = Color(0, 0.6, 1.0)
	_debug_mesh_instance.material_override = mat
	
	add_child(_debug_mesh_instance)

func _on_curve_changed():
	_update_debug_mesh()

func _process(_delta):
	# Redraw if we are in editor and something moved
	if Engine.is_editor_hint() and debug_draw:
		# Optimization: Only redraw if transform changed or flagged dirty? 
		# For now, simplistic update is fine for a tool script.
		_update_debug_mesh()

func _update_debug_mesh():
	if not _debug_mesh_instance or not _debug_mesh_instance.mesh:
		_setup_debug_drawer()
		
	var mesh = _debug_mesh_instance.mesh as ImmediateMesh
	mesh.clear_surfaces()
	
	if not river_curve or not debug_draw:
		return
		
	mesh.surface_begin(Mesh.PRIMITIVE_LINES)
	
	# Tessellate gives us points in LOCAL space of the curve resource
	var points = river_curve.tessellate(5, 1)
	
	for i in range(points.size() - 1):
		var p1 = points[i]
		var p2 = points[i+1]
		
		# Draw Line
		mesh.surface_add_vertex(p1)
		mesh.surface_add_vertex(p2)
		
		# Draw simple cross-hatch for width visualization (Optional)
		# (Just main line for now to keep it clean)
		
	mesh.surface_end()

func apply_river_to_terrain():
	if not terrain:
		printerr("RiverManager: No terrain assigned!")
		return
		
	print("RiverManager: Baking curve into terrain SDF...")
	
	var points = river_curve.tessellate(5, 1)
	
	for i in range(points.size() - 1):
		var p1 = points[i]
		var p2 = points[i+1]
		
		# IMPORTANT: RiverManager local -> Global -> Terrain Local
		# If Terrain is at (0,0,0), Global is fine.
		var start_pos = to_global(p1)
		var end_pos = to_global(p2)
		
		terrain.add_path(start_pos, end_pos, river_width)
