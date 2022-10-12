extends Node

@export var cameraL : Camera3D = null
@export var cameraR : Camera3D = null

# Called when the node enters the scene tree for the first time.
func _ready():
	var cameraFovValueLabel : Label = find_child("CameraFovName")
	var cameraFovSlider : HSlider = find_child("CameraFovSlider")
	
	cameraFovSlider.min_value = 20
	cameraFovSlider.max_value = 170
	cameraFovSlider.value = cameraL.fov
	cameraFovSlider.value_changed.connect(func(v: float):
		cameraL.fov = v;
		cameraR.fov = v;
		pass
	)
	pass # Replace with function body.


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta):
	pass
