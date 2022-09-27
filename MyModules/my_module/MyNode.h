#pragma once

#include <core/string/ustring.h>
#include <scene/main/node.h>

class MyNode : public Node {
    GDCLASS(MyNode, Node);

public:
    MyNode();
    ~MyNode();
    String say_hello();
    void _notification(int what);

    static void _bind_methods();
};
