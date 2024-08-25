"""
This script describes how to create brand new metadata for an Unreal asset, then export it
to USD and have the metadata end up on the exported prim.

Of course, it is also possible to directly export to USD assets that were originally imported
from USD with collected metadata. That metadata will be written back out to the exported prim,
and in most cases should end up identical to the metadata from the originally imported prim.

Like for the importing case on collect_metadata.py, for the exporting case the metadata should
be stored inside UsdAssetUserData objects, which are added to the UObjects to export. These
can be persisted and permanently saved with the UObjects, or just temporarily added before
export as we'll do here.

Note that we support exporting metadata for all asset types that we support exporting to USD,
including even ULevels and ULevelSequences. For levels and level sequences the metadata will
be exported to the top-level "Root" prim, that is set as the default prim on the exported layers.

When exporting levels, note that you can use `unreal.UsdConversionLibrary.set_usd_asset_user_data()`
in order to set your UsdAssetUserData on the ULevel objects from Python. It is also important to
stress that the ULevel UObjects can contain metadata, but the UWorld UObjects that own them cannot.
If you have a UWorld, you can use `levels = unreal.EditorLevelUtils.get_levels(world)` to try and get
at the persistent level owned by the UWorld, and stash your metadata there.

Additionally, when exporting levels, note that you can use the export option
"export_component_metadata" to export metadata held by individual SceneComponents on that level.
Note that metadata exported in this manner ends up on the layers corresponding to the levels
themselves, while metadata stored on assets ends up on the layers corresponding to each individual
asset. A benefit of this separation is the fact that when e.g. a StaticMeshComponent contains metadata
to export, but also its StaticMesh contains its own set of metadata to export, the two sets
will be exported to their separate layers, but composed together with regular USD composition
whenever the stage is opened. The component's metadata will provide the stronger opinion in that case.
"""

import os
import unreal

ASSET_CONTENT_PATH = "/Engine/BasicShapes/Cube.Cube"
EXPORT_FILE_PATH = r"C:/Users/<your username>/Desktop/ExportTest/output.usda"

asset = unreal.load_asset(ASSET_CONTENT_PATH)
assert(asset)

# When exporting, the stage identifier is not important: We will export all entries one after the other
# When exporting, if we have a single prim path then its actual value doesn't matter: The output prim will
# receive all entries from that single prim path regardless of what it is
aud = unreal.UsdAssetUserData()
success = unreal.UsdConversionLibrary.set_metadata_field(
	aud,
	key="customData:someColorValue",
	value=unreal.UsdConversionLibrary.stringify_as_float3(unreal.Vector(1.0, 0.5, 0.3)),
	value_type_name="float3",
	stage_identifier="StageIdentifier",
	prim_path="/SomePrimPath",
	trigger_property_change_events=False
)
assert(success)

# Set the UsdAssetUserData on the asset we're about to export
success = unreal.UsdConversionLibrary.set_usd_asset_user_data(asset, aud)
assert(success)

# Setup export options
options = unreal.StaticMeshExporterUSDOptions()
options.mesh_asset_options.material_baking_options.textures_dir = unreal.DirectoryPath(os.path.join(os.path.dirname(EXPORT_FILE_PATH), "Textures"))
options.metadata_options.export_asset_info = True  # Exports generic info about the Unreal asset, can be turned off independently of the metadata export
options.metadata_options.export_asset_metadata = True
options.metadata_options.blocked_prefix_filters = []  # We can use filters on export too, but we won't for this sample.
options.metadata_options.invert_filters = False 	  # Check collect_metadata.py for an example of filter usage

# Export the asset
task = unreal.AssetExportTask()
task.set_editor_property('object', asset)
task.set_editor_property('filename', EXPORT_FILE_PATH)
task.set_editor_property('automated', True)
task.set_editor_property('options', options)
task.set_editor_property('exporter', unreal.StaticMeshExporterUsd())
task.set_editor_property('replace_identical', True)
success = unreal.Exporter.run_asset_export_task(task)
assert(success)

# The exported stage should look something like this:
"""
#usda 1.0
(
    defaultPrim = "Cube"
    metersPerUnit = 0.01
    upAxis = "Z"
)

def Mesh "Cube" (
    assetInfo = {
        asset identifier = @C:/Users/<your username>/Desktop/ExportTest/output.usda@
        string name = "Cube"
        dictionary unreal = {
            string assetType = "StaticMesh"
            string contentPath = "/Engine/BasicShapes/Cube.Cube"
            string engineVersion = "<your engine version>"
            string exportTime = "2000.11.26-10.33.01"
        }
        string version = "895C90D17BDB5F192671D6911E2114627986D8A3"
    }
    customData = {
        float3 someColorValue = (1, 0.5, 0.3)           <---------- exported metadata
    }
)
{
	...
}
"""