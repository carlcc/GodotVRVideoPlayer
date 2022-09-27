#include "MySpatial.h"
#include <core/math/transform_3d.h>

MySpatial::MySpatial()
{
    // set_process(true);
}

void MySpatial::_notification(int what)
{
    if (what == NOTIFICATION_PROCESS) {
        auto deltaTime = get_process_delta_time();
        elapsedTime_ += deltaTime;
        set_position(Vector3 { 0.0F, 2 * sinf(elapsedTime_), 0.0F });
    }
    Node3D::_notification(what);
}
void MySpatial::_bind_methods()
{
    // ClassDB::bind_method(D_METHOD("say_hello"), &MyNode::say_hello);
    //    ClassDB::bind_method(D_METHOD("_notification"), &MyNode::_notification);
    // BIND_VMETHOD(MethodInfo("_notification", PropertyInfo(Variant::INT, "what")));
}