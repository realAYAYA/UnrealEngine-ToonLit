import unreal

file_a = "C:\\MyScenes\\file_a.fbx"
file_b = "C:\\MyScenes\\file_a.fbx"
imported_scenes_path = "/Game/ImportedScenes"

print 'Preparing import options...'
advanced_mesh_options = unreal.DatasmithStaticMeshImportOptions()
advanced_mesh_options.set_editor_property('max_lightmap_resolution', unreal.DatasmithImportLightmapMax.LIGHTMAP_512)
advanced_mesh_options.set_editor_property('min_lightmap_resolution', unreal.DatasmithImportLightmapMin.LIGHTMAP_64)
advanced_mesh_options.set_editor_property('generate_lightmap_u_vs', True)
advanced_mesh_options.set_editor_property('remove_degenerates', True)

base_options = unreal.DatasmithImportBaseOptions()
base_options.set_editor_property('include_geometry', True)
base_options.set_editor_property('include_material', True)
base_options.set_editor_property('include_light', True)
base_options.set_editor_property('include_camera', True)
base_options.set_editor_property('include_animation', True)
base_options.set_editor_property('static_mesh_options', advanced_mesh_options)
base_options.set_editor_property('scene_handling', unreal.DatasmithImportScene.CURRENT_LEVEL)
base_options.set_editor_property('asset_options', [])  # Not used

vred_options = unreal.DatasmithVREDImportOptions()
vred_options.set_editor_property('merge_nodes', False)
vred_options.set_editor_property('optimize_duplicated_nodes', False)
vred_options.set_editor_property('import_var', True)
vred_options.set_editor_property('var_path', "")
vred_options.set_editor_property('import_light_info', True)
vred_options.set_editor_property('light_info_path', "")
vred_options.set_editor_property('import_clip_info', True)
vred_options.set_editor_property('clip_info_path', "")
vred_options.set_editor_property('textures_dir', "")
vred_options.set_editor_property('import_animations', True)
vred_options.set_editor_property('intermediate_serialization', unreal.DatasmithVREDIntermediateSerializationType.DISABLED)
vred_options.set_editor_property('colorize_materials', False)
vred_options.set_editor_property('generate_lightmap_u_vs', False)
vred_options.set_editor_property('import_animations', True)

# Direct import to scene and assets:
print 'Importing directly to scene...'
unreal.VREDLibrary.import_(file_a, imported_scenes_path, base_options, None, True)

#2-stage import step 1:
print 'Parsing to scene object...'
scene = unreal.DatasmithVREDSceneElement.construct_datasmith_scene_from_file(file_b, imported_scenes_path, base_options, vred_options)
print 'Resulting datasmith scene: ' + str(scene)
print '\tProduct name: ' + str(scene.get_product_name())
print '\tMesh actor count: ' + str(len(scene.get_all_mesh_actors()))
print '\tLight actor count: ' + str(len(scene.get_all_light_actors()))
print '\tCamera actor count: ' + str(len(scene.get_all_camera_actors()))
print '\tCustom actor count: ' + str(len(scene.get_all_custom_actors()))
print '\tMaterial count: ' + str(len(scene.get_all_materials()))
print '\tAnimNode count: ' + str(len(scene.get_all_anim_nodes()))
print '\tAnimClip count: ' + str(len(scene.get_all_anim_clips()))
print '\tExtra light info count: ' + str(len(scene.get_all_extra_lights_info()))
print '\tVariant count: ' + str(len(scene.get_all_variants()))

# Modify one of the AnimNodes
# Warning: The AnimNode nested structure is all USTRUCTs, which are value types, and the Array accessor returns 
# a copy. Meaning something like anim_nodes[0].name = 'new_name' will set the name on the COPY of anim_nodes[0]
anim_nodes = scene.get_all_anim_nodes()
if len(anim_nodes) > 0:
    node_0 = anim_nodes[0]
    old_name = node_0.name 
    print 'Anim node old name: ' + old_name
    
    node_0.name += '_MODIFIED'
    modified_name = node_0.name    
    print 'Anim node modified name: ' + modified_name
    
    anim_nodes[0] = node_0
    scene.set_all_anim_nodes(anim_nodes)
    
    # Check modification
    new_anim_nodes = scene.get_all_anim_nodes()
    print 'Anim node retrieved modified name: ' + new_anim_nodes[0].name
    assert new_anim_nodes[0].name == modified_name, "Node modification didn't work!"

    # Restore to previous state
    node_0 = new_anim_nodes[0]
    node_0.name = old_name
    new_anim_nodes[0] = node_0
    scene.set_all_anim_nodes(new_anim_nodes)
    
# 2-stage import step 2:
print 'Importing assets and actors...'
result = scene.import_scene()
print 'Import results: '
print '\tImported actor count: ' + str(len(result.imported_actors))
print '\tImported mesh count: ' + str(len(result.imported_meshes))
print '\tImported level sequences: ' + str([a.get_name() for a in result.animations])
print '\tImported level variant sets asset: ' + str(result.level_variant_sets.get_name())
if result.import_succeed:
    print 'Import succeeded!'
else:
    print 'Import failed!'
