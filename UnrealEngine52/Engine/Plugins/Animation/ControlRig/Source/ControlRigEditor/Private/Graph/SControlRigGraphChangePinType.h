// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "SGraphPin.h"
#include "RigVMModel/RigVMPin.h"
#include "IPropertyAccessEditor.h"
#include "ControlRigBlueprint.h"

class SControlRigChangePinType : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SControlRigChangePinType)
	: _ModelPins()
    , _Blueprint(nullptr)
	{}

		SLATE_ARGUMENT(TArray<URigVMPin*>, ModelPins)
		SLATE_ARGUMENT(UControlRigBlueprint*, Blueprint)

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

	TArray<URigVMPin*> ModelPins;
	UControlRigBlueprint* Blueprint;
	FPropertyBindingWidgetArgs BindingArgs;
};

class SControlRigGraphChangePinType : public SGraphPin
{
public:

	SLATE_BEGIN_ARGS(SControlRigGraphChangePinType){}

		SLATE_ARGUMENT(TArray<URigVMPin*>, ModelPins)
		SLATE_ARGUMENT(UControlRigBlueprint*, Blueprint)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:

	//~ Begin SGraphPin Interface
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;
	//~ End SGraphPin Interface

	TArray<URigVMPin*> ModelPins;
	UControlRigBlueprint* Blueprint;
};
