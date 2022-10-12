extends Node3D


@onready var _gravity : MeshInstance3D = $gravity
@onready var _magneto : MeshInstance3D = $magneto
@onready var _accelerator : MeshInstance3D = $accelerator
@onready var _gyroscope : MeshInstance3D = $gyroscope
@onready var _gravityLabel : Label = $Control/grav
@onready var _magnetoLabel : Label = $Control/mag
@onready var _acceleratorLabel : Label = $Control/acc
@onready var _gyroscopeLabel : Label = $Control/gyro
@onready var _northLabel : Label = $Control/north

# Called when the node enters the scene tree for the first time.
func _ready():
	pass # Replace with function body.


# This function calculates a rotation matrix based on a direction vector. As our arrows are cylindrical we don't
# care about the rotation around this axis.
func get_basis_for_arrow(p_vector):
	var rotate = Basis()

	# as our arrow points up, Y = our direction vector
	rotate.y = p_vector.normalized()

	# get an arbitrary vector we can use to calculate our other two vectors
	var v = Vector3(1.0, 0.0, 0.0)
	if abs(v.dot(rotate.y)) > 0.9:
		v = Vector3(0.0, 1.0, 0.0)

	# use our vector to get a vector perpendicular to our two vectors
	rotate.x = rotate.y.cross(v).normalized()

	# and the cross product again gives us our final vector perpendicular to our previous two vectors
	rotate.z = rotate.x.cross(rotate.y).normalized()

	return rotate

# This function combines the magnetometer reading with the gravity vector to get a vector that points due north
func calc_north(p_grav, p_mag):
	# Always use normalized vectors!
	p_grav = p_grav.normalized()

	# Calculate east (or is it west) by getting our cross product.
	# The cross product of two normalized vectors returns a vector that
	# is perpendicular to our two vectors
	var east = p_grav.cross(p_mag.normalized()).normalized()

	# Cross again to get our horizon aligned north
	return east.cross(p_grav).normalized()

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta):
	var acc:Vector3 = Input.get_accelerometer()
	var grav = Input.get_gravity()
	var mag = Input.get_magnetometer()
	var gyro = Input.get_gyroscope()
	
	
	# Check if we have all needed data
	if grav.length() < 0.1:
		if acc.length() < 0.1:
			# we don't have either...
			grav = Vector3(0.0, -1.0, 0.0)
		else:
			# The gravity vector is calculated by the OS by combining the other sensor inputs.
			# If we don't have a gravity vector, from now on, use accelerometer...
			grav = acc

	if mag.length() < 0.1:
		mag = Vector3(1.0, 0.0, 0.0)
	
	_gravity.look_at(_gravity.position + grav)
	var north = calc_north(grav,mag)
	_magneto.look_at(_magneto.position + north)
	
	_gravityLabel.text = "grav {0}".format([grav])
	_magnetoLabel.text = "mag {0}".format([mag])
	_northLabel.text = "north {0}".format([north])
	_acceleratorLabel.text = "acc {0}".format([acc])
	_gyroscopeLabel.text = "gyro {0}".format([gyro])
