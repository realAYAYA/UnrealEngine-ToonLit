// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "Widgets/SCompoundWidget.h"

namespace ESelectInfo { enum Type : int; }
namespace ETextCommit { enum Type : int; }

class FText;
class IDetailLayoutBuilder;
class SHorizontalBox;
class SVerticalBox;
class UCustomizableObjectNodeMeshClipWithMesh;
struct FAssetData;


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

	/** Pointer to a FString to indicate that a method must be selected */
	TSharedPtr<FString> SelectAMethod;

	/** Callback to add a tag to the arry of tags */
	FReply OnAddTagPressed();
};
