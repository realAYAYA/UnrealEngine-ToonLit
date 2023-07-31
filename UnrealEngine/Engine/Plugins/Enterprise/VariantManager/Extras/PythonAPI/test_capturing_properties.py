# This scripts describes the process of capturing an managing properties with a little more
# detail than test_variant_manager.py

import unreal

# Create all assets and objects we'll use
lvs = unreal.VariantManagerLibrary.create_level_variant_sets_asset("LVS", "/Game/")
lvs_actor = unreal.VariantManagerLibrary.create_level_variant_sets_actor(lvs)
if lvs is None or lvs_actor is None:
    print ("Failed to spawn either the LevelVariantSets asset or the LevelVariantSetsActor!")
    quit()

var_set1 = unreal.VariantSet()
var_set1.set_display_text("My VariantSet")

var1 = unreal.Variant()
var1.set_display_text("Variant 1")

# Adds the objects to the correct parents
lvs.add_variant_set(var_set1)
var_set1.add_variant(var1)

# Spawn a simple cube static mesh actor
cube = unreal.EditorAssetLibrary.load_asset("StaticMesh'/Engine/BasicShapes/Cube.Cube'")
spawned_actor = None
if cube:
    location = unreal.Vector()
    rotation = unreal.Rotator()
    spawned_actor = unreal.EditorLevelLibrary.spawn_actor_from_object(cube, location, rotation)
    spawned_actor.set_actor_label("Cube Actor")
else:
    print ("Failed to find Cube asset!")

if spawned_actor is None:
    print ("Failed to spawn an actor for the Cube asset!")
    quit()

# Bind spawned_actor to all our variants
var1.add_actor_binding(spawned_actor)

# See which properties can be captured from any StaticMeshActor
capturable_by_class = unreal.VariantManagerLibrary.get_capturable_properties(unreal.StaticMeshActor.static_class())

# See which properties can be captured from our specific spawned_actor
# This will also return the properties for any custom component structure we might have setup on the actor
capturable_props = unreal.VariantManagerLibrary.get_capturable_properties(spawned_actor)

print ("Capturable properties for actor '" + spawned_actor.get_actor_label() + "':")
for prop in capturable_props:
    print ("\t" + prop)

print ("Capturable properties for its class:")
for prop in capturable_by_class:
    print ("\t" + prop)

# Returns nullptr for invalid paths. This will also show an error in the Output Log
prop1 = var1.capture_property(spawned_actor, "False property path")
assert prop1 is None

# Comparison is case insensitive: The property is named "Can be Damaged", but this still works
prop2 = var1.capture_property(spawned_actor, "cAn Be DAMAged")
assert prop2 is not None and prop2.get_full_display_string() == "Can be Damaged"

# Attempts to capture the same property more than once are ignored, and None is returned
prop2attempt2 = var1.capture_property(spawned_actor, "Can Be Damaged")
assert prop2attempt2 is None

# Check which properties have been captured for some actor in a variant
print ("Captured properties for '" + spawned_actor.get_actor_label() + "' so far:")
captured_props = var1.get_captured_properties(spawned_actor)
for captured_prop in captured_props:
    print ("\t" + captured_prop.get_full_display_string())

# Capture property in a component
prop3 = var1.capture_property(spawned_actor, "Static Mesh Component / Relative Location")
assert prop3 is not None and prop3.get_full_display_string() == "Static Mesh Component / Relative Location"

# Can't capture the component itself. This will also show an error in the Output Log
prop4 = var1.capture_property(spawned_actor, "Static Mesh Component")
assert prop4 is None

# Capture material property
prop5 = var1.capture_property(spawned_actor, "Static Mesh Component / Material[0]")
assert prop5 is not None

# Removing property captures
var1.remove_captured_property(spawned_actor, prop2)
var1.remove_captured_property_by_name(spawned_actor, "Static Mesh Component / Relative Location")

print ("Captured properties for '" + spawned_actor.get_actor_label() + "' at the end:")
captured_props = var1.get_captured_properties(spawned_actor)
for captured_prop in captured_props:
    print ("\t" + captured_prop.get_full_display_string())

# Should only have the material property left
assert len(captured_props) == 1 and captured_props[0].get_full_display_string() == "Static Mesh Component / Material[0]"