#pragma once

#include <core/ustring.h>
#include <scene/main/node.h>

class MyNode : public Node {
    GDCLASS(MyNode, Node);
    OBJ_CATEGORY("Nodes");

public:
    String say_hello();
    void _notification(int what);

    static void _bind_methods();
};
