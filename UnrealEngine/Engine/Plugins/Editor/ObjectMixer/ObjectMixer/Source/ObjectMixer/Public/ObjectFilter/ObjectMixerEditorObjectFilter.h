// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/LightComponent.h"
#include "UObject/Class.h"

#include "ObjectMixerEditorObjectFilter.generated.h"

UENUM(BlueprintType)
enum class EObjectMixerInheritanceInclusionOptions : uint8
{
	// Get only the properties in the specified classes without considering parent or child classes
	None,
	// Get properties from the class that the specified classes immediately derive from, but not the parents' parents + Specified Classes
	IncludeOnlyImmediateParent,
	// Get properties from child classes but not child classes of child classes + Specified Classes
	IncludeOnlyImmediateChildren,
	// IncludeOnlyImmediateParent + IncludeOnlyImmediateChildren + Specified Classes
	IncludeOnlyImmediateParentAndChildren,
	// Go up the chain of parent classes to get all properties in the specified classes' ancestries + Specified Classes
	IncludeAllParents,
	// Get properties from all derived classes recursively + Specified Classes
	IncludeAllChildren,
	// IncludeAllParents + IncludeAllChildren + Specified Classes
	IncludeAllParentsAndChildren,
	// IncludeAllParents + IncludeOnlyImmediateChildren + Specified Classes
	IncludeAllParentsAndOnlyImmediateChildren, 
	// IncludeOnlyImmediateParent + IncludeAllChildren + Specified Classes
	IncludeOnlyImmediateParentAndAllChildren 
};

UENUM(BlueprintType)
enum class EObjectMixerTreeViewMode : uint8
{
	// Show all matching objects without folders
	NoFolders,
	// Display objects in a hierarchy with folders
	Folders 
};

/**
 * Native class for filtering object types to Object Mixer.
 * Native C++ classes should inherit directly from this class.
 */
UCLASS(Abstract, BlueprintType) 
class OBJECTMIXEREDITOR_API UObjectMixerObjectFilter : public UObject
{
	GENERATED_BODY()
	
public:

	UObjectMixerObjectFilter() = default;
	
	/**
	 * Return the basic object types you want to filter for in your level.
	 * For example, if you want to work with Lights, return ULightComponentBase.
	 * If you also want to see the properties for parent or child classes,
	 * override the GetObjectMixerPropertyInheritanceInclusionOptions and GetForceAddedColumns functions.
	 */
	virtual TSet<UClass*> GetObjectClassesToFilter() const { return {}; }

	/**
	 * Return the basic actor types you want to be able to place using the Add button.
	 * Note that only subclasses of AActor are supported and only those which have a registered factory.
	 * This includes most engine actor types.
	 */
	virtual TSet<TSubclassOf<AActor>> GetObjectClassesToPlace() const { return {}; }

	/**
	 * Get the text to display for the object name/label.
	 * This is useful if one of your classes is a component type and you want the label of the component's owning actor, for example.
	 * If not overridden, this returns the actor's label if it's an actor and the object's name for all other objects.
	 * @param bIsHybridRow If true, this row represents a single matching component condensed into a single row with its actor parent.
	 */
	virtual FText GetRowDisplayName(UObject* InObject, const bool bIsHybridRow = false) const;

	/**
	 * Get the text to display for the row's tooltip when hovering over it.
	 * @param bIsHybridRow If true, this row represents a single matching component condensed into a single row with its actor parent. (unused)
	 */
	virtual FText GetRowTooltipText(UObject* InObject, const bool bIsHybridRow = false) const;

	/**
	 * Controls how to display the row's visibility icon. Return true if the object should be visible.
	 * Generally this should work like the Scene Outliner does.
	 * If not overridden, we use the Editor Visibility of the object's AActor outer (unless it's an actor itself).
	 */
	virtual bool GetRowEditorVisibility(UObject* InObject) const;

	/**
	 * Controls what happens when the row's visibility icon is clicked.
	 * Generally this should work like the Scene Outliner does.
	 * If not overridden, we set the Editor Visibility of the object's AActor outer (unless it's an actor itself).
	 */
	virtual void OnSetRowEditorVisibility(UObject* InObject, bool bNewIsVisible) const;

	/**
	 * Specify a list of property names corresponding to columns you want to show by default.
	 * For example, you can specify "Intensity" and "LightColor" to show only those property columns by default in the UI.
	 * Columns not specified will not be shown by default but can be enabled by the user in the UI.
	 */
	virtual TSet<FName> GetColumnsToShowByDefault() const;

	/**
	 * Specify a list of property names corresponding to columns you don't want to ever show.
	 * For example, you can specify "Intensity" and "LightColor" to ensure that they can't be enabled or shown in the UI.
	 * Columns not specified can be enabled by the user in the UI.
	 */
	virtual TSet<FName> GetColumnsToExclude() const;

	/**
	 * Specify a list of property names found in parent classes you want to show that aren't in the specified classes.
	 * Note that properties specified here do not override the properties specified in GetColumnsToExclude(),
	 * but do override the supported property tests so these will appear even if ShouldIncludeUnsupportedProperties returns false.
	 * For example, a ULightComponent displays "LightColor" in the editor's details panel,
	 * but ULightComponent itself doesn't have a property named "LightColor". Instead it's in its parent class, ULightComponentBase.
	 * In this scenario, ULightComponent is specified and PropertyInheritanceInclusionOptions is None, so "LightColor" won't appear by default.
	 * Specify "LightColor" in this function to ensure that "LightColor" will appear as a column as long as
	 * the property is accessible to one of the specified classes regardless of which parent class it comes from.
	 */
	virtual TSet<FName> GetForceAddedColumns() const { return {}; }

	/**
	 * Specify whether we should return only the properties of the specified classes or the properties of parent and child classes.
	 * Defaults to 'None' which only considers the properties of the specified classes.
	 * If you're not seeing all the properties you expected, try overloading this function.
	 */
	virtual EObjectMixerInheritanceInclusionOptions GetObjectMixerPropertyInheritanceInclusionOptions() const;

	/**
	 * Specify whether we should return only the specified classes or the parent and child classes in placement mode.
	 * Defaults to 'None' which only considers the specified classes.
	 */
	virtual EObjectMixerInheritanceInclusionOptions GetObjectMixerPlacementClassInclusionOptions() const;

	/**
	 * If true, properties that are not visible in the details panel and properties not supported by SSingleProperty will be selectable.
	 * Defaults to false.
	 */
	virtual bool ShouldIncludeUnsupportedProperties() const;

	static TSet<UClass*> GetParentAndChildClassesFromSpecifiedClasses(
		const TSet<UClass*>& InSpecifiedClasses, EObjectMixerInheritanceInclusionOptions Options);

	static TSet<UClass*> GetParentAndChildClassesFromSpecifiedClasses(
		const TSet<TSubclassOf<AActor>>& InSpecifiedClasses, EObjectMixerInheritanceInclusionOptions Options);

	static EFieldIterationFlags GetDesiredFieldIterationFlags(const bool bIncludeInheritedProperties);

protected:

	/**
	 * Given a set of property names you wish to include, returns a list of all other properties on InObject not found in ExcludeList.
	 * Useful when defining default visible columns in a list view.
	 */
	TSet<FName> GenerateIncludeListFromExcludeList(const TSet<FName>& ExcludeList) const;
};

/**
 * Script class for filtering object types to Object Mixer.
 * Blueprint classes should inherit directly from this class.
 */
UCLASS(Abstract, Blueprintable)
class OBJECTMIXEREDITOR_API UObjectMixerBlueprintObjectFilter : public UObjectMixerObjectFilter
{
	GENERATED_BODY()
public:
	
	/**
	 * Return the basic object types you want to filter for in your level.
	 * For example, if you want to work with Lights, return ULightComponentBase.
	 * If you also want to see the properties for parent or child classes,
	 * override the GetObjectMixerPropertyInheritanceInclusionOptions and GetForceAddedColumns functions.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Object Mixer")
	TSet<UClass*> GetObjectClassesToFilter() const override;

	TSet<UClass*> GetObjectClassesToFilter_Implementation() const
	{
		return Super::GetObjectClassesToFilter();
	}

	/**
	 * Return the basic actor types you want to be able to place using the Add button.
	 * Note that only subclasses of AActor are supported and only those which have a registered factory.
	 * This includes most engine actor types.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Object Mixer")
	TSet<TSubclassOf<AActor>> GetObjectClassesToPlace() const override;

	TSet<TSubclassOf<AActor>> GetObjectClassesToPlace_Implementation() const
	{
		return Super::GetObjectClassesToPlace();
	}

	/**
	 * Get the text to display for the object name/label.
	 * This is useful if one of your classes is a component type and you want the label of the component's owning actor, for example.
	 * If not overridden, this returns the actor's label if it's an actor and the object's name for all other objects.
	 * @param bIsHybridRow If true, this row represents a single matching component condensed into a single row with its actor parent.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Object Mixer")
	FText GetRowDisplayName(UObject* InObject, const bool bIsHybridRow) const override;
	
	FText GetRowDisplayName_Implementation(UObject* InObject, const bool bIsHybridRow) const
	{
		return Super::GetRowDisplayName(InObject, bIsHybridRow);
	}

	/**
	 * Get the text to display for the row's tooltip when hovering over it.
	 * @param bIsHybridRow If true, this row represents a single matching component condensed into a single row with its actor parent. (unused)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Object Mixer")
	FText GetRowTooltipText(UObject* InObject, const bool bIsHybridRow) const override;
	
	FText GetRowTooltipText_Implementation(UObject* InObject, const bool bIsHybridRow) const
	{
		return Super::GetRowTooltipText(InObject, bIsHybridRow);
	}
	
	/**
	 * Controls how to display the row's visibility icon. Return true if the object should be visible.
	 * Generally this should work like the Scene Outliner does.
	 * If not overridden, we use the Editor Visibility of the object's AActor outer (unless it's an actor itself).
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Object Mixer")
	bool GetRowEditorVisibility(UObject* InObject) const override;

	bool GetRowEditorVisibility_Implementation(UObject* InObject) const
	{
		return Super::GetRowEditorVisibility(InObject);
	}
	
	/**
	 * Controls what happens when the row's visibility icon is clicked.
	 * Generally this should work like the Scene Outliner does.
	 * If not overridden, we set the Editor Visibility of the object's AActor outer (unless it's an actor itself).
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Object Mixer")
	void OnSetRowEditorVisibility(UObject* InObject, bool bNewIsVisible) const override;

	void OnSetRowEditorVisibility_Implementation(UObject* InObject, bool bNewIsVisible) const
	{
		return Super::OnSetRowEditorVisibility(InObject, bNewIsVisible);
	}
	
	/**
	 * Specify a list of property names corresponding to columns you want to show by default.
	 * For example, you can specify "Intensity" and "LightColor" to show only those property columns by default in the UI.
	 * Columns not specified will not be shown by default but can be enabled by the user in the UI.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Object Mixer")
	TSet<FName> GetColumnsToShowByDefault() const override;
	
	TSet<FName> GetColumnsToShowByDefault_Implementation() const
	{
		return Super::GetColumnsToShowByDefault();
	}
	
	/**
	 * Specify a list of property names corresponding to columns you don't want to ever show.
	 * For example, you can specify "Intensity" and "LightColor" to ensure that they can't be enabled or shown in the UI.
	 * Columns not specified can be enabled by the user in the UI.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Object Mixer")
	TSet<FName> GetColumnsToExclude() const override;

	TSet<FName> GetColumnsToExclude_Implementation() const
	{
		return Super::GetColumnsToExclude();
	}

	/**
	 * Specify a list of property names found in parent classes you want to show that aren't in the specified classes.
	 * Note that properties specified here do not override the properties specified in GetColumnsToExclude().
	 * For example, a ULightComponent displays "LightColor" in the editor's details panel,
	 * but ULightComponent itself doesn't have a property named "LightColor". Instead it's in its parent class, ULightComponentBase.
	 * In this scenario, ULightComponent is specified and PropertyInheritanceInclusionOptions is None, so "LightColor" won't appear by default.
	 * Specify "LightColor" in this function to ensure that "LightColor" will appear as a column as long as
	 * the property is accessible to one of the specified classes regardless of which parent class it comes from.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Object Mixer")
	TSet<FName> GetForceAddedColumns() const override;
	
	TSet<FName> GetForceAddedColumns_Implementation() const
	{
		return Super::GetForceAddedColumns();
	}
	
	/**
	 * Specify whether we should return only the properties of the specified classes or the properties of parent and child classes.
	 * Defaults to 'None' which only considers the properties of the specified classes.
	 * If you're not seeing all the properties you expected, try overloading this function.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Object Mixer")
	EObjectMixerInheritanceInclusionOptions GetObjectMixerPropertyInheritanceInclusionOptions() const override;

	EObjectMixerInheritanceInclusionOptions GetObjectMixerPropertyInheritanceInclusionOptions_Implementation() const
	{
		return Super::GetObjectMixerPropertyInheritanceInclusionOptions();
	}

	/**
	 * Specify whether we should return only the specified classes or the parent and child classes in placement mode.
	 * Defaults to 'None' which only considers the specified classes.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Object Mixer")
	EObjectMixerInheritanceInclusionOptions GetObjectMixerPlacementClassInclusionOptions() const override;

	EObjectMixerInheritanceInclusionOptions GetObjectMixerPlacementClassInclusionOptions_Implementation() const
	{
		return Super::GetObjectMixerPlacementClassInclusionOptions();
	}

	/**
	 * If true, properties that are not visible in the details panel and properties not supported by SSingleProperty will be selectable.
	 * Defaults to false.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Object Mixer")
	bool ShouldIncludeUnsupportedProperties() const override;

	bool ShouldIncludeUnsupportedProperties_Implementation() const
	{
		return Super::ShouldIncludeUnsupportedProperties();
	}

};
