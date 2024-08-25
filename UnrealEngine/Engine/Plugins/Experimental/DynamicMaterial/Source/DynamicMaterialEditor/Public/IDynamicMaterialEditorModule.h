// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "Delegates/Delegate.h"
#include "Modules/ModuleManager.h"

class UClass;
class UObject;
struct FDMObjectMaterialProperty;

DECLARE_DELEGATE_RetVal_OneParam(TArray<FDMObjectMaterialProperty>, FDMGetObjectMaterialPropertiesDelegate, UObject* InObject)

/** Material Designer - Build your own materials in a slimline editor! */
class IDynamicMaterialEditorModule : public IModuleInterface
{
protected:
	static constexpr const TCHAR* ModuleName = TEXT("DynamicMaterialEditor");

public:
	static bool IsLoaded()
	{
		return FModuleManager::Get().IsModuleLoaded(ModuleName);
	}

	static IDynamicMaterialEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IDynamicMaterialEditorModule>(ModuleName);
	}

	virtual void OpenEditor(UWorld* InWorld) = 0;

	virtual void RegisterCustomMaterialPropertyGenerator(UClass* InClass, FDMGetObjectMaterialPropertiesDelegate InGenerator) = 0;
};
