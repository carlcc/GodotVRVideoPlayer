extends Node

@onready var _lcam : Camera3D = $LEye/LCamera
@onready var _rcam : Camera3D = $REye/RCamera

# Called when the node enters the scene tree for the first time.
func _ready():
	var levp : SubViewport = $LEye
	var revp : SubViewport = $REye

	var lett :TextureRect = $Control.find_child("LEyeTexture", true)
	var rett :TextureRect = $Control.find_child("REyeTexture", true)
	
	lett.texture = levp.get_texture()
	rett.texture = revp.get_texture()
	
	$PanoramaPlayer/Camera3d.queue_free()
	pass # Replace with function body.


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta):
	pass


func _input(event):
	_lcam._input(event)
	_rcam._input(event)
