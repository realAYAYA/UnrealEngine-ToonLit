// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "SGraphPin.h"
#include "RigVMModel/RigVMPin.h"
#include "IPropertyAccessEditor.h"
#include "RigVMBlueprint.h"

DECLARE_DELEGATE_OneParam(FOnTypeSelected, TRigVMTypeIndex);

class RIGVMEDITOR_API SRigVMGraphChangePinType : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SRigVMGraphChangePinType)
	: _Types()
	, _OnTypeSelected(nullptr)
	{}

		SLATE_ARGUMENT(TArray<TRigVMTypeIndex>, Types)
		SLATE_EVENT(FOnTypeSelected, OnTypeSelected)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

protected:

	static FText GetBindingText(const FRigVMTemplateArgumentType& InType);
	FText GetBindingText(URigVMPin* ModelPin) const;
	FText GetBindingText() const;
	const FSlateBrush* GetBindingImage() const;
	FLinearColor GetBindingColor() const;
	bool OnCanBindProperty(FProperty* InProperty) const;
	bool OnCanBindToClass(UClass* InClass) const;
	void OnAddBinding(FName InPropertyName, const TArray<FBindingChainElement>& InBindingChain);
	void FillPinTypeMenu( FMenuBuilder& MenuBuilder );
	void HandlePinTypeChanged(FRigVMTemplateArgumentType InType);

	TArray<TRigVMTypeIndex> Types;
	FOnTypeSelected OnTypeSelected;
	FPropertyBindingWidgetArgs BindingArgs;
};
