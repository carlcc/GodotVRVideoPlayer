extends Node

	
var textureRect :TextureRect
var mediaStream : FfmpegMediaStream
var playProgressBar : HSlider

var isProgressBarDragging : bool = false

# Called when the node enters the scene tree for the first time.
func _ready():
	textureRect = find_child("VideoTextureRect", true)
	playProgressBar = find_child("PlayProgressBar", true)
	pass # Replace with function body.


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta):
	if mediaStream != null:
		mediaStream.update(delta)
		if not isProgressBarDragging:
			playProgressBar.value = mediaStream.get_position()
	pass

func _on_play_button_pressed():
	var ms = FfmpegMediaStream.new()
	if not ms.init("res://Data/shjx.mp4"):
		return
	mediaStream = ms
	ms.play()
	textureRect.texture = ms.get_texture()
	playProgressBar.max_value = ms.get_length()
	playProgressBar.value = 0
	


func _on_play_progress_bar_drag_started():
	isProgressBarDragging = true
	pass # Replace with function body.


func _on_play_progress_bar_drag_ended(value_changed):
	isProgressBarDragging = false
	if value_changed:
		mediaStream.seek(playProgressBar.value)
	pass # Replace with function body.
