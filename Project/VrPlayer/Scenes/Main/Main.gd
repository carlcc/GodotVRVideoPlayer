extends Node


# Called when the node enters the scene tree for the first time.
func _ready():
	var btn1 = find_child("Button") as Button
	var btn2 = find_child("Button2") as Button
	var btn3 = find_child("Button3") as Button
	var btn4 = find_child("Button4") as Button
	
	btn1.pressed.connect(func(): get_tree().change_scene_to_file("res://Scenes/Flat2DPlayer/Flat2DPlayer.tscn"))
	btn2.pressed.connect(func(): get_tree().change_scene_to_file("res://Scenes/TheaterPlayer/TheaterPlayer.tscn"))
	btn3.pressed.connect(func(): get_tree().change_scene_to_file("res://Scenes/PanoramaPlayer/PanoramaPlayer.tscn"))
	btn4.pressed.connect(func(): get_tree().change_scene_to_file("res://Scenes/VrPlayer/VrPlayer.tscn"))
	pass # Replace with function body.


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta):
	pass
