extends Node

class_name PlayingControlPanel

enum MaterialMode {
	k2d,
	k3d,
	kPanorama,
};

@export var materialNv12 : ShaderMaterial
@export var materialYuv420P : ShaderMaterial

@export var materialNv12_3D : ShaderMaterial
@export var materialYuv420P_3D : ShaderMaterial

@export var materialNv12_Panorama : ShaderMaterial
@export var materialYuv420P_Panorama : ShaderMaterial

var _materialMode: MaterialMode = MaterialMode.k2d

signal on_play()
signal on_progress_drag_begin()
signal on_progress_drag_end(valueChanged: bool)
signal on_pixel_format_change(material: Material, texture: Texture)


@onready var _playButton : Button = find_child("PlayButton", true)
@onready var _progressBar : HSlider = find_child("PlayProgressBar", true)
@onready var _timeLabel : Label = find_child("TimeLabel", true)
@onready var _infoLabel : Label = find_child("InfoLabel", true)
var _mediaStream : FfmpegMediaStream

var _isProgressBarDragging : bool = false
var _filePath: String = ""

var _currentHwAccel : String
var _currentPixelFormat : String
var _availableHwDecoders: PackedStringArray

func _updateInfoLabel():
	_infoLabel.text = "Current File: {0}\nAvailable HwAccels: {1}\nCurrent HwAccel: {2}\nCurrent PixFmt: {3}".format(
		[_filePath, _availableHwDecoders, _currentHwAccel, _currentPixelFormat])

# Called when the node enters the scene tree for the first time.
func _ready():
	_playButton.pressed.connect(_on_play_pressed)
	_progressBar.drag_started.connect(_on_progress_bar_drag_begin)
	_progressBar.drag_ended.connect(_on_progress_bar_drag_end)
	var exitBtn : Button = find_child("ExitButton", true)
	exitBtn.pressed.connect(func (): get_tree().change_scene_to_file("res://Scenes/Main/main.tscn"))
	
	# TODO: Configurable scan directory
	var videoFileList : OptionButton = find_child("VideoFileList")
	for f in FileUtils.list_files_in_directory_absolute("res://Data"):
		videoFileList.add_item(f)
	for f in FileUtils.list_files_in_directory_absolute("/sdcard/Videos"):
		videoFileList.add_item(f)
	for f in FileUtils.list_files_in_directory_absolute(OS.get_system_dir(OS.SYSTEM_DIR_MOVIES)):
		videoFileList.add_item(f)
		
	if videoFileList.get_item_count() > 0:
		set_file(videoFileList.get_item_text(0))
	videoFileList.item_selected.connect(func (index:int):
		set_file(videoFileList.get_item_text(index))
	)

func set_material_mode(mode: MaterialMode):
	_materialMode = mode
	
func set_file(path: String):
	_filePath = path

func set_progress(value: float):
	_progressBar.value = value

func set_min_progress(value: float):
	_progressBar.min_value = value
	
func set_max_progress(value: float):
	_progressBar.max_value = value
	
func get_progress() -> float :
	return _progressBar.value

static func _to_hhmmss(t: float) -> String:
	var h: int = t / 3600
	t = fmod(t, 3600.0)
	var m: int = t / 60
	t = fmod(t, 60)
	var s: int = t
	return "%02d"%h + ":%02d:"%m + "%02d"%s

func _process(delta):
	if _mediaStream != null:
		_mediaStream.update(delta)
		if not _isProgressBarDragging:
			set_progress(_mediaStream.get_position())
		_timeLabel.text = "{0}/{1}".format([_to_hhmmss(_mediaStream.get_position()), _to_hhmmss(_mediaStream.get_length())])
			
func _on_pixel_format_changed(fmt: int):
	var material: Material = null
	if fmt == FfmpegMediaStream.kPixelFormatNv12:
		if _materialMode == MaterialMode.k3d:
			material = materialNv12_3D.duplicate()
		elif _materialMode == MaterialMode.k2d:
			material = materialNv12.duplicate()
		else:
			material = materialNv12_Panorama.duplicate()
		material.set_shader_parameter("yTexture", _mediaStream.get_texture(0))
		material.set_shader_parameter("uvTexture", _mediaStream.get_texture(1))
		_currentPixelFormat = "Nv12"
	elif fmt == FfmpegMediaStream.kPixelFormatYuv420P:
		if _materialMode == MaterialMode.k3d:
			material = materialYuv420P_3D.duplicate()
		elif _materialMode == MaterialMode.k2d:
			material = materialYuv420P.duplicate()
		else:
			material = materialYuv420P_Panorama.duplicate()
		material.set_shader_parameter("yTexture", _mediaStream.get_texture(0))
		material.set_shader_parameter("uTexture", _mediaStream.get_texture(1))
		material.set_shader_parameter("vTexture", _mediaStream.get_texture(2))
		_currentPixelFormat = "Yuv420P"
	else:
		print("Unsupported pixel format")
		_currentPixelFormat = "Unknown"
	emit_signal("on_pixel_format_change", material, _mediaStream.get_texture(0))
	print("Pixel format changed!")
	_updateInfoLabel()
	
func _on_play_pressed():
	var ms = FfmpegMediaStream.new()
	if not ms.set_file(_filePath):
		return
	var hwAccels = ms.available_video_hardware_accelerators()
	print("Available video hw decoders: ", hwAccels)
	var hw = "none"
	if not hwAccels.is_empty():
		hw = hwAccels[0]
	ms.create_decoders(hw)
	set_max_progress(ms.get_length())
	set_min_progress(0)
	_availableHwDecoders = hwAccels
	_currentHwAccel = hw
	_updateInfoLabel()
	
	
	if _mediaStream != null:
		_mediaStream.disconnect("pixel_format_changed", _on_pixel_format_changed)
		_mediaStream.pixel_format_changed.disconnect(_on_pixel_format_changed);
	ms.pixel_format_changed.connect(_on_pixel_format_changed)
	_mediaStream = ms
	ms.play()
	emit_signal("on_play")
	
func _on_progress_bar_drag_begin():
	if _mediaStream == null:
		return
	_isProgressBarDragging = true
	emit_signal("on_progress_drag_begin")
		
func _on_progress_bar_drag_end(valueChanged: bool):
	if _mediaStream == null:
		return
	_isProgressBarDragging = false
	if valueChanged:
		_mediaStream.seek(get_progress())
	emit_signal("on_progress_drag_end", valueChanged)
	
