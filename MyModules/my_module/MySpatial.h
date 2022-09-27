#pragma once
#include <scene/3d/node_3d.h>

class MySpatial : public Node3D {
    GDCLASS(MySpatial, Node3D);

public:
    MySpatial();

    void _notification(int what);
    static void _bind_methods();

private:
    float elapsedTime_ { 0.0F };
};

