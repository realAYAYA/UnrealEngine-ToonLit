// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "IDetailCustomization.h"
#include "Math/MathFwd.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateEnums.h"

class FString;
class IDetailLayoutBuilder;
class IPropertyHandle;
struct FAssetData;


class FCustomizableObjectNodeObjectDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** ILayoutDetails interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

	void ParentObjectSelectionChanged(const FAssetData& AssetData);

private:

	class UCustomizableObjectNodeObject* Node;
	TArray< TSharedPtr<FString> > GroupNodeComboOptions;
	IDetailLayoutBuilder* DetailBuilderPtr = nullptr;

	TArray< TSharedPtr<FString> > ParentComboOptions;
	TArray< class UCustomizableObjectNodeMaterial* > ParentOptionNode;

	void OnGroupNodeComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> ParentProperty);

	// Refreshes the Details View when the states variable has been updated
	void OnStatesPropertyChanged();

	FIntPoint GetGridSize() const;
	TArray<FIntRect> GetBlocks() const;

	// Widget to create the runtime parameters view
	TSharedPtr< class SCustomizableObjectNodeObjectSatesView > StatesViewWidget;
};
