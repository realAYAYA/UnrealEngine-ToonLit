// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"
#include "RigVMHost.h"
#include "RigVMBlueprint.h"
#include "Editor/RigVMDetailsViewWrapperObject.h"
#include "EdGraph/RigVMEdGraph.h"
#include "Widgets/SRigVMGraphPinNameListValueWidget.h"
#include "Styling/SlateTypes.h"
#include "IPropertyUtilities.h"
#include "EdGraph/RigVMEdGraphSchema.h"

class IPropertyHandle;

class RIGVMEDITOR_API FRigVMLocalVariableDetailCustomization : public IDetailCustomization
{
	FRigVMLocalVariableDetailCustomization()
	: GraphBeingCustomized(nullptr)
	, BlueprintBeingCustomized(nullptr)
	, NameValidator(nullptr, nullptr, NAME_None)
	{}

	
public:

	// Makes a new instance of this detail layout class for a specific detail view requesting it
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FRigVMLocalVariableDetailCustomization);
	}

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:

	URigVMGraph* GraphBeingCustomized;
	URigVMBlueprint* BlueprintBeingCustomized;
	FRigVMGraphVariableDescription VariableDescription;
	TArray<TWeakObjectPtr<URigVMDetailsViewWrapperObject>> ObjectsBeingCustomized;

	TSharedPtr<IPropertyHandle> NameHandle;
	TSharedPtr<IPropertyHandle> TypeHandle;
	TSharedPtr<IPropertyHandle> TypeObjectHandle;
	TSharedPtr<IPropertyHandle> DefaultValueHandle;

	FRigVMLocalVariableNameValidator NameValidator;
	TArray<TSharedPtr<FString>> EnumOptions;

	FText GetName() const;
	void SetName(const FText& InNewText, ETextCommit::Type InCommitType);
	bool OnVerifyNameChanged(const FText& InText, FText& OutErrorMessage);
	
	FEdGraphPinType OnGetPinInfo() const;
	void HandlePinInfoChanged(const FEdGraphPinType& PinType);

	ECheckBoxState HandleBoolDefaultValueIsChecked( ) const;
	void OnBoolDefaultValueChanged(ECheckBoxState InCheckBoxState);
};



