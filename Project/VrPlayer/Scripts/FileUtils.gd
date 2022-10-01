extends Object

class_name FileUtils

static func list_dirs_in_directory(path: String):
	var files = []
	var dir = DirAccess.open(path)
	if dir != null:
		dir.list_dir_begin()
		while true:
			var file = dir.get_next()
			if file == "":
				break
			elif not file.begins_with(".") and dir.current_is_dir():
				files.append(file)
		dir.list_dir_end()

	return files

static func list_dirs_in_directory_absolute(path: String):
	var ret = []
	if not path.ends_with("/") and not path.ends_with("\\"):
		path += '/'
	for f in list_dirs_in_directory_absolute(path):
		ret.append(path + f)
	return ret

static func list_files_in_directory(path: String, fileOnly: bool = true):
	var files = []
	var dir = DirAccess.open(path)
	if dir != null:
		dir.list_dir_begin()
		while true:
			var file = dir.get_next()
			if file == "":
				break
			elif not file.begins_with("."):
				if fileOnly and dir.current_is_dir():
					pass
				else:
					files.append(file)
		dir.list_dir_end()

	return files

static func list_files_in_directory_absolute(path: String, fileOnly: bool = true):
	var ret = []
	if not path.ends_with("/") and not path.ends_with("\\"):
		path += '/'
	for f in list_files_in_directory(path, fileOnly):
		ret.append(path + f)
	return ret
