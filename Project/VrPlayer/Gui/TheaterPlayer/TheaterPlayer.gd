extends Node3D

var material : StandardMaterial3D
var mediaStream : FfmpegMediaStream

# Called when the node enters the scene tree for the first time.
func _ready():
	var mesh = $MeshInstance3d as MeshInstance3D
	material = mesh.get_active_material(0) as StandardMaterial3D
	pass # Replace with function body.


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta):
	if mediaStream != null:
		mediaStream.update(delta)
	pass


func _on_play_pressed():
	var ms = FfmpegMediaStream.new()
	if not ms.init("res://Data/shjx.mp4"):
		return
	mediaStream = ms
	ms.play()
	material.albedo_texture = ms.get_texture()
