extends Node

@export var materialNv12 : ShaderMaterial
@export var materialYuv420P : ShaderMaterial
	
var textureRect :TextureRect
var mediaStream : FfmpegMediaStream
var playProgressBar : HSlider

var isProgressBarDragging : bool = false

# var currentPixelFormat = kPixelFormatNone

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
	# NONE
	# VDPAU
	# CUDA
	# VAAPI
	# DXVA2
	# QSV
	# VIDEOTOOLBOX
	# D3D11VA
	# DRM
	# OPENCL
	# MEDIACODEC
	# VULKAN
	var hw = "dxva2"
	if not ms.set_file("res://Data/shjx.mp4"):
		return
	print("Available video hw decoders: ", ms.available_video_hardware_accelerators())
	ms.create_decoders(hw)
	playProgressBar.max_value = ms.get_length()
	playProgressBar.value = 0
	
	if mediaStream != null:
		mediaStream.disconnect("pixel_format_changed", _on_pixel_format_changed)
		mediaStream.pixel_format_changed.disconnect(_on_pixel_format_changed);
	ms.pixel_format_changed.connect(_on_pixel_format_changed)
	mediaStream = ms
	ms.play()

func _on_pixel_format_changed(fmt):
	if fmt == FfmpegMediaStream.kPixelFormatNv12:
		var material = materialNv12.duplicate()
		textureRect.texture = mediaStream.get_texture(0) # so that the size is correct
		textureRect.material = material
		material.set_shader_parameter("yTexture", mediaStream.get_texture(0))
		material.set_shader_parameter("uvTexture", mediaStream.get_texture(1))
	elif fmt == FfmpegMediaStream.kPixelFormatYuv420P:
		var material = materialYuv420P.duplicate()
		textureRect.texture = mediaStream.get_texture(0) # so that the size is correct
		textureRect.material = material
		material.set_shader_parameter("yTexture", mediaStream.get_texture(0))
		material.set_shader_parameter("uTexture", mediaStream.get_texture(1))
		material.set_shader_parameter("vTexture", mediaStream.get_texture(2))
	else:
		print("Unsupported pixel format")
	print("Pixel format changed!")
	pass


func _on_play_progress_bar_drag_started():
	isProgressBarDragging = true
	pass # Replace with function body.


func _on_play_progress_bar_drag_ended(value_changed):
	if mediaStream == null:
		return
	isProgressBarDragging = false
	if value_changed:
		mediaStream.seek(playProgressBar.value)
	pass # Replace with function body.
