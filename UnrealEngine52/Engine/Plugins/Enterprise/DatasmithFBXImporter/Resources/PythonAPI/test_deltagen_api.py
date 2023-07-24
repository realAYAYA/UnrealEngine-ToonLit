import unreal

file_a = "C:\\MyScenes\\fulltest.fbx"
file_b = "C:\\MyScenes\\fulltest.fbx"
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

dg_options = unreal.DatasmithDeltaGenImportOptions()
dg_options.set_editor_property('merge_nodes', False)
dg_options.set_editor_property('optimize_duplicated_nodes', False)
dg_options.set_editor_property('remove_invisible_nodes', False)
dg_options.set_editor_property('simplify_node_hierarchy', False)
dg_options.set_editor_property('import_var', True)
dg_options.set_editor_property('var_path', "")
dg_options.set_editor_property('import_pos', True)
dg_options.set_editor_property('pos_path', "")
dg_options.set_editor_property('import_tml', True)
dg_options.set_editor_property('tml_path', "")
dg_options.set_editor_property('textures_dir', "")
dg_options.set_editor_property('intermediate_serialization', unreal.DatasmithDeltaGenIntermediateSerializationType.DISABLED)
dg_options.set_editor_property('colorize_materials', False)
dg_options.set_editor_property('generate_lightmap_u_vs', False)
dg_options.set_editor_property('import_animations', True)

# Direct import to scene and assets:
print 'Importing directly to scene...'
unreal.DeltaGenLibrary.import_(file_a, imported_scenes_path, base_options, None, False)

#2-stage import step 1:
print 'Parsing to scene object...'
scene = unreal.DatasmithDeltaGenSceneElement.construct_datasmith_scene_from_file(file_b, imported_scenes_path, base_options, dg_options)
print 'Resulting datasmith scene: ' + str(scene)
print '\tProduct name: ' + str(scene.get_product_name())
print '\tMesh actor count: ' + str(len(scene.get_all_mesh_actors()))
print '\tLight actor count: ' + str(len(scene.get_all_light_actors()))
print '\tCamera actor count: ' + str(len(scene.get_all_camera_actors()))
print '\tCustom actor count: ' + str(len(scene.get_all_custom_actors()))
print '\tMaterial count: ' + str(len(scene.get_all_materials()))
print '\tAnimationTimeline count: ' + str(len(scene.get_all_animation_timelines()))
print '\tVariant count: ' + str(len(scene.get_all_variants()))

# Modify one of the Timelines
# Warning: The Animation nested structure is all USTRUCTs, which are value types, and the Array accessor returns 
# a copy. Meaning something like timeline[0].name = 'new_name' will set the name on the COPY of anim_nodes[0]
timelines = scene.get_all_animation_timelines()
if len(timelines) > 0:
    tim_0 = timelines[0]
    old_name = tim_0.name 
    print 'Timeline old name: ' + old_name
    
    tim_0.name += '_MODIFIED'
    modified_name = tim_0.name    
    print 'Anim node modified name: ' + modified_name
    
    timelines[0] = tim_0
    scene.set_all_animation_timelines(timelines)
    
    # Check modification
    new_timelines = scene.get_all_animation_timelines()
    print 'Anim node retrieved modified name: ' + new_timelines[0].name
    assert new_timelines[0].name == modified_name, "Node modification didn't work!"

    # Restore to previous state
    tim_0 = new_timelines[0]
    tim_0.name = old_name
    new_timelines[0] = tim_0
    scene.set_all_animation_timelines(new_timelines)
    
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
