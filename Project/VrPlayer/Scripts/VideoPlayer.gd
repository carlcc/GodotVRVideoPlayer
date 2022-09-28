extends Node

	
var textureRect :TextureRect
var playback : VideoStreamPlaybackFfmpeg

# Called when the node enters the scene tree for the first time.
func _ready():
	textureRect = find_child("VideoTextureRect", true)
	pass # Replace with function body.


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta):
	if playback != null:
		playback.update(delta)
		# textureRect.texture = playback.get_texture()
	pass


func _on_set_file_pressed():
	var stream = VideoStreamFfmpeg.new()
	stream.file = "res://Data/windows.mp4"
	playback = stream.instantiate_playback() as VideoStreamPlaybackFfmpeg
	playback.play()
	
	textureRect.texture = playback.get_texture()
	
	pass # Replace with function body.
