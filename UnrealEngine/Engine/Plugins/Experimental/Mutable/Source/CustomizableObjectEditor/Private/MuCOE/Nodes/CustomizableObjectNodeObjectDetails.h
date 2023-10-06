// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"

namespace ESelectInfo { enum Type : int; }

class FString;
class IDetailLayoutBuilder;
class IPropertyHandle;
class SWidget;
struct FAssetData;
struct FPropertyChangedEvent;


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

	// Component and LOD combobox generation callbacks.
	TSharedRef<SWidget> OnGenerateComponentComboBoxForPicker();
	TSharedRef<SWidget> OnGenerateLODComboBoxForPicker();

	// Component and LOD menu generation callbacks
	TSharedRef<SWidget> OnGenerateComponentMenuForPicker();
	TSharedRef<SWidget> OnGenerateLODMenuForPicker();

	// Component and LOD OnSelect callbacks
	void OnSelectedComponentChanged(int32 NewComponentIndex);
	void OnSelectedLODChanged(int32 NewLODIndex);

	// Component and LOD name generation callbacks
	FText GetCurrentComponentName() const;
	FText GetCurrentLODName() const;

	// Calback to force the refresh of the detail if the number of components or LODs changes
	void OnNumComponentsOrLODsChanged(const FPropertyChangedEvent& PropertyChangedEvent);

	// Refreshes the Details View when the states variable has been updated
	void OnStatesPropertyChanged();

	FIntPoint GetGridSize() const;
	TArray<FIntRect> GetBlocks() const;
};
