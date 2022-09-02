#pragma once
#include <scene/3d/spatial.h>

class MySpatial : public Spatial {
    GDCLASS(MySpatial, Spatial);
    OBJ_CATEGORY("3D");

public:
    void _notification(int what);
    static void _bind_methods();

private:
    float elapsedTime_ { 0.0F };
};

