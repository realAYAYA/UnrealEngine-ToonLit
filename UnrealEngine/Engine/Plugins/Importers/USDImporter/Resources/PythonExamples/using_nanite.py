"""
This script describes how to open and import USD Stages in ways that automatically configure the generated StaticMeshes
for Nanite.

In summary, there are two different settings you can use: The NaniteTriangleThreshold property (any static mesh
generated with this many triangles or more will be enabled for Nanite), and the 'unrealNanite' property (force Nanite
enabled or disabled on a per-prim basis).

Note that there are two cases in which Nanite will not be enabled for generated StaticMeshes, even when using the
above settings:
    - When the Mesh prim is configured for multiple LODs (by using Variants and Variant Sets);
    - When the generated StaticMesh would have led to more than 64 material slots.

Additionally, when exporting existing UStaticMeshes to USD Mesh prims we will automatically emit the
`uniform token unrealNanite = "enable"` attribute whenever the source UStaticMesh has "Enable Nanite Support" checked.

The text below describes a "sample_scene.usda" file (some attributes omitted for brevity):


    # Assuming a triangle threshold of 2000, this prim will generate a StaticMesh with Nanite disabled:
    def Mesh "small_mesh"
    {
        # Mesh data with 1000 triangles
    }


    # Assuming a triangle threshold of 2000, this prim will generate a StaticMesh with Nanite enabled:
    def Mesh "large_mesh"
    {
        # Mesh data with 5000 triangles
    }


    # Assuming a triangle threshold of 2000, this prim will generate a StaticMesh with Nanite enabled:
    def Mesh "small_but_enabled"
    {
        # Mesh data with 1000 triangles

        uniform token unrealNanite = "enable"
    }


    # Assuming a triangle threshold of 2000, this prim will generate a StaticMesh with Nanite disabled:
    def Mesh "large_but_disabled"
    {
        # Mesh data with 5000 triangles

        uniform token unrealNanite = "disable"
    }


    # Assuming a triangle threshold of 2000 and that we're collapsing prims with "component" kind,
    # this prim hierarchy will lead to a StaticMesh with Nanite enabled, as the final StaticMesh will end up with 2000
    # total triangles.
    def Xform "nanite_collapsed" ( kind = "component" )
    {
        def Mesh "small_mesh_1"
        {
            # Mesh data with 1000 triangles
        }

        def Mesh "small_mesh_2"
        {
            # Mesh data with 1000 triangles
        }
    }


    # Assuming a triangle threshold of 2000 and that we're collapsing prims with "component" kind,
    # this prim hierarchy will lead to a StaticMesh with Nanite enabled. The combined triangle count is below the
    # threshold, however we have an 'unrealNanite' opinion on the root of the collapsed hierarchy that overrides it.
    # Note that in case of collapsing, we will only consider the 'unrealNanite' attribute on the root, and will
    # disregard opinions for it specified on each individual Mesh prim.
    def Xform "nanite_collapsed" ( kind = "component" )
    {
        uniform token unrealNanite = "enable"

        def Mesh "small_mesh_1"
        {
            # Mesh data with 500 triangles
        }

        def Mesh "small_mesh_2"
        {
            # Mesh data with 500 triangles
        }
    }
"""

import unreal

ROOT_LAYER_FILENAME = r"C:\MyFolder\sample_scene.usda"
DESTINATION_CONTENT_PATH = r"/Game/Imported/"


def specify_nanite_when_importing():
    """ Describes how to specify the Nanite triangle threshold when importing a stage """
    options = unreal.UsdStageImportOptions()
    options.import_actors = True
    options.import_geometry = True
    options.import_skeletal_animations = True
    options.nanite_triangle_threshold = 2000  # Use your threshold here

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


def specify_nanite_when_opening():
    """ Describes how to specify the Nanite triangle threshold when opening a stage """
    editor_actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    stage_actor = editor_actor_subsystem.spawn_actor_from_class(unreal.UsdStageActor, unreal.Vector())

    # Either one of the two lines below should work, without any difference
    stage_actor.set_editor_property('nanite_triangle_threshold', 2000)
    stage_actor.set_nanite_triangle_threshold(2000)

    stage_actor.set_editor_property('root_layer', unreal.FilePath(ROOT_LAYER_FILENAME))