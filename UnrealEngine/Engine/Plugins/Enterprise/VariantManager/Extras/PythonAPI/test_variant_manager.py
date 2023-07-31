# This script describes a generic use case for the variant manager

import unreal

# Create all assets and objects we'll use
lvs = unreal.VariantManagerLibrary.create_level_variant_sets_asset("LVS", "/Game/")
lvs_actor = unreal.VariantManagerLibrary.create_level_variant_sets_actor(lvs)
if lvs is None or lvs_actor is None:
    print ("Failed to spawn either the LevelVariantSets asset or the LevelVariantSetsActor!")
    quit()

var_set1 = unreal.VariantSet()
var_set1.set_display_text("My VariantSet")

var_set2 = unreal.VariantSet()
var_set2.set_display_text("VariantSet we'll delete")

var1 = unreal.Variant()
var1.set_display_text("Variant 1")

var2 = unreal.Variant()
var2.set_display_text("Variant 2")

var3 = unreal.Variant()
var3.set_display_text("Variant 3")

var4 = unreal.Variant()
var4.set_display_text("Variant 4")

var5 = unreal.Variant()
var5.set_display_text("Variant 5")

# Adds the objects to the correct parents
lvs.add_variant_set(var_set1)
var_set1.add_variant(var1)
var_set1.add_variant(var2)
var_set1.add_variant(var3)
var_set1.add_variant(var4)
var_set1.add_variant(var5)

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
var2.add_actor_binding(spawned_actor)
var3.add_actor_binding(spawned_actor)
var4.add_actor_binding(spawned_actor)
var5.add_actor_binding(spawned_actor)

capturable_props = unreal.VariantManagerLibrary.get_capturable_properties(spawned_actor)

print ("Capturable properties for actor '" + spawned_actor.get_actor_label() + "':")
for prop in capturable_props:
    print ("\t" + prop)

# Capture all available properties on Variant 1
for prop in capturable_props:
    var1.capture_property(spawned_actor, prop)

# Capture just materials on Variant 2
just_mat_props = (p for p in capturable_props if "Material[" in p)
for prop in just_mat_props:
    var2.capture_property(spawned_actor, prop)

# Capture just relative location on Variant 4
just_rel_loc = (p for p in capturable_props if "Relative Location" in p)
rel_loc_props = []
for prop in just_rel_loc:
    captured_prop = var4.capture_property(spawned_actor, prop)
    rel_loc_props.append(captured_prop)

# Store a property value on Variant 4
rel_loc_prop = rel_loc_props[0]
print (rel_loc_prop.get_full_display_string())
spawned_actor.set_actor_relative_location(unreal.Vector(100, 200, 300), False, False)
rel_loc_prop.record()

# Move the target actor to some other position
spawned_actor.set_actor_relative_location(unreal.Vector(500, 500, 500), False, False)

# Can switch on the variant, applying the recorded value of all its properties to all
# of its bound actors
var4.switch_on()  # Cube will be at 100, 200, 300 after this

# Apply the recorded value from just a single property
rel_loc_prop.apply()

# Get the relative rotation property
rel_rot = [p for p in capturable_props if "Relative Rotation" in p][0]

# Remove objects
lvs.remove_variant_set(var_set2)
lvs.remove_variant_set(var_set2)
var_set1.remove_variant(var3)
var5.remove_actor_binding(spawned_actor)
var1.remove_captured_property_by_name(spawned_actor, rel_rot)