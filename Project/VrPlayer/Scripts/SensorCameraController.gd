extends Node

@export var camera : Camera3D = null

func _get_gravity():
	var grav = Input.get_gravity()
	
	# Check if we have all needed data
	if grav.length() < 0.1:
		var acc : Vector3 = Input.get_accelerometer()
		if acc.length() < 0.1:
			# we don't have either...
			grav = Vector3(0.0, -1.0, 0.0)
		else:
			# The gravity vector is calculated by the OS by combining the other sensor inputs.
			# If we don't have a gravity vector, from now on, use accelerometer...
			grav = acc
	return grav
	
func _get_magneto():
	var mag = Input.get_magnetometer()
	if mag.length() < 0.1:
		mag = Vector3(1.0, 0.0, 0.0)
	return mag

# This function combines the magnetometer reading with the gravity vector to get a vector that points due north
func _calc_south(p_grav, p_mag):
	# Always use normalized vectors!
	p_grav = p_grav.normalized()

	# Calculate east (or is it west) by getting our cross product.
	# The cross product of two normalized vectors returns a vector that
	# is perpendicular to our two vectors
	var east = p_grav.cross(p_mag.normalized()).normalized()

	# Cross again to get our horizon aligned north
	return east.cross(p_grav).normalized()

func _get_south():
	return _calc_south(_get_gravity(), _get_magneto())

# This function takes our gyro input and update an orientation matrix accordingly
# The gyro is special as this vector does not contain a direction but rather a
# rotational velocity. This is why we multiply our values with delta.
func _rotate_by_gyro(p_gyro, p_basis, p_delta):
	var rotate = Basis()

	rotate = rotate.rotated(p_basis.x, p_gyro.x * p_delta)
	rotate = rotate.rotated(p_basis.y, p_gyro.y * p_delta)
	rotate = rotate.rotated(p_basis.z, p_gyro.z * p_delta)

	return rotate * p_basis

# Called when the node enters the scene tree for the first time.
func _ready():
	var thisNode = self.get_node(".");
	if camera == null and thisNode is Camera3D:
		camera = thisNode as Camera3D
	if camera == null and thisNode.get_parent() is Camera3D:
		camera = thisNode.get_parent() as Camera3D
	pass # Replace with function body.

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta):
	var gyro = Input.get_gyroscope()
	camera.transform.basis = _rotate_by_gyro(gyro, camera.transform.basis, delta)
