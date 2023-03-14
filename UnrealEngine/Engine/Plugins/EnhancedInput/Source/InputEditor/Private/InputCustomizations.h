// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "IDetailCustomNodeBuilder.h"
#include "IPropertyTypeCustomization.h"
#include "KeyStructCustomization.h"

class IDetailLayoutBuilder;

class FInputContextDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** ILayoutDetails interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;
};

class FEnhancedActionMappingCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FEnhancedActionMappingCustomization());
	}

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	/** Searches the trigger array of an input action - returns true if any combo input triggers are present */
	bool DoesTriggerArrayContainCombo() const;
private:

	void RemoveMappingButton_OnClick() const;
	void OnTriggersChanged() const;

	TSharedPtr<IPropertyTypeCustomization> KeyStructInstance;
	TSharedPtr<IPropertyHandle> MappingPropertyHandle;
	TSharedPtr<FKeyStructCustomization> KeyStructCustomization;
};

/**
 * Customization for UEnhancedInputDeveloperSettings.
 *
 * This will just make the normal details panel for UEnhancedInputDeveloperSettings, and then add all the default settings
 * of Input Triggers and Input Modifiers by gather all the CDO's for them.
 */
class FEnhancedInputDeveloperSettingsCustomization : public IDetailCustomization
{
public:
	//~ IDetailCustomization interface
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FEnhancedInputDeveloperSettingsCustomization());
	}

	virtual ~FEnhancedInputDeveloperSettingsCustomization();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	virtual void CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder) override;
	//~ End IDetailCustomization interface
	
private:

	/** Gather all of the CDO's for the given class, Native and Blueprint. */
	static TArray<UObject*> GatherClassDetailsCDOs(UClass* Class);

	/**
	 * Called when any Asset is added, removed, or renamed.
	 *
	 * This will rebuild the Modifier and Trigger CDO views to make sure that
	 * any newly added Blueprint
	 */
	void RebuildDetailsViewForAsset(const FAssetData& AssetData, const bool bIsAssetBeingRemoved);

	/** Callbacks that are triggered from the Asset Registry. */
	void OnAssetAdded(const FAssetData& AssetData) { RebuildDetailsViewForAsset(AssetData, false); }
	void OnAssetRemoved(const FAssetData& AssetData) { RebuildDetailsViewForAsset(AssetData, true); }
	void OnAssetRenamed(const FAssetData& AssetData, const FString&) { RebuildDetailsViewForAsset(AssetData, false); }
	
	/**
	 * Create a new category on the DetailBuilder and add each object in the given array as an external reference.
	 * 
	 * @param DetailBuilder			The details builder that can be used to add categories
	 * @param CategoryName			The name of the new category to add
	 * @param ObjectsToCustomize	Array of CDO objects to customize and add as external references
	 */
	void CustomizeCDOValues(IDetailLayoutBuilder& DetailBuilder, const FName CategoryName, const TArray<UObject*>& ObjectsToCustomize);

	// Cached details builder so that we can rebuild the details when a new BP asset is added
	TWeakPtr<IDetailLayoutBuilder> CachedDetailBuilder;

	/**
	 * Populated by RebuildDetailsViewForAsset so that we can exclude any blueprint classes
	 * that have been removed from the asset registry but are still loaded in memory.
	 * This is reset when CustomizeCDOValues is called
	 */
	static TSet<FName> ExcludedAssetNames;
};