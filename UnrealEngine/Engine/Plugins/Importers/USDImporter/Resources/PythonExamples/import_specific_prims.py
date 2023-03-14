"""
This script describes how to import a USD Stage into actors and assets, also optionally specifying specific prim
paths to import.

The `prims_to_import` property can be left at its default value of ["/"] (a list with just the "/" prim path in it)
to import the entire stage.

The provided paths in `prims_to_import` are used as a USD population mask when opening the stage, and as such are
subject to the rules described here: https://graphics.pixar.com/usd/release/api/class_usd_stage_population_mask.html

As an example, consider the following USD stage:

        #usda 1.0
        def Xform "ParentA"
        {
            def Xform "ChildA"
            {
            }

            def Xform "ChildB"
            {
            }
        }
        def Xform "ParentB"
        {
            def Xform "ChildC"
            {
            }

            def Xform "ChildD"
            {
            }
        }

In general, the main thing to keep in mind is that if "/ParentA" is within `prims_to_import`, ParentA and *all* of its
children will be imported. As a consequence, having both "/ParentA" and "/ParentA/ChildA" on the list is reduntant, as
"/ParentA" will already lead to "/ParentA/ChildA" being imported, as previously mentioned.
"""

import unreal

ROOT_LAYER_FILENAME = r"C:\MyFolder\my_scene.usda"
DESTINATION_CONTENT_PATH = r"/Game/Imported/"

options = unreal.UsdStageImportOptions()
options.import_actors = True
options.import_geometry = True
options.import_skeletal_animations = True
options.import_level_sequences = True
options.import_materials = True
options.prim_path_folder_structure = False
options.prims_to_import = [
    "/ParentA",  # This will import ParentA, ChildA and ChildB
    "/ParentB/ChildC"  # This will import ParentB and ChildC only (and *not* import ChildD)
]

task = unreal.AssetImportTask()
task.set_editor_property('filename', ROOT_LAYER_FILENAME)
task.set_editor_property('destination_path', DESTINATION_CONTENT_PATH)
task.set_editor_property('automated', True)
task.set_editor_property('options', options)
task.set_editor_property('replace_existing', True)

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
asset_tools.import_asset_tasks([task])

asset_paths = task.get_editor_property("imported_object_paths")
success = asset_paths and len(asset_paths) > 0