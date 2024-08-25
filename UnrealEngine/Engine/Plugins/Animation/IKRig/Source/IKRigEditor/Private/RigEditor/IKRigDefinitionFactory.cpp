// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigEditor/IKRigDefinitionFactory.h"
#include "Rig/IKRigDefinition.h"
#include "AssetTypeCategories.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IKRigDefinitionFactory)

#define LOCTEXT_NAMESPACE "IKRigDefinitionFactory"


UIKRigDefinitionFactory::UIKRigDefinitionFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UIKRigDefinition::StaticClass();
}

UObject* UIKRigDefinitionFactory::FactoryCreateNew(
	UClass* Class,
	UObject* InParent,
	FName InName,
	EObjectFlags InFlags,
	UObject* Context, 
	FFeedbackContext* Warn)
{
	// create the IK Rig asset
	return NewObject<UIKRigDefinition>(InParent, InName, InFlags | RF_Transactional);
}

bool UIKRigDefinitionFactory::ShouldShowInNewMenu() const
{
	return true;
}

FText UIKRigDefinitionFactory::GetDisplayName() const
{
	return LOCTEXT("IKRigDefinition_DisplayName", "IK Rig");
}

uint32 UIKRigDefinitionFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Animation;
}

FText UIKRigDefinitionFactory::GetToolTip() const
{
	return LOCTEXT("IKRigDefinition_Tooltip", "Defines a set of IK Solvers and Effectors to pose a skeleton with Goals.");
}

FString UIKRigDefinitionFactory::GetDefaultNewAssetName() const
{
	return FString(TEXT("IK_NewIKRig"));
}
#undef LOCTEXT_NAMESPACE

