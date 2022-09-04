#include "editor_scene_importer_my_fbx.h"
#include <scene/3d/spatial.h>

EditorSceneImporterMyFbx::EditorSceneImporterMyFbx()
{
}

EditorSceneImporterMyFbx::~EditorSceneImporterMyFbx()
{
}

uint32_t EditorSceneImporterMyFbx::get_import_flags() const
{
    return IMPORT_SCENE | IMPORT_ANIMATION;
}

void EditorSceneImporterMyFbx::get_extensions(List<String>* r_extensions) const
{
    r_extensions->push_back("myfbx");
}

Node* EditorSceneImporterMyFbx::import_scene(const String& p_path, uint32_t p_flags, int p_bake_fps,
    uint32_t p_compress_flags, List<String>* r_missing_deps, Error* r_err)
{
    Spatial* node = memnew(Spatial);
    node->set_name("MyFbxRoot");

    auto* child = memnew(Spatial);
    child->set_name("ChildNode");
    node->add_child(child);
    child->set_owner(node);
    return node;
}

Ref<Animation> EditorSceneImporterMyFbx::import_animation(const String& p_path, uint32_t p_flags, int p_bake_fps)
{
    return nullptr;
}