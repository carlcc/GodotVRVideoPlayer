extends Node3D


@onready var _controlPanel : PlayingControlPanel = $PlayingControlPanel

# Called when the node enters the scene tree for the first time.
func _ready():
	_controlPanel.on_play.connect(_on_play)
	_controlPanel.on_pixel_format_change.connect(_on_pixel_format_changed)
	_controlPanel.set_material_mode(PlayingControlPanel.MaterialMode.k3d)
	pass # Replace with function body.


func _on_pixel_format_changed(material: Material, texture: Texture):
	var meshInstance : MeshInstance3D = $MeshInstance3d
	meshInstance.mesh.surface_set_material(0, material)

	pass
	
func _on_play():
	pass

