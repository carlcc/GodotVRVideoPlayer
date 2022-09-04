#pragma once

#ifdef TOOLS_ENABLED

#include "editor/import/resource_importer_scene.h"
#include "editor/project_settings_editor.h"

class EditorSceneImporterMyFbx : public EditorSceneImporter {
    GDCLASS(EditorSceneImporterMyFbx, EditorSceneImporter);

public:
    EditorSceneImporterMyFbx();

    ~EditorSceneImporterMyFbx();

    uint32_t get_import_flags() const;

    void get_extensions(List<String>* r_extensions) const;

    Node* import_scene(const String& p_path, uint32_t p_flags, int p_bake_fps,
        uint32_t p_compress_flags, List<String>* r_missing_deps, Error* r_err);

    Ref<Animation> import_animation(const String& p_path, uint32_t p_flags, int p_bake_fps);
};

#endif
