extends Control

class_name ProgressDragArea

@export var drag_sensitivity : float = 0.1

signal on_seek_offset(deltaTimeMs: float)

var _is_mouse_down : bool = false
var _mouse_down_motion: Vector2 = Vector2(0, 0)
var _is_mouse_in_dead_zone : bool = false

var _is_finger_down : bool = false
var _finger_down_motion: Vector2 = Vector2(0, 0)
var _is_finger_in_dead_zone : bool = false


const kPreviewLabelLayoutOffset : Vector2 = Vector2(0, -40)
const kDeadZoneSize : float = 10


@onready var _mouseOffsetPreviewLabel : Label = find_child("MouseOffsetPreviewLabel", true)
@onready var _fingerOffsetPreviewLabel : Label = find_child("FingerOffsetPreviewLabel", true)

func _add_mouse_down_motion(delta: Vector2):
	_mouse_down_motion += delta
	_mouseOffsetPreviewLabel.text = "%.1f"%_offset_to_delta_time(_mouse_down_motion.x)
	_mouseOffsetPreviewLabel.position += delta
	if _is_mouse_in_dead_zone:
		if absf(_mouse_down_motion.x) >= kDeadZoneSize:
			_is_mouse_in_dead_zone = false
			_mouseOffsetPreviewLabel.visible = true;
	
func _add_finger_down_motion(delta: Vector2):
	_finger_down_motion += delta
	_fingerOffsetPreviewLabel.text = "%.1f"%_offset_to_delta_time(_finger_down_motion.x)
	_fingerOffsetPreviewLabel.position += delta
	if _is_finger_in_dead_zone:
		if absf(_mouse_down_motion.x) >= kDeadZoneSize:
			_is_finger_in_dead_zone = false
			_fingerOffsetPreviewLabel.visible = true;
	
# Called when the node enters the scene tree for the first time.
func _ready():
	pass # Replace with function body.
	
func _offset_to_delta_time(offset: float) -> float:
	return offset * drag_sensitivity
	
func _handle_drag_result(motion: Vector2):
	emit_signal("on_seek_offset", _offset_to_delta_time(motion.x))
	pass
	
func _is_point_in_area(p: Vector2):
	var r : Rect2 = Rect2(global_position, size)
	return r.has_point(p)
	
func _input(event: InputEvent):
	if event is InputEventMouseButton:
		var mbte = event as InputEventMouseButton
		if mbte.button_index == MOUSE_BUTTON_LEFT:
			if mbte.pressed and not _is_point_in_area(mbte.position):
				return
			_is_mouse_down = mbte.pressed
			if _is_mouse_down:
				_mouse_down_motion = Vector2(0, 0)
				_mouseOffsetPreviewLabel.global_position = mbte.position + kPreviewLabelLayoutOffset
				_add_mouse_down_motion(Vector2(0, 0))
			else:
				_mouseOffsetPreviewLabel.visible = false
				if not _is_mouse_in_dead_zone:
					_handle_drag_result(_mouse_down_motion)
				_is_mouse_in_dead_zone = true
		return
	
	if event is InputEventScreenTouch:
		var te = event as InputEventScreenTouch
		if te.index == 0 and _is_point_in_area(te.position):
			if te.pressed and not _is_point_in_area(te.position):
				return
			_is_finger_down = te.pressed
			if _is_finger_down:
				_finger_down_motion = Vector2(0, 0)
				_fingerOffsetPreviewLabel.global_position = te.position + kPreviewLabelLayoutOffset
				_add_finger_down_motion(Vector2(0, 0))
			else:
				_fingerOffsetPreviewLabel.visible = false;
				if not _is_finger_in_dead_zone:
					_handle_drag_result(_finger_down_motion)
				_is_finger_in_dead_zone = true
		return
	
	if event is InputEventMouseMotion:
		if _is_mouse_down:
			var me = event as InputEventMouseMotion
			_add_mouse_down_motion(me.relative)
		return
		
	if event is InputEventScreenDrag:
		if _is_finger_down:
			var de = event as InputEventScreenDrag
			if de.index == 0:
				_finger_down_motion += de.relative
				_add_finger_down_motion(de.relative)
		return
 
# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta):
	pass
