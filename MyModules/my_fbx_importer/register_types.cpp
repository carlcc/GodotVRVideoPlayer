#include "register_types.h"
#include <editor/editor_node.h>
#include "editor_scene_importer_my_fbx.h"

#ifdef TOOLS_ENABLED

static void _editor_init() {
    Ref<EditorSceneImporterMyFbx> import_fbx;
    import_fbx.instance();
    ResourceImporterScene::get_singleton()->add_importer(import_fbx);
}

#endif

void register_my_fbx_importer_types() {
#ifdef TOOLS_ENABLED
    ClassDB::APIType prev_api = ClassDB::get_current_api();
    ClassDB::set_current_api(ClassDB::API_EDITOR);

    ClassDB::register_class<EditorSceneImporterMyFbx>();

    ClassDB::set_current_api(prev_api);

    EditorNode::add_init_callback(_editor_init);
#endif
}

void unregister_my_fbx_importer_types() {
}
