// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/ControlRigWrapperObject.h"

#if WITH_EDITOR
#include "ControlRigElementDetails.h"
#include "ControlRigModuleDetails.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#endif

UClass* UControlRigWrapperObject::GetClassForStruct(UScriptStruct* InStruct, bool bCreateIfNeeded) const
{
	UClass* Class = Super::GetClassForStruct(InStruct, bCreateIfNeeded);
	if(Class == nullptr)
	{
		return nullptr;
	}
	
	const FName WrapperClassName = Class->GetFName();

#if WITH_EDITOR
	if(InStruct->IsChildOf(FRigBaseElement::StaticStruct()))
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		if (!PropertyEditorModule.GetClassNameToDetailLayoutNameMap().Contains(WrapperClassName))
		{
			if(InStruct == FRigBoneElement::StaticStruct())
			{
				PropertyEditorModule.RegisterCustomClassLayout(WrapperClassName, FOnGetDetailCustomizationInstance::CreateStatic(&FRigBoneElementDetails::MakeInstance));
			}
			else if(InStruct == FRigNullElement::StaticStruct())
			{
				PropertyEditorModule.RegisterCustomClassLayout(WrapperClassName, FOnGetDetailCustomizationInstance::CreateStatic(&FRigNullElementDetails::MakeInstance));
			}
			else if(InStruct == FRigControlElement::StaticStruct())
			{
				PropertyEditorModule.RegisterCustomClassLayout(WrapperClassName, FOnGetDetailCustomizationInstance::CreateStatic(&FRigControlElementDetails::MakeInstance));
			}
			else if(InStruct == FRigConnectorElement::StaticStruct())
			{
				PropertyEditorModule.RegisterCustomClassLayout(WrapperClassName, FOnGetDetailCustomizationInstance::CreateStatic(&FRigConnectorElementDetails::MakeInstance));
			}
			else if(InStruct == FRigSocketElement::StaticStruct())
			{
				PropertyEditorModule.RegisterCustomClassLayout(WrapperClassName, FOnGetDetailCustomizationInstance::CreateStatic(&FRigSocketElementDetails::MakeInstance));
			}
		}
	}
#endif

	return Class;
}
