// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "HAL/PlatformMath.h"
#include "Input/Reply.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Styling/SlateTypes.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateEnums.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/SCompoundWidget.h"

class FCustomizableInstanceDetails;
class SButton;
class SExpandableArea;
class SSearchBox;
class STextBlock;
class SVerticalBox;
class UCustomizableObjectInstance;
struct FGeometry;
struct FPointerEvent;

struct FMutableParamExpandableArea
{
	TSharedPtr<SExpandableArea> ExpandableArea;
	TSharedPtr<SVerticalBox> VerticalBox;

	FMutableParamExpandableArea(TSharedPtr<SExpandableArea> &InExpandableArea, TSharedPtr<SVerticalBox> &InVerticalBox)
		: ExpandableArea(InExpandableArea), VerticalBox(InVerticalBox)
	{ }
};

class CUSTOMIZABLEOBJECTEDITOR_API SCustomizableInstanceProperties : public SCompoundWidget
{
public:
	friend class SCreateProfileParameters;
	
	SLATE_BEGIN_ARGS(SCustomizableInstanceProperties) {}
	SLATE_ARGUMENT(TWeakObjectPtr<UCustomizableObjectInstance>, CustomInstance)
	SLATE_ARGUMENT(TWeakPtr<FCustomizableInstanceDetails>, InstanceDetails)
	SLATE_END_ARGS()
	
	/**
	 * Construct the widget
	 *
	 * @param	InArgs			A declaration from which to construct the widget
	 */
	void Construct(const FArguments& InArgs);

	// SWidget interface
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

	/** */
	const UCustomizableObjectInstance* GetInstance() const;
	void SetNoInstanceMessage( const FText& Message );

	/** Set SimplifiedProjectorInterface bool value - Deprecated */
	void SetSimplifiedProjectorInterface( bool Enabled );

	/** Redraws the Instance Properties Widget */
	void ResetParamBox();

	/** Bool to Redraw the Instance Properties Widget */
	bool bShouldResetParamBox = true;

private:
	/** Bool that decides the type of interface of projectors - Deprecated */
	bool SimplifiedProjectorInterface;

	/** Message to to indicate that there is no instance and the CO needs to be compiled*/
	FText NoInstanceMessage;

	/** Pointer to the Instance Details. Owner of this widget. */
	TWeakPtr<FCustomizableInstanceDetails> InstanceDetails;

	/** Pointer to the Customizable Object Instance */
	TWeakObjectPtr<UCustomizableObjectInstance> CustomInstance;

	/** Array with all the possible states for the states combo box */
	TArray< TSharedPtr<FString> > StateNames;
	
	/** Pointer to the widget that stores all the parameter */
	TSharedPtr<SVerticalBox> ParamBox;

	/** Array to store the option names of integers */
	//TArray< TSharedPtr< TArray< TSharedPtr<FString> > > > IntOptionNames;
	
	/** Array to store dynamic brushes */
	TArray< TSharedPtr<class FDeferredCleanupSlateBrush> > DynamicBrushes;
	
	/** Used to insert child params to a parent's expandable area */
	TMap<FString, FMutableParamExpandableArea> ParamNameToExpandableAreaMap;
	
	/** Maps param name to children param indices, used to walk the params in order respecting parent/children relationships */
	TMultiMap<FString, int32> ParamChildren;
	
	/** Maps param index to bool telling if it has parent, same use as previous line */
	TMap<int32, bool> ParamHasParent; 

	//int32 NumParams = 0; // Same use as previous line

	// These arrays store the textures available for texture parameters of the model.
	// These come from the texture generators registered in the CustomizableObjectSystem
	TArray< TSharedPtr<FString> > TextureParameterValueNames;
	TArray< int64_t > TextureParameterValues;

	struct FSliderData
	{
		FSliderData( TSharedPtr<SSpinBox<float>> InSlider, const FString& InParamName, int32 InRangeIndex, float InLastValueSet)
		{
			Slider = InSlider;
			ParameterName = InParamName;
			RangeIndex = InRangeIndex;

			LastValueSet = InLastValueSet;
		}

		TSharedPtr<SSpinBox<float>> Slider;
		FString ParameterName;
		int32 RangeIndex;
		float LastValueSet;
	};
	TArray< FSliderData > FloatSliders;

	// Get the last value set on a slider
	float GetSliderValue( int Slider ) const;

	/** Adds a widget for each the parameter */
	void AddParameter(int32 ParamIndex);

	/** Fills the children map with each parameter name and its index of the children objects */
	void FillChildrenMap(int32 ParamIndex);

	/** Fills recursively the paramchildren and adds the parameter to create its widget */
	void RecursivelyAddParamAndChildren(int32 ParamIndexInObject);

	/** CheckBox OnChecked callbacks - Determine the parameters that are shown */
	void OnShowOnlyRuntimeSelectionChanged(ECheckBoxState InCheckboxState);
	void OnShowOnlyRelevantSelectionChanged(ECheckBoxState InCheckboxState);

	/** ComboBox OnSelectionChanged callback - Change the State of the instance */
	void OnStateComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);

	/** Copies the preview instance parameters */
	FReply OnCopyAllParameters();

	/** Pastes the preview instance parameters */
	FReply OnPasteAllParameters();

	/** Clears the preview instance parameters */
	FReply OnResetAllParameters();

	/** Buffer to store the copied parameters */
	class FBufferArchive *ToBinary;

	/** Check box callbacks for bool parameters */
	void OnBoolParameterChanged(ECheckBoxState InCheckboxState, FString ParamName);
	ECheckBoxState GetBoolParameterValue(FString ParamName) const;

	/** Expandable Area OnAreaExpansionChanged callback */
	void OnAreaExpansionChanged(bool bExpanded, FString ParamName) const;

	/** Slider callbacks for float parameters */
	float GetFloatParameterValue(FString ParamName, int RangeIndex) const;
	void OnFloatParameterChanged(float Value, int SliderIndex);
	void OnFloatParameterSliderBegin();
	void OnFloatParameterSliderEnd(float Value);

	/** Spinbox callback for int parameters */
	void OnIntParameterChanged(int32 Value, FString ParamName); // Not implemented Yet

	/** SearchBox OnTextCommitted Callback to search a parameter int by name */
	void OnIntParameterComboBoxChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo, FString ParamName);

	/** Gets the name value of a int parameter */
	FString GetIntParameterValue(FString ParamName, int RangeIndex = -1) const;

	/** CheckBox IsChecked Callbacks for projectors management */
	ECheckBoxState IsProjectorHidden(FString ParamNameWithIndex) const;
	ECheckBoxState IsProjectorTranslate(FString ParamNameWithIndex) const;
	ECheckBoxState IsProjectorRotate(FString ParamNameWithIndex) const;
	ECheckBoxState IsProjectorScale(FString ParamNameWithIndex) const;

	/** CheckBox OnCheckStateChanged Callbacks for projectors management */
	void OnProjectorTranslateStateChanged( ECheckBoxState InCheckboxState, FString ParamNameWithIndex) const;
	void OnProjectorRotateStateChanged( ECheckBoxState InCheckboxState, FString ParamNameWithIndex) const;
	void OnProjectorScaleStateChanged( ECheckBoxState InCheckboxState, FString ParamNameWithIndex) const;
	
	/** Button OnClicked Callback to select or unselect a projector */
	FReply OnProjectorSelectStateChanged(FString ParamNameWithIndex);
	
	/** SearchBox OnTextCommitted Callback to search a parameter texture by name */
	void OnProjectorTextureParameterComboBoxChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo, FString ParamNameWithIndex) const;

	/** Slider OnValueChanged Callbacks for float parameters */
	void OnProjectorFloatParameterChanged(float Value, int SliderIndex, FString ParamNameWithIndex);
	void OnProjectorFloatParameterSliderBegin();
	void OnProjectorFloatParameterSliderEnd(float Value);

	/** OnClicked Button Callbacks - Manage Projector layers */
	FReply OnProjectorLayerAdded(FString ParamName) const;
	FReply OnProjectorLayerRemoved(FString ParamNameWithIndex) const;

	/** OnClicked Button Callbacks - Manage Projectors Transform */
    FReply OnProjectorCopyTransform(FString ParamName) const;
    FReply OnProjectorPasteTransform(FString ParamName) const;
    FReply OnProjectorResetTransform(FString ParamName) const;

	/** Gets the value of a vector parameter */
	FLinearColor GetColorParameterValue(FString ParamName) const;

	/** Color Block OnMouseButtonDown Callback */
	FReply OnColorBlockMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, FString ParamName);

	/** Color Picker OnColorCommitted Callback - Changes the value of a vector parameter */
	void OnSetColorFromColorPicker(FLinearColor NewColor, FString PickerParamName) const;

	/** ComboBox OnSelectionChanged Callback - Changes the texture of a projector */
	void OnTextureParameterComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo, FString ParamName);

	/** Array with the names of the current projector parameter names with their multidimensional (range) index */
	TArray<FString> ArrayProjectorParameterNameWithIndex;

	bool bSlidersChanged = false;

	// Map where to store the instances of the select / unselect projector SButton
	TMap<FString, TSharedPtr<SButton>> MapProjectorButton;
	TMap<FString, TSharedPtr<STextBlock>> MapProjectorButtonTextBlock;


	/** Called when the filter text changes.  This filters specific property nodes out of view */
	void OnFilterTextChanged(const FText& InFilterText);

	/*Search box that filters the Customizable Instance Properties*/
	TSharedPtr<SSearchBox> SB_Properties;

/*Profiles*/

	/*Open the window to create a new profile*/
	FReply CreateParameterProfileWindow();

	/*Remove selected profile*/
	FReply RemoveParameterProfile();

	/*Stores the profile that has been selected*/
	void OnProfileSelectedChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);

	/*Set all the profile parameters to the current COI*/
	void SetParameterProfileNamesOnEditor();

	/*Stores all the possible profiles for the current COI*/
	TArray<TSharedPtr<FString>> ParameterProfileNames;

	/*Check if there is the COI has any available parameter*/
	bool HasAnyParameters() const;

	void InstanceUpdated(UCustomizableObjectInstance* Instance) const;
	
	/*Check if SaveDescriptor has been called before LoadDescriptor*/
	bool ParametersHaveBeenRead;
};