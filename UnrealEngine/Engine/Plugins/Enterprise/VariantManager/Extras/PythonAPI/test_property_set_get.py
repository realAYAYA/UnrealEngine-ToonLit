import unreal

# Create all assets and objects we'll use
lvs = unreal.VariantManagerLibrary.create_level_variant_sets_asset("LVS", "/Game/")
lvs_actor = unreal.VariantManagerLibrary.create_level_variant_sets_actor(lvs)
if lvs is None or lvs_actor is None:
    print ("Failed to spawn either the LevelVariantSets asset or the LevelVariantSetsActor!")
    quit()

# Create a variant set and add it to lvs
var_set1 = unreal.VariantSet()
var_set1.set_display_text("My VariantSet")
lvs.add_variant_set(var_set1)

# Create a variant and add it to var_set1
var1 = unreal.Variant()
var1.set_display_text("Variant 1")
var_set1.add_variant(var1)

# Create a test actor and add it to var1. The test actor has almost all possible types of capturable properties
location = unreal.Vector()
rotation = unreal.Rotator()
test_actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.VariantManagerTestActor, location, rotation)
var1.add_actor_binding(test_actor)

capturable_props = unreal.VariantManagerLibrary.get_capturable_properties(test_actor)
captured_props = []

print ("Capturable properties for actor '" + test_actor.get_actor_label() + "':")
for prop in capturable_props:
    print ("\t" + prop)

    # All test properties are named like 'Captured____Property'
    # The check here avoids capturing generic Actor properties like 'Can be Damaged'
    if str(prop).startswith('Captured') and str(prop).endswith('Property'):
        new_prop = var1.capture_property(test_actor, prop)
        captured_props.append(new_prop)

for prop in captured_props:
    type_str = prop.get_property_type_string()

    # Set a value for a property depending on its type
    if type_str == "bool":
        prop.set_value_bool(True)
    elif type_str == "int":
        prop.set_value_int(2)
    elif type_str == "float":
        prop.set_value_float(2.0)
    elif type_str == "object":
        cube = unreal.EditorAssetLibrary.load_asset("StaticMesh'/Engine/BasicShapes/Cube.Cube'")
        prop.set_value_object(cube)
    elif type_str == "strint":
        prop.set_value_string("new string")
    elif type_str == "rotator":
        prop.set_value_rotator(unreal.Rotator(11, 12, 13))
    elif type_str == "color":
        prop.set_value_color(unreal.Color(21, 22, 23, 24))
    elif type_str == "linear_color":
        prop.set_value_linear_color(unreal.LinearColor(0.31, 0.32, 0.33, 0.34))
    elif type_str == "vector":
        prop.set_value_vector(unreal.Vector(41, 42, 43))
    elif type_str == "quat":
        prop.set_value_quat(unreal.Quat(0.51, 0.52, 0.53, 0.54))
    elif type_str == "vector4":
        prop.set_value_vector4(unreal.Vector4(6.1, 6.2, 6.3, 6.4))
    elif type_str == "Vector2D":
        prop.set_value_vector2d(unreal.Vector2D(7.1, 7.2))
    elif type_str == "int_Point":
        prop.set_value_int_point(unreal.IntPoint(81, 82))

# Easier to print using getattr
for prop in captured_props:
    type_str = prop.get_property_type_string()
    print (getattr(prop, "get_value_" + type_str)())