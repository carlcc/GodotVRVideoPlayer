extends Node

static func _find_scenes(dirs) -> Array[String]:
	FileAccess
	var arr : Array[String] = []
	for dir in dirs:
		var fileName : String = "res://Scenes/" + dir + "/" + dir + ".tscn"
		if FileAccess.file_exists(fileName):
			arr.append(fileName)
		else:
			fileName = "res://Scenes/" + dir + "/Main.tscn"
			if FileAccess.file_exists(fileName):
				arr.append(fileName)
	return arr
	

# Called when the node enters the scene tree for the first time.
func _ready():
	# auto detect available scenes and make the main menu
	var dirs = FileUtils.list_dirs_in_directory("res://Scenes")
	dirs.erase("Main")
	dirs.sort()
	
	for dir in dirs:
		# ".remap" suffix will be appened in apk files
		var sceneFile : String = "res://Scenes/" + dir + "/" + dir + ".tscn"
		if not FileAccess.file_exists(sceneFile) and not FileAccess.file_exists(sceneFile + ".remap"):
			sceneFile = "res://Scenes/" + dir + "/Main.tscn"
		if not FileAccess.file_exists(sceneFile) and not FileAccess.file_exists(sceneFile + ".remap"):
			continue

		var btn : Button = Button.new()
		btn.text = dir + " scene"
		btn.pressed.connect(func(): get_tree().change_scene_to_file(sceneFile))
		btn.size_flags_vertical = Control.SIZE_EXPAND_FILL
		
		$VBoxContainer.add_child(btn)
	


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta):
	pass
