#include "MyNode.h"

MyNode::MyNode()
{
    printf("Create %p...\n", this);
}

MyNode::~MyNode()
{
    printf("Destory %p...\n", this);
}

String MyNode::say_hello()
{
    return "hello from my module";
}

void MyNode::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("say_hello"), &MyNode::say_hello);
    //    ClassDB::bind_method(D_METHOD("_notification"), &MyNode::_notification);
    // BIND_VMETHOD(MethodInfo("_notification", PropertyInfo(Variant::INT, "what")));
}
void MyNode::_notification(int what)
{
    printf("Got notification: %d\n", what);
    Node::_notification(what);
}

