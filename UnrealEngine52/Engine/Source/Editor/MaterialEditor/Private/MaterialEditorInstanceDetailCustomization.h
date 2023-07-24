// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/SlateEnums.h"
#include "Layout/Visibility.h"
#include "IDetailCustomization.h"
#include "Input/Reply.h"
#include "Customizations/ColorStructCustomization.h"


struct FAssetData;
class IDetailGroup;
class IDetailLayoutBuilder;
class IPropertyHandle;
class UMaterialEditorInstanceConstant;

DECLARE_DELEGATE_OneParam(FGetShowHiddenParameters, bool&);

/*-----------------------------------------------------------------------------
   FMaterialInstanceParameterDetails
-----------------------------------------------------------------------------*/

class FMaterialInstanceParameterDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<class IDetailCustomization> MakeInstance(UMaterialEditorInstanceConstant* MaterialInstance, FGetShowHiddenParameters InShowHiddenDelegate);
	
	/** Constructor */
	FMaterialInstanceParameterDetails(UMaterialEditorInstanceConstant* MaterialInstance, FGetShowHiddenParameters InShowHiddenDelegate);

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

	static TOptional<float> OnGetValue(TSharedRef<IPropertyHandle> PropertyHandle);
	static void OnValueCommitted(float NewValue, ETextCommit::Type CommitType, TSharedRef<IPropertyHandle> PropertyHandle);

private:
	/** Returns the function parent path */
	FString GetFunctionParentPath() const;

	/** Builds the custom parameter groups category */
	void CreateGroupsWidget(TSharedRef<IPropertyHandle> ParameterGroupsProperty, class IDetailCategoryBuilder& GroupsCategory);

	/** Builds the widget for an individual parameter group */
	void CreateSingleGroupWidget(struct FEditorParameterGroup& ParameterGroup, TSharedPtr<IPropertyHandle> ParameterGroupProperty, class IDetailGroup& DetailGroup);

	/** These methods generate the custom widgets for the various parameter types */
	void CreateParameterValueWidget(class UDEditorParameterValue* Parameter, TSharedPtr<IPropertyHandle> ParameterProperty, IDetailGroup& DetailGroup);
	void CreateMaskParameterValueWidget(class UDEditorParameterValue* Parameter, TSharedPtr<IPropertyHandle> ParameterProperty, IDetailGroup& DetailGroup);
	void CreateVectorChannelMaskParameterValueWidget(class UDEditorParameterValue* Parameter, TSharedPtr<IPropertyHandle> ParameterProperty, IDetailGroup& DetailGroup);
	void CreateScalarAtlasPositionParameterValueWidget(class UDEditorParameterValue* Parameter, TSharedPtr<IPropertyHandle> ParameterProperty, IDetailGroup& DetailGroup);
	void CreateLabeledTextureParameterValueWidget(class UDEditorParameterValue* Parameter, TSharedPtr<IPropertyHandle> ParameterProperty, IDetailGroup& DetailGroup);

	FString GetCurvePath(class UDEditorScalarParameterValue * Parameter) const;

	/** Returns true if the parameter is in the visible expressions list */
	bool IsVisibleExpression(class UDEditorParameterValue* Parameter);

	/** Returns true if the parameter should be displayed */
	EVisibility ShouldShowExpression(class UDEditorParameterValue* Parameter) const;

	/** Called to check if an asset can be set as a parent */
	bool OnShouldSetAsset(const FAssetData& InAssetData) const;

	/** Called when an asset is set as a parent */
	void OnAssetChanged(const FAssetData& InAssetData, TSharedRef<IPropertyHandle> InHandle);

	/** Returns true if the refraction options should be displayed */
	EVisibility ShouldShowMaterialRefractionSettings() const;

	/** Returns true if the refraction options should be displayed */
	EVisibility ShouldShowSubsurfaceProfile() const;

	//Functions supporting copy/paste of entire parameter groups.

	/**
	 * Copy all parameter values in a parameter group to the clipboard
	 * in the format "Param1=\"Value1\"\nParams2=\"Value2\"..."
	 */
	void OnCopyParameterValues(int32 ParameterGroupIndex);

	/** Whether it is possible to copy parameter values for the given parent group index */
	bool CanCopyParameterValues(int32 ParameterGroupIndex);

	/**
	 * Paste parameter values from the clipboard, assumed to be
	 * in the format copied by CopyParameterValues.
	 */
	void OnPasteParameterValues(int32 ParameterGroupIndex);

	/**
	 * Whether it is possible to paste parameter values onto the given group index,
	 * and if there is anything on the clipboard to paste from.
	 */
	bool CanPasteParameterValues(int32 ParameterGroupIndex);

	/** Creates all the lightmass property override widgets. */
	void CreateLightmassOverrideWidgets(IDetailLayoutBuilder& DetailLayout);

	//Functions supporting BasePropertyOverrides

	/** Creates all the base property override widgets. */
	void CreateBasePropertyOverrideWidgets(IDetailLayoutBuilder& DetailLayout, IDetailGroup& MaterialPropertyOverrideGroup);

	EVisibility IsOverriddenAndVisible(TAttribute<bool> IsOverridden) const;

	bool OverrideOpacityClipMaskValueEnabled() const;
	bool OverrideBlendModeEnabled() const;
	bool OverrideShadingModelEnabled() const;
	bool OverrideTwoSidedEnabled() const;
	bool OverrideIsThinSurfaceEnabled() const;
	bool OverrideDitheredLODTransitionEnabled() const;
	bool OverrideOutputTranslucentVelocityEnabled() const;
	bool OverrideMaxWorldPositionOffsetDisplacementEnabled() const;
	void OnOverrideOpacityClipMaskValueChanged(bool NewValue);
	void OnOverrideBlendModeChanged(bool NewValue);
	void OnOverrideShadingModelChanged(bool NewValue);
	void OnOverrideTwoSidedChanged(bool NewValue);
	void OnOverrideIsThinSurfaceChanged(bool NewValue);
	void OnOverrideDitheredLODTransitionChanged(bool NewValue);
	void OnOverrideOutputTranslucentVelocityChanged(bool NewValue);
	void OnOverrideMaxWorldPositionOffsetDisplacementChanged(bool NewValue);

private:
	/** Object that stores all of the possible parameters we can edit */
	UMaterialEditorInstanceConstant* MaterialEditorInstance;

	/** Delegate to call to determine if hidden parameters should be shown */
	FGetShowHiddenParameters ShowHiddenDelegate;

	/** Associated FMaterialInstance utilities */
	TWeakPtr<class IPropertyUtilities> PropertyUtilities;
};

