// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

namespace UE::DataInterfaceGraphEditor
{

class FAssetTypeActions_DataInterfaceGraph;
class FDataInterfacePropertyTypeCustomization;
class FPropertyTypeIdentifier;

class FModule : public IModuleInterface
{
private:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	TSharedPtr<UE::DataInterfaceGraphEditor::FAssetTypeActions_DataInterfaceGraph> AssetTypeActions_DataInterfaceGraph;
	TSharedPtr<UE::DataInterfaceGraphEditor::FPropertyTypeIdentifier> DataInterfacePropertyTypeIdentifier;
};

}