"""
This script describes the basic usage of the USD Stage Actor and USD Stage Editor.
"""

import os
import unreal
from pxr import Usd, UsdUtils, UsdGeom, Gf

ROOT_LAYER_FILENAME = r"C:\MyFolder\my_scene.usda"
DESTINATION_CONTENT_PATH = r"/Game/Imported/"

# Stage Actors can be spawned on the UE level like this.
# If the Stage Editor is open when this happens, it will automatically attach to the last actor spawned.
editor_actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
stage_actor = editor_actor_subsystem.spawn_actor_from_class(unreal.UsdStageActor, unreal.Vector())

# You can open a stage and interact with it by manipulating the Stage Actor directly:
stage_actor.set_editor_property('root_layer', unreal.FilePath(ROOT_LAYER_FILENAME))

# Any stage opened by a Stage Actor will be opened within the UsdUtils' stage cache.
# This means you can retrieve the same stage via Python from it:
stage = None
for s in UsdUtils.StageCache.Get().GetAllStages():
    if s.GetRootLayer().GetDisplayName() == os.path.basename(ROOT_LAYER_FILENAME):
         stage = s
         break

# Interacting with the stage via Python will update the UE level automatically:
prim = stage.GetPrimAtPath("/Cup")
xform_api = UsdGeom.XformCommonAPI(prim)
trans, _, _, _, _ = xform_api.GetXformVectors(Usd.TimeCode.Default())
delta = Gf.Vec3d(100, 100, 100)
xform_api.SetTranslate(trans + delta)

# You can also toggle variants in this way, and both the USD Stage Editor and the UE level will update automatically:
prim = stage.GetPrimAtPath("/PrimWithALODVariantSet")
variant_sets = prim.GetVariantSets()
if variant_sets.HasVariantSet("LOD"):
    var_set = variant_sets.GetVariantSet("LOD")
    for var in var_set.GetVariantNames():
        print(f"Setting variant set '{var_set.GetName()}' with variant '{var}'")
        var_set.SetVariantSelection(var)

# Alternatively it is possible to interact with the USD Stage Editor via Python.
# Here are some examples of operations you can do. Please consult the documentation for the full list
unreal.UsdStageEditorLibrary.open_stage_editor()
unreal.UsdStageEditorLibrary.file_open(ROOT_LAYER_FILENAME)
other_stage_actor = unreal.UsdStageEditorLibrary.get_attached_stage_actor()
unreal.UsdStageEditorLibrary.set_attached_stage_actor(stage_actor)
selected_prims = unreal.UsdStageEditorLibrary.get_selected_prim_paths()

# You can even trigger the Actions->Import button from here, as a way of importing
# the current state of the opened stage:
options = unreal.UsdStageImportOptions()
options.import_actors = True
options.import_geometry = True
options.import_skeletal_animations = True
options.import_level_sequences = True
options.import_materials = True
options.prim_path_folder_structure = False
options.prims_to_import = [
    "/ParentA",
    "/ParentB/ChildC"
]
unreal.UsdStageEditorLibrary.actions_import(DESTINATION_CONTENT_PATH, options)