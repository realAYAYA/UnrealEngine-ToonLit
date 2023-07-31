import unreal

OUTPUT_FILENAME = r"C:\MyFolder\output.usda"
INPUT_ASSET_CONTENT_PATH = r"/Game/MyStaticMesh"

asset = unreal.load_asset(INPUT_ASSET_CONTENT_PATH)

options = unreal.StaticMeshExporterUSDOptions()
options.stage_options.meters_per_unit = 0.02
options.mesh_asset_options.use_payload = True
options.mesh_asset_options.payload_format = "usda"
options.mesh_asset_options.bake_materials = True

task = unreal.AssetExportTask()
task.set_editor_property('object', asset)
task.set_editor_property('filename', OUTPUT_FILENAME)
task.set_editor_property('automated', True)
task.set_editor_property('options', options)
task.set_editor_property('exporter', unreal.StaticMeshExporterUsd())

unreal.Exporter.run_asset_export_task(task)