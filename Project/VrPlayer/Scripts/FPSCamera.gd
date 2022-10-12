extends Node

@export var canMove : bool = true
@export var camera : Camera3D = null

const MOUSE_SENSITIVITY = 0.002
const MOVE_SPEED = 1.5

var rot = Vector3()
var velocity = Vector3()

var isMouseDown : bool = false


func _ready():
	var thisNode = self.get_node(".");
	if camera == null and thisNode is Camera3D:
		camera = thisNode as Camera3D
	if camera == null and thisNode.get_parent() is Camera3D:
		camera = thisNode.get_parent() as Camera3D
	
	pass


func _input(event):
	# Mouse look (only if the mouse is captured).
	if event is InputEventMouseMotion and Input.get_mouse_mode() == Input.MOUSE_MODE_CAPTURED:
		# Horizontal mouse look.
		rot.y -= event.relative.x * MOUSE_SENSITIVITY
		# Vertical mouse look.
		rot.x = clamp(rot.x - event.relative.y * MOUSE_SENSITIVITY, -1.57, 1.57)
		camera.transform.basis = Basis.from_euler(rot)
		
	if event is InputEventScreenDrag:
		var de = event as InputEventScreenDrag
		rot.y -= event.relative.x * MOUSE_SENSITIVITY
		# Vertical mouse look.
		rot.x = clamp(rot.x - event.relative.y * MOUSE_SENSITIVITY, -1.57, 1.57)
		camera.transform.basis = Basis.from_euler(rot)
	
	if event.is_action_pressed("RightMouseDown"):
		Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)
	elif event.is_action_released("RightMouseDown"):
		Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)

func _process(delta):
	if canMove:
		var motion = Vector3(
				Input.get_action_strength("MoveRight") - Input.get_action_strength("MoveLeft"),
				0,
				Input.get_action_strength("MoveBack") - Input.get_action_strength("MoveForward")
		)

		motion.y += Input.get_action_strength("MoveUp") - Input.get_action_strength("MoveDown")
		# Normalize motion to prevent diagonal movement from being
		# `sqrt(2)` times faster than straight movement.
		motion = motion.normalized()

		velocity += MOVE_SPEED * delta * (camera.transform.basis * motion)
		velocity *= 0.85
		camera.position += velocity
