"""
This script describes how the metadata from USD prims can be collected and manipulated on
the Unreal side, after fully importing a stage or opening one with a stage actor. Just
remember to set the desired metadata options on the stage actor itself if using one, as
it will not collect metadata by default (e.g. `stage_actor.set_collect_metadata(True)`).

In general the metadata will be collected into UsdAssetUserData objects, that will be added
to the generated assets (and components, if the import option is set).

All asset types that we generate (with the exeption of UPhysicsAssets) can contain
UsdAssetUserData objects and will receive USD metadata from their source prims when being
generated.

The metadata values will always be stringified and stored within the UsdAssetUserData as
strings, as there is no real way of storing anything close to a "dynamic type" or variant
type (like VtValues) with UPROPERTIES. Keep in mind that unreal.UsdConversionLibrary contains
functions that can help converting these values to and from strings into Unreal types (the
very last step of this script shows an example of this). These functions can be used from
Python or Editor Utility Blueprints.
"""

import os
import unreal

DESTINATION_CONTENT_PATH = r"/Game/Imported/"

this_directory = os.path.dirname(os.path.realpath(__file__))
full_file_path = os.path.join(this_directory, "metadata_cube.usda")

# Setup import options
options = unreal.UsdStageImportOptions()
options.import_actors = True
options.import_geometry = True
options.import_materials = True
options.metadata_options.collect_metadata = True
options.metadata_options.collect_from_entire_subtrees = True  # Collapsed assets will contain metadata from collapsed children
options.metadata_options.collect_on_components = True  # Spawned components will have Asset User Data too
options.metadata_options.blocked_prefix_filters = ["customData:int"]  # All metadata entries starting with "customData:int" will be blocked
options.metadata_options.invert_filters = False  # When this is false, the prefix filters select stuff to block, and allows everything else.
												 # When this is true, the prefix filters select stuff to allow, and block everything else

# Import scene
task = unreal.AssetImportTask()
task.set_editor_property('filename', full_file_path)
task.set_editor_property('destination_path', DESTINATION_CONTENT_PATH)
task.set_editor_property('automated', True)
task.set_editor_property('options', options)
task.set_editor_property('replace_existing', True)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
asset_tools.import_asset_tasks([task])
asset_paths = task.get_editor_property("imported_object_paths")
success = asset_paths and len(asset_paths) > 0
if not success:
	exit()

# Find our imported static mesh
asset_paths = unreal.EditorAssetLibrary.list_assets(DESTINATION_CONTENT_PATH)
for asset_path in asset_paths:
	asset = unreal.load_asset(asset_path)
	if isinstance(asset, unreal.StaticMesh):
		static_mesh = asset
		break

# Get the USD asset user data from the static mesh
aud = unreal.UsdConversionLibrary.get_usd_asset_user_data(static_mesh)

# Iterate through all the collected metadata
for stage_identifier, metadata in aud.stage_identifier_to_metadata.items():
	for prim_path, prim_metadata in metadata.prim_path_to_metadata.items():
		for key, value in prim_metadata.metadata.items():
			print(f"{stage_identifier=} {prim_path=}, {key=}, {value=}")

# Get whatever USD generated for the stage identifier
stage_identifier = list(aud.stage_identifier_to_metadata.keys())[0]

# You can omit the stage identifier and prim path if the asset user data contains exactly one entry for either
has_int_value = unreal.UsdConversionLibrary.has_metadata_field(aud, "customData:intValue")
assert(not has_int_value)  # We filtered entries starting with "customData:int", so we won't get the "intValue" entry

# Get a particular metadata value directly without iterating through the structs (there are additional functions within UsdConversionLibrary)
metadata_value = unreal.UsdConversionLibrary.get_metadata_field(aud, "customData:nestedMap:someColor", stage_identifier, "/Cube")
print(metadata_value.stringified_value)  # Prints "(1, 0.5, 0.3)". Note that stringified_value is always just a string, however
print(metadata_value.type_name)  # Prints "float3"

# Can use the library's utility functions to unstringify stringified_value directly into UE types
vector = unreal.UsdConversionLibrary.unstringify_as_float3(metadata_value.stringified_value)
assert(isinstance(vector, unreal.Vector))
print(vector)  # Prints "<Struct 'Vector' (0x0000026B026371A0) {x: 1.000000, y: 0.500000, z: 0.300000}>"


