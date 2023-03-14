# This script shows how to setup dependencies between variants when using the Variant Manager Python API

import unreal

lvs = unreal.VariantManagerLibrary.create_level_variant_sets_asset("LVS", "/Game/")
if lvs is None:
    print ("Failed to spawn either the LevelVariantSets asset or the LevelVariantSetsActor!")
    quit()

var_set_colors = unreal.VariantSet()
var_set_colors.set_display_text("Colors")

var_set_letters = unreal.VariantSet()
var_set_letters.set_display_text("Letters")

var_set_fruits = unreal.VariantSet()
var_set_fruits.set_display_text("Fruits")

var_red = unreal.Variant()
var_red.set_display_text("Red")

var_green = unreal.Variant()
var_green.set_display_text("Green")

var_blue = unreal.Variant()
var_blue.set_display_text("Blue")

var_a = unreal.Variant()
var_a.set_display_text("A")

var_b = unreal.Variant()
var_b.set_display_text("B")

var_apple = unreal.Variant()
var_apple.set_display_text("Apple")

var_orange = unreal.Variant()
var_orange.set_display_text("Orange")

# Adds the objects to the correct parents
lvs.add_variant_set(var_set_colors)
lvs.add_variant_set(var_set_letters)
lvs.add_variant_set(var_set_fruits)

var_set_colors.add_variant(var_red)
var_set_colors.add_variant(var_green)
var_set_colors.add_variant(var_blue)

var_set_letters.add_variant(var_a)
var_set_letters.add_variant(var_b)

var_set_fruits.add_variant(var_apple)
var_set_fruits.add_variant(var_orange)

# Let's make variant 'Red' also switch on variant 'A' before it is switched on
dep1 = unreal.VariantDependency()
dep1.set_editor_property('VariantSet', var_set_letters)
dep1.set_editor_property('Variant', var_a)
dep1_index = var_red.add_dependency(dep1)

# Let's also make variant 'A' also switch on variant 'Orange' before it is switched on
dep2 = unreal.VariantDependency()
dep2.set_editor_property('VariantSet', var_set_fruits)
dep2.set_editor_property('Variant', var_orange)
var_a.add_dependency(dep2)

# Because dependencies trigger first, this will switch on 'Orange', then 'A' and finally 'Red'
var_red.switch_on()

# Let's disable the first dependency
# Note we need to set the struct back into the variant, as it's returned by value
dep1.set_editor_property('bEnabled', False)
var_red.set_dependency(dep1_index[0], dep1)

# Now this will only trigger 'Red', because dep1 is disabled and doesn't propagate to variant 'A'
var_red.switch_on ( )
