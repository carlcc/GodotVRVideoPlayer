extends Node


# Called when the node enters the scene tree for the first time.
func _ready():
	pass # Replace with function body.


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta):
	pass


func _on_set_file_pressed():
	var player = $VideoStreamPlayer
	var stream = VideoStreamFfmpeg.new()
	stream.file = "res://Data/windows.mp4"
	player.stream = stream
	player.play()
	pass # Replace with function body.
