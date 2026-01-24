extends Camera3D

@export var move_speed: float = 100.0
@export var look_sensitivity: float = 0.002
@export var fast_speed_multiplier: float = 3.0

var rotation_target: Vector3 = Vector3.ZERO

func _ready():
	# Capture the mouse cursor
	Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)
	rotation_target = rotation

func _input(event):
	# Handle mouse movement
	if event is InputEventMouseMotion and Input.get_mouse_mode() == Input.MOUSE_MODE_CAPTURED:
		rotation_target.y -= event.relative.x * look_sensitivity
		rotation_target.x -= event.relative.y * look_sensitivity
		rotation_target.x = clamp(rotation_target.x, deg_to_rad(-89), deg_to_rad(89))
		
		rotation = rotation_target

	# Release mouse with ESC key
	if event.is_action_pressed("ui_cancel"):
		if Input.get_mouse_mode() == Input.MOUSE_MODE_CAPTURED:
			Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)
		else:
			Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)

func _process(delta):
	var input_dir = Vector3.ZERO
	
	# Get movement input (THIS NEEDS TO BE SET UP IN PROJECT SETTINGS)
	if Input.is_action_pressed("move_forward"):  input_dir += -global_transform.basis.z
	if Input.is_action_pressed("move_backward"): input_dir += global_transform.basis.z
	if Input.is_action_pressed("move_left"):     input_dir += -global_transform.basis.x
	if Input.is_action_pressed("move_right"):    input_dir += global_transform.basis.x
	if Input.is_action_pressed("move_up"):       input_dir += Vector3.UP
	if Input.is_action_pressed("move_down"):     input_dir += Vector3.DOWN
	
	var current_speed = move_speed
	if Input.is_key_pressed(KEY_CTRL): # Boost speed
		current_speed *= fast_speed_multiplier
		
	global_position += input_dir.normalized() * current_speed * delta
