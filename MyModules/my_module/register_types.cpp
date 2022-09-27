#include "register_types.h"
#include "MyNode.h"
#include "MySpatial.h"

void initialize_my_module_module(ModuleInitializationLevel level)
{
    if (level == ModuleInitializationLevel::MODULE_INITIALIZATION_LEVEL_SCENE) {
        ClassDB::register_class<MyNode>();
        ClassDB::register_class<MySpatial>();
    }
}

void uninitialize_my_module_module(ModuleInitializationLevel level)
{
}
