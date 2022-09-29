extends Node

	
var textureRect :TextureRect
var mediaStream : FfmpegMediaStream

# Called when the node enters the scene tree for the first time.
func _ready():
	textureRect = find_child("VideoTextureRect", true)
	pass # Replace with function body.


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta):
	if mediaStream != null:
		mediaStream.update(delta)
	pass


func _on_set_file_pressed():
	var ms = FfmpegMediaStream.new()
	if not ms.init("res://Data/shjx.mp4"):
		return
	mediaStream = ms
	ms.play()
	textureRect.texture = ms.get_texture()
	pass # Replace with function body.
