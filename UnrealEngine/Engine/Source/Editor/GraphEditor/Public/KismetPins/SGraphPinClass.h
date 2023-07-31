// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetRegistry/AssetData.h"
#include "CoreMinimal.h"
#include "Framework/SlateDelegates.h"
#include "Input/Reply.h"
#include "Internationalization/Text.h"
#include "KismetPins/SGraphPinObject.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"

class SWidget;
class UClass;
class UEdGraphPin;

/////////////////////////////////////////////////////
// SGraphPinClass

class GRAPHEDITOR_API SGraphPinClass : public SGraphPinObject
{
public:
	SLATE_BEGIN_ARGS(SGraphPinClass) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);
	void SetAllowAbstractClasses(bool bAllow) { bAllowAbstractClasses = bAllow; }
protected:

	// Called when a new class was picked via the asset picker
	void OnPickedNewClass(UClass* ChosenClass);

	//~ Begin SGraphPinObject Interface
	virtual FReply OnClickUse() override;
	virtual bool AllowSelfPinWidget() const override { return false; }
	virtual TSharedRef<SWidget> GenerateAssetPicker() override;
	virtual FText GetDefaultComboText() const override;
	virtual FOnClicked GetOnUseButtonDelegate() override;
	virtual const FAssetData& GetAssetData(bool bRuntimePath) const override;
	//~ End SGraphPinObject Interface

	/** Cached AssetData without the _C */
	mutable FAssetData CachedEditorAssetData;

	/** Whether abstract classes should be filtered out in the class viewer */
	bool bAllowAbstractClasses;
};
