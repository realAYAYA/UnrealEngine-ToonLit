// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"
#include "ControlRig.h"
#include "ControlRigBlueprint.h"
#include "Editor/DetailsViewWrapperObject.h"
#include "Graph/ControlRigGraph.h"
#include "Graph/SControlRigGraphPinNameListValueWidget.h"
#include "Styling/SlateTypes.h"
#include "IPropertyUtilities.h"
#include "Graph/ControlRigGraphSchema.h"

class IPropertyHandle;

class FRigVMLocalVariableDetails : public IDetailCustomization
{
	FRigVMLocalVariableDetails()
	: GraphBeingCustomized(nullptr)
	, BlueprintBeingCustomized(nullptr)
	, NameValidator(nullptr, nullptr, NAME_None)
	{}

	
public:

	// Makes a new instance of this detail layout class for a specific detail view requesting it
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FRigVMLocalVariableDetails);
	}

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:

	URigVMGraph* GraphBeingCustomized;
	UControlRigBlueprint* BlueprintBeingCustomized;
	FRigVMGraphVariableDescription VariableDescription;
	TArray<TWeakObjectPtr<UDetailsViewWrapperObject>> ObjectsBeingCustomized;

	TSharedPtr<IPropertyHandle> NameHandle;
	TSharedPtr<IPropertyHandle> TypeHandle;
	TSharedPtr<IPropertyHandle> TypeObjectHandle;
	TSharedPtr<IPropertyHandle> DefaultValueHandle;

	FControlRigLocalVariableNameValidator NameValidator;
	TArray<TSharedPtr<FString>> EnumOptions;

	FText GetName() const;
	void SetName(const FText& InNewText, ETextCommit::Type InCommitType);
	bool OnVerifyNameChanged(const FText& InText, FText& OutErrorMessage);
	
	FEdGraphPinType OnGetPinInfo() const;
	void HandlePinInfoChanged(const FEdGraphPinType& PinType);

	ECheckBoxState HandleBoolDefaultValueIsChecked( ) const;
	void OnBoolDefaultValueChanged(ECheckBoxState InCheckBoxState);
};



