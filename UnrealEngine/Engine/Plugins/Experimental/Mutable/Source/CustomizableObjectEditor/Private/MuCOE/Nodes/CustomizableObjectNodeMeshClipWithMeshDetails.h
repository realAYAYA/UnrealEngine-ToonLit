// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "IDetailCustomization.h"
#include "Input/Reply.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateEnums.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FText;
class IDetailLayoutBuilder;
class SHorizontalBox;
class SVerticalBox;
class UCustomizableObjectNodeMeshClipWithMesh;
struct FAssetData;

// Widget that represents the array of tags of the clip with mesh node
class STagView : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(class STagView) {}
	SLATE_ARGUMENT(FString, TagValue)
	SLATE_ARGUMENT(int32, TagIndex)
	SLATE_ARGUMENT(UCustomizableObjectNodeMeshClipWithMesh*, Node)
	SLATE_ARGUMENT(IDetailLayoutBuilder*, DetailBuilderPtr)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// Delete a tag of the array
	FReply DeleteTag();

	// Callback for the text box. Modifies the content of a tag.
	void OnTextBoxTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit);

public:

	// Pointer to the builder passed by CustomizeDetails method
	IDetailLayoutBuilder* DetailBuilderPtr = nullptr;

	// Pointer to the clip with mesh node
	UCustomizableObjectNodeMeshClipWithMesh* Node;

	// Value of the tag
	FString TagValue;

	// Index of the tag inside the array of tags
	int32 TagIndex;
};


class FCustomizableObjectNodeMeshClipWithMeshDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** ILayoutDetails interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

	/** Callback when new CO is selected */
	void ClipperCustomizableObjectSelectionChanged(const FAssetData& AssetData);

private:
	/** Callback when the clipping method is selected */
	void OnClippingMethodComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);

	/** Callback when new material name is selected */
	void OnMeshClipWithMeshNodeComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);

	/** Creates the array of widgets tags */
	void CreateTagView();

	/** Initializes the text of the method combobox and the view to display */
	TSharedPtr<FString> SetInitialClippingMethod();

	/** The node for which details are being customized */
	class UCustomizableObjectNodeMeshClipWithMesh* Node;

	/** Pointer to the builder passed by CustomizeDetails method */
	IDetailLayoutBuilder* DetailBuilderPtr = nullptr;

	/** References to the names of all possible methods to select the material */
	TArray<TSharedPtr<FString>> ClippingMethods;

	/** References to the names of all possible selectable material nodes in SelectedCO */
	TArray<TSharedPtr<FString>> ArrayMaterialNodeOption;

	/** To cover all LODs from a single selection, only unique names of materials are considered */
	TArray<FString> ArrayMaterialNodeName;

	/** Selected CO in the details tab of the Node node, for which to select a material that will be clipped by Node node */
	class UCustomizableObject* SelectedCO;

	/** Selector with all the materials of the Customizable Object */
	TSharedPtr< SHorizontalBox > MaterialsSelector;

	/** ComboBox with all the materials of the Customizable Object */
	TSharedPtr< SVerticalBox > TagView;

	/** Pointer to a FString to indicate that a method must be selected */
	TSharedPtr<FString> SelectAMethod;

	/** Callback to add a tag to the arry of tags */
	FReply OnAddTagPressed();
};
