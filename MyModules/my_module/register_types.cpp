#include "register_types.h"
#include "MyNode.h"
#include "MySpatial.h"

void register_my_module_types()
{
    ClassDB::register_class<MyNode>();
    ClassDB::register_class<MySpatial>();
}

void unregister_my_module_types()
{
}
