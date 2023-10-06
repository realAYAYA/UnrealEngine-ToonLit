// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ScopedTransaction.h"
#include "IPropertyAccessEditor.h"
#include "Features/IModularFeatures.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Styling/AppStyle.h"
#include "Widgets/SCompoundWidget.h"
#include "ChooserPropertyAccess.h"

namespace UE::ChooserEditor
{
		
// Wrapper widget for Property access widget, which can update when the target Class changes
class CHOOSEREDITOR_API SPropertyAccessChainWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPropertyAccessChainWidget)
	{}

	SLATE_ARGUMENT(IHasContextClass*, ContextClassOwner)
	SLATE_ARGUMENT(FString, TypeFilter);
	SLATE_ARGUMENT(FString, BindingColor);
	SLATE_ARGUMENT(bool, AllowFunctions);
	SLATE_EVENT(FOnAddBinding, OnAddBinding);
	SLATE_ATTRIBUTE(const FChooserPropertyBinding*, PropertyBindingValue);
            
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs);
	virtual ~SPropertyAccessChainWidget();

private:
	TSharedRef<SWidget> CreatePropertyAccessWidget();
	void UpdateWidget();
	void ContextClassChanged();
		
	FString TypeFilter;
	FString BindingColor;
	FDelegateHandle ContextClassChangedHandle();
	IHasContextClass* ContextClassOwner = nullptr;
	FOnAddBinding OnAddBinding;
	TAttribute<const FChooserPropertyBinding*> PropertyBindingValue;
	bool bAllowFunctions;
};
	

}