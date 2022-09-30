extends Node

	
var textureRect :TextureRect
@onready var _controlPanel : PlayingControlPanel = $PlayingControlPanel


# Called when the node enters the scene tree for the first time.
func _ready():
	textureRect = find_child("VideoTextureRect", true)
	_controlPanel.on_play.connect(_on_play)
	_controlPanel.on_progress_drag_begin.connect(_on_progress_drag_begin)
	_controlPanel.on_progress_drag_end.connect(_on_progress_drag_end)
	_controlPanel.on_pixel_format_change.connect(_on_pixel_format_changed)
	_controlPanel.set_file("res://Data/shjx.mp4")
	_controlPanel.set_3d_material(false)
	pass # Replace with function body.


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta):
	pass
	
func _on_pixel_format_changed(material: Material, texture: Texture):
	textureRect.texture = texture
	textureRect.material = material
	pass
	
func _on_play():
	pass


func _on_progress_drag_begin():
	pass # Replace with function body.


func _on_progress_drag_end(value_changed):
	pass # Replace with function body.
