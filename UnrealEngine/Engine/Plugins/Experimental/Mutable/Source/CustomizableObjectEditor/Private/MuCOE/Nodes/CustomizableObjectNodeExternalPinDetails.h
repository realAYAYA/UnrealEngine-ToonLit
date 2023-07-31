// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "IDetailCustomization.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateEnums.h"

class FString;
class IDetailLayoutBuilder;
class UCustomizableObjectNodeExternalPin;
struct FAssetData;


class FCustomizableObjectNodeExternalPinDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** ILayoutDetails interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

private:
	IDetailLayoutBuilder* DetailBuilderPtr = nullptr;

	UCustomizableObjectNodeExternalPin* Node = nullptr;
	
	TArray<TSharedPtr<FString>> GroupNodeComboBoxOptions;

	void ParentObjectSelectionChanged(const FAssetData& AssetData);

	void OnGroupNodeComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);
};
