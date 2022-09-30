extends Node

class_name PlayingControlPanel

@export var materialNv12 : ShaderMaterial
@export var materialYuv420P : ShaderMaterial

@export var materialNv12_3D : ShaderMaterial
@export var materialYuv420P_3D : ShaderMaterial

var _is3dMaterial: bool = false

signal on_play()
signal on_progress_drag_begin()
signal on_progress_drag_end(valueChanged: bool)
signal on_pixel_format_change(material: Material, texture: Texture)


@onready var _playButton : Button = find_child("PlayButton", true)
@onready var _progressBar : HSlider = find_child("PlayProgressBar", true)
var _mediaStream : FfmpegMediaStream

var _isProgressBarDragging : bool = false
var _filePath: String = "res://Data/shjx.mp4"

# Called when the node enters the scene tree for the first time.
func _ready():
	_playButton.pressed.connect(_on_play_pressed)
	_progressBar.drag_started.connect(_on_progress_bar_drag_begin)
	_progressBar.drag_ended.connect(_on_progress_bar_drag_end)

func set_3d_material(b: bool):
	_is3dMaterial = b
	
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

func _process(delta):
	if _mediaStream != null:
		_mediaStream.update(delta)
		if not _isProgressBarDragging:
			set_progress(_mediaStream.get_position())
			
func _on_pixel_format_changed(fmt: int):
	var material: Material = null
	if fmt == FfmpegMediaStream.kPixelFormatNv12:
		if _is3dMaterial:
			material = materialNv12_3D.duplicate()
		else:
			material = materialNv12.duplicate()
		material.set_shader_parameter("yTexture", _mediaStream.get_texture(0))
		material.set_shader_parameter("uvTexture", _mediaStream.get_texture(1))
	elif fmt == FfmpegMediaStream.kPixelFormatYuv420P:
		if _is3dMaterial:
			material = materialYuv420P_3D.duplicate()
		else:
			material = materialYuv420P.duplicate()
		material.set_shader_parameter("yTexture", _mediaStream.get_texture(0))
		material.set_shader_parameter("uTexture", _mediaStream.get_texture(1))
		material.set_shader_parameter("vTexture", _mediaStream.get_texture(2))
	else:
		print("Unsupported pixel format")	
	emit_signal("on_pixel_format_change", material, _mediaStream.get_texture(0))
	print("Pixel format changed!")
	
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
	
