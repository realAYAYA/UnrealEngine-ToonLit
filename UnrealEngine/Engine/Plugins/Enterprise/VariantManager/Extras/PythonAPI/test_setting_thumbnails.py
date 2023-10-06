# This script describes the different ways of setting variant thumbnails via the Python API

import unreal

def import_texture(filename, contentpath):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', filename)
    task.set_editor_property('destination_path', contentpath)
    task.automated = True

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset_tools.import_asset_tasks([task])

    asset_paths = task.get_editor_property("imported_object_paths")
    if not asset_paths:
        unreal.log_warning("No assets were imported!")
        return None

    return unreal.load_asset(asset_paths[0])

if __name__ == "__main__":
    lvs = unreal.VariantManagerLibrary.create_level_variant_sets_asset("LVS", "/Game/")
    lvs_actor = unreal.VariantManagerLibrary.create_level_variant_sets_actor(lvs)
    if lvs is None or lvs_actor is None:
        print ("Failed to spawn either the LevelVariantSets asset or the LevelVariantSetsActor!")
        quit()

    var_set1 = unreal.VariantSet()
    var_set1.set_display_text("My VariantSet")

    varTexture = unreal.Variant()
    varTexture.set_display_text("From texture")

    varPath = unreal.Variant()
    varPath.set_display_text("From path")

    varCam = unreal.Variant()
    varCam.set_display_text("From cam")

    varViewport = unreal.Variant()
    varViewport.set_display_text("From viewport")

    lvs.add_variant_set(var_set1)
    var_set1.add_variant(varTexture)
    var_set1.add_variant(varPath)
    var_set1.add_variant(varCam)
    var_set1.add_variant(varViewport)

    # Set thumbnail from an unreal texture
    texture = import_texture("C:\\Path\\To\\Image.jpg", "/Game/Textures")
    if texture:
        varTexture.set_thumbnail_from_texture(texture)
        var_set1.set_thumbnail_from_texture(texture)

    # Set thumbnail directly from a filepath
    varPath.set_thumbnail_from_file("C:\\Path\\To\\Image.png")

    # Set thumbnail from camera transform and properties
    trans = unreal.Transform()
    fov = 50
    minZ = 50
    gamma = 2.2
    varCam.set_thumbnail_from_camera(lvs_actor, trans, fov, minZ, gamma)

    # Set thumbnail directly from the active editor viewport
    varViewport.set_thumbnail_from_editor_viewport()