// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "UObject/ObjectPtr.h"
#include "UObject/WeakObjectPtr.h"

class FDetailWidgetRow;
class FReply;
class ICustomizableObjectInstanceEditor;
class IDetailCategoryBuilder;
class IDetailGroup;
class IDetailLayoutBuilder; 
class SWidget;
class UCustomizableObjectInstance;

enum class ECheckBoxState : uint8;
namespace ESelectInfo { enum Type : int; }

struct FGeometry;
struct FPointerEvent;

class FCustomizableInstanceDetails : public IDetailCustomization
{
public:
	// Makes a new instance of this detail layout class for a specific detail view requesting it.
	static TSharedRef<IDetailCustomization> MakeInstance();
	
	// ILayoutDetails interface
	/** Do not use. Add details customization in the other CustomizeDetails signature. */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override {};

	/** Customize details here. */
	virtual void CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder) override;
	
	// Own interface	
	/** Refresh the custom details. */
	void Refresh() const;

private:

	// Callback to regenerate the details when the instance has finished an update
	void InstanceUpdated(UCustomizableObjectInstance* Instance) const;

	// State Selector 
	// Generates The StateSelector Widget
	TSharedRef<SWidget> GenerateStateSelector();
	void OnStateComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);

	// Instance Profiles Functions
	TSharedRef<SWidget> GenerateInstanceProfileSelector();
	FReply CreateParameterProfileWindow();
	FReply RemoveParameterProfile();
	void OnProfileSelectedChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);

	// Function to change the parameters view
	void OnShowOnlyRuntimeSelectionChanged(ECheckBoxState InCheckboxState);
	void OnShowOnlyRelevantSelectionChanged(ECheckBoxState InCheckboxState);
	void OnUseUISectionsSelectionChanged(ECheckBoxState InCheckboxState);

	// Main parameter generation functions
	// Returns true if parameters have been hidden due to runtime type
	bool GenerateParametersView(IDetailCategoryBuilder& MainCategory);
	void RecursivelyAddParamAndChildren(const int32 ParamIndexInObject, const FString ParentName, IDetailCategoryBuilder& DetailsCategory);
	void FillChildrenMap(int32 ParamIndexInObject);

	// Function to determine if a parameter widget should be generated
	bool IsVisible(int32 ParamIndexInObject);
	bool IsMultidimensionalProjector(int32 ParamIndexInObject);

	// Main widget generation functions
	IDetailGroup* GenerateParameterSection(const int32 ParamIndexInObject, IDetailCategoryBuilder& DetailsCategory);
	void GenerateWidgetRow(FDetailWidgetRow& WidgetRow, const FString& ParamName, const int32 ParamIndexInObject);
	TSharedRef<SWidget> GenerateParameterWidget(const int32 ParamIndexInObject);

	// Int Parameters Functions
	TSharedRef<SWidget> GenerateIntWidget(const int32 ParamIndexInObject);
	void OnIntParameterComboBoxChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo, const FString ParamName);
	TSharedRef<SWidget> OnGenerateWidgetIntParameter(TSharedPtr<FString> InItem) const;
	
	// Float Parameters Functions
	TSharedRef<SWidget> GenerateFloatWidget(const int32 ParamIndexInObject);
	float GetFloatParameterValue(const FString ParamName, int32 RangeIndex) const;
	void OnFloatParameterChanged(float Value, const FString ParamName, int32 RangeIndex);
	void OnFloatParameterSliderBegin();
	void OnFloatParameterSliderEnd(float Value, const FString ParamName, int32 RangeIndex);
 
	// Texture Parameters Functions
	TSharedRef<SWidget> GenerateTextureWidget(const int32 ParamIndexInObject);
	void GenerateTextureParameterOptions();
	void OnTextureParameterComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo, const FString ParamName);
	
	// Color Parameters Functions
	TSharedRef<SWidget> GenerateColorWidget(const int32 ParamIndexInObject);
	FLinearColor GetColorParameterValue(const FString ParamName) const;
	FReply OnColorBlockMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, const FString ParamName);
	void OnSetColorFromColorPicker(FLinearColor NewColor, const FString PickerParamName) const;

	// Bool Parameters Functions
	TSharedRef<SWidget> GenerateBoolWidget(const int32 ParamIndexInObject);
	ECheckBoxState GetBoolParameterValue(const FString ParamName) const;
	void OnBoolParameterChanged(ECheckBoxState InCheckboxState, const FString ParamName);

	// Projector Parameters Functions
	TSharedRef<SWidget> GenerateSimpleProjector(const int32 ParamIndexInObject);
	TSharedRef<SWidget> GenerateMultidimensionalProjector(const int32 ParamIndexInObject);
	TSharedPtr<ICustomizableObjectInstanceEditor> GetEditorChecked() const;
	FReply OnProjectorSelectChanged(const FString ParamName, const int32 RangeIndex) const;
	FReply OnProjectorCopyTransform(const FString ParamName, const int32 RangeIndex) const;
	FReply OnProjectorPasteTransform(const FString ParamName, const int32 RangeIndex);
	FReply OnProjectorResetTransform(const FString ParamName, const int32 RangeIndex);
	FReply OnProjectorLayerAdded(const FString ParamName) const;
	FReply OnProjectorLayerRemoved(const FString ParamName, const int32 RangeIndex) const;
	void OnProjectorTextureParameterComboBoxChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo, const FString ParamName, int32 RangeIndex) const;
	TSharedRef<SWidget> MakeTextureComboEntryWidget(TSharedPtr<FString> InItem) const;
	TSharedRef<SWidget> OnGenerateWidgetProjectorParameter(TSharedPtr<FString> InItem) const;

	// Parameter Functions
	FReply OnCopyAllParameters();
	FReply OnPasteAllParameters();
	FReply OnResetAllParameters();
	void OnResetParameterButtonClicked(int32 ParameterIndex);
	void SetParameterValueToDefault(int32 ParameterIndex);

private:

	/** Pointer to the Customizable Object Instance */
	TWeakObjectPtr<UCustomizableObjectInstance> CustomInstance;

	/** Details builder pointer */
	TWeakPtr<IDetailLayoutBuilder> LayoutBuilder;

	/** Map to keep track of the generated parameter sections */
	TMap<FString, IDetailGroup*> GeneratedSections;

	/** Used to insert child params to a parent's expandable area */
	TMap<FString, IDetailGroup*> ParentsGroups;

	/*Stores all the possible profiles for the current COI*/
	TArray<TSharedPtr<FString>> ParameterProfileNames;

	/** Array with all the possible states for the states combo box */
	TArray< TSharedPtr<FString> > StateNames;

	// These arrays store the textures available for texture parameters of the model.
	// These come from the texture generators registered in the CustomizableObjectSystem
	TArray<TSharedPtr<FString>> TextureParameterValueNames;
	TArray<FName> TextureParameterValues;

	/** Weak pointer of the open editor */
	TWeakPtr<ICustomizableObjectInstanceEditor> WeakEditor;

	/** Maps param name to children param indices, used to walk the params in order respecting parent/children relationships */
	TMultiMap<FString, int32> ParamChildren;

	/** Maps param index to bool telling if it has parent, same use as previous line */
	TMap<int32, bool> ParamHasParent;

	/** Array with all the possible multilayer projector texture options */
	TArray<TSharedPtr<TArray<TSharedPtr<FString>>>> ProjectorTextureOptions;

	/** Map from ParamIndexInObject to the param's int selector options */
	TMap<int32, TSharedPtr<TArray<TSharedPtr<FString>>>> IntParameterOptions;

	/** Map from ParamIndexInObject to the projector param pose options  */
	TMap<int32, TSharedPtr<TArray<TSharedPtr<FString>>>> ProjectorParameterPoseOptions;

	/** True when a slider is being edited*/
	bool bUpdatingSlider = false;
};

