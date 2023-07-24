// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

namespace UE::AnimNext::InterfaceGraphEditor
{

class FAssetTypeActions_AnimNextInterfaceGraph;
class FAnimNextInterfacePropertyTypeCustomization;
class FPropertyTypeIdentifier;

class FModule : public IModuleInterface
{
private:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	TSharedPtr<UE::AnimNext::InterfaceGraphEditor::FAssetTypeActions_AnimNextInterfaceGraph> AssetTypeActions_AnimNextInterfaceGraph;
	TSharedPtr<UE::AnimNext::InterfaceGraphEditor::FPropertyTypeIdentifier> AnimNextInterfacePropertyTypeIdentifier;
};

}