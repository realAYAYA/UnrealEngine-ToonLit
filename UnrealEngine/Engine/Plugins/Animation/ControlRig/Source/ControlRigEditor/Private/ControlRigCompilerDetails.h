// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "RigVMCompiler/RigVMCompiler.h"
#include "ControlRigBlueprint.h"

class IPropertyHandle;

class FRigVMCompileSettingsDetails : public IPropertyTypeCustomization
{
public:

	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FRigVMCompileSettingsDetails);
	}

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

protected:

	FReply OnInspectMemory(ERigVMMemoryType InMemoryType);
	FReply OnCopyASTClicked();
	FReply OnCopyByteCodeClicked();
	FReply OnCopyHierarchyGraphClicked();
	FReply OnCopyGeneratedCodeClicked();
	
	UControlRigBlueprint* BlueprintBeingCustomized = nullptr;
};
