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
signal on_paused()
signal on_stopped()
signal on_pixel_format_change(material: Material, texture: Texture)


@onready var _playButton : Button = find_child("PlayButton", true)
@onready var _progressBar : HSlider = find_child("PlayProgressBar", true)
@onready var _timeLabel : Label = find_child("TimeLabel", true)
@onready var _infoLabel : Label = find_child("InfoLabel", true)
var _mediaStream : FfmpegMediaStream
var _currentPlayState : int = FfmpegMediaStream.kStateStopped

var _isProgressBarDragging : bool = false
var _filePath: String = ""

var _currentPixelFormat : String
var _availableDecoders : Array[FfmpegCodec]
var _currentEncapsulationFormat : String
var _currentVideoEncodingFormat : String
var _currentPlaySpeedScale: float = 1.0

var _currentVideoCodec : FfmpegCodec = null
var _currentVideoCodecHw : FfmpegCodecHwConfig = null

const _kPlaySpeedScales : Array[float] = [
	0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0
]

func _updateInfoLabel():
	var str : String = ""
	str += "Current File: %s\n" % _filePath
	str += "Current PixFmt: %s\n" % _currentPixelFormat
	str += "Current Encapsulation: %s\n" % _currentEncapsulationFormat
	str += "Current Video Codec Format: %s\n" % _currentVideoEncodingFormat
	str += "Current Hw Accel: %s\n" % _currentVideoCodecHw.get_name()
	str += "Available codecs:\n"
	var availableCodecStrings : Array[String] = []
	for codec in _availableDecoders:
		var hwNames : Array[String] = []
		for hwcfg in codec.available_hw_configs():
			hwNames.push_back(hwcfg.get_name())
		str += "  {0}, with hwaccels: [{1}]\n".format([codec.get_name(), ",".join(hwNames)])
	
	_infoLabel.text = str

func _on_play_speed_selected(index: int):
	_currentPlaySpeedScale = _kPlaySpeedScales[index]

# Called when the node enters the scene tree for the first time.
func _ready():
	_playButton.pressed.connect(_on_play_pressed)
	_progressBar.drag_started.connect(_on_progress_bar_drag_begin)
	_progressBar.drag_ended.connect(_on_progress_bar_drag_end)
	var exitBtn : Button = find_child("ExitButton", true)
	exitBtn.pressed.connect(func ():
		get_tree().change_scene_to_file("res://Scenes/Main/main.tscn")
	)

	var progressDragArea : ProgressDragArea = find_child("ProgressDragArea")
	# progressDragArea.on_seek_offset.connect(_seek_offset)
	
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

	# setup play speed
	var playSpeedOption : OptionButton = find_child("PlaySpeedOption")
	for speed in _kPlaySpeedScales:
		playSpeedOption.add_item("x%.2f" % speed)
	playSpeedOption.item_selected.connect(_on_play_speed_selected)
	var defaultPlaySpeedIndex = _kPlaySpeedScales.find(1.0)
	if defaultPlaySpeedIndex == -1:
		defaultPlaySpeedIndex = 0
	playSpeedOption.select(defaultPlaySpeedIndex)


func _exit_tree():
	# make sure _mediaStream will not callback
	# after all it's children destroyed
	_mediaStream = null

func set_material_mode(mode: MaterialMode):
	_materialMode = mode
	
static func _choose_video_decoder(decoders : Array[FfmpegCodec]) -> FfmpegCodec:
	for dec in decoders:
		var hwCfgs = dec.available_hw_configs()
		if hwCfgs.size() > 0:
			return dec;
		
	if decoders.is_empty():
		return null
	return decoders[0]
	
static func _choose_video_hw_config(configs: Array[FfmpegCodecHwConfig]) -> FfmpegCodecHwConfig:
	if configs.is_empty():
		return null
	return configs[0]
	
func set_file(path: String):
	_filePath = path
	var ms = FfmpegMediaStream.new()
	if not ms.set_file(_filePath):
		return
	var decoders = ms.available_video_decoders()
	var decoder = _choose_video_decoder(decoders)
	if decoder == null:
		print("No decoder found!")
		return
	var hwCfg = _choose_video_hw_config(decoder.available_hw_configs())
	if hwCfg == null:
		print("No hw decoder found!")
	
	if !ms.create_decoders(decoder, hwCfg):
		return
	_progressBar.min_value = 0
	_progressBar.max_value = ms.get_length()
	_currentEncapsulationFormat = ms.get_encapsulation_format()
	_currentVideoEncodingFormat = ms.get_video_encoding_format()
	_availableDecoders = ms.available_video_decoders()
	_currentVideoCodecHw = hwCfg
	_updateInfoLabel()
	
	if _mediaStream != null:
		_mediaStream.disconnect("pixel_format_changed", _on_pixel_format_changed)
		_mediaStream.pixel_format_changed.disconnect(_on_pixel_format_changed);
	ms.pixel_format_changed.connect(_on_pixel_format_changed)
	ms.play_state_changed.connect(_on_stream_play_state_change)
	_mediaStream = ms
	ms.play()
	
func get_progress() -> float :
	return _progressBar.value

static func _to_hhmmss(t: float) -> String:
	var h: int = int(t / 3600)
	t = fmod(t, 3600.0)
	var m: int = int(t / 60)
	t = fmod(t, 60)
	var s: int = int(t)
	return "%02d"%h + ":%02d:"%m + "%02d"%s

const _kAutoHideDelay : float = 4
var _autoHideTimer : float = _kAutoHideDelay

func _process(delta):
	if _mediaStream != null:
		_mediaStream.update(delta * _currentPlaySpeedScale)
		if not _isProgressBarDragging:
			_progressBar.value = _mediaStream.get_position()
		_timeLabel.text = "{0}/{1}".format([_to_hhmmss(_mediaStream.get_position()), _to_hhmmss(_mediaStream.get_length())])

		# Auto hide the control ui
		if _autoHideTimer > 0:
			_autoHideTimer -= delta
			if _autoHideTimer <= 0:
				self.visible = false
			
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
	if _mediaStream == null:
		return
	if _mediaStream.is_playing():
		_mediaStream.pause()
	elif _mediaStream.is_paused():
		_mediaStream.play()
	
func _on_progress_bar_drag_begin():
	if _mediaStream == null:
		return
	_isProgressBarDragging = true
	emit_signal("on_progress_drag_begin")
		
func _on_progress_bar_drag_end(_valueChanged: bool):
	if _mediaStream == null:
		return
	_isProgressBarDragging = false
	_mediaStream.seek(get_progress())
	
func _seek_offset(offsetMs: float):
	if _mediaStream == null:
		return
	var pos = _mediaStream.get_position() + offsetMs
	pos = clampf(pos, 0, _mediaStream.get_length())
	_mediaStream.seek(pos)
	
func _on_stream_play_state_change(state: int):
	_currentPlayState = state
	if state == FfmpegMediaStream.kStatePlaying:
		emit_signal("on_play")
		_playButton.text = "pause"
	elif state == FfmpegMediaStream.kStatePaused:
		emit_signal("on_paused")
		_playButton.text = "play"
	else:
		emit_signal("on_stopped")
		_playButton.text = "play"

func _input(event):
	if event is InputEventMouseMotion or event is InputEventScreenDrag:
		self.visible = true
		self._autoHideTimer = _kAutoHideDelay
	if event is InputEventMouseButton and (event as InputEventMouseButton).is_pressed():
		self.visible = true
		self._autoHideTimer = _kAutoHideDelay
	if event is InputEventScreenTouch and (event as InputEventScreenTouch).is_pressed():
		self.visible = true
		self._autoHideTimer = _kAutoHideDelay
