// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModelViewViewModelBlueprintModule.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#include "KismetCompilerModule.h"
#include "ViewModel/MVVMViewModelBlueprint.h"
#include "ViewModel/MVVMViewModelBlueprintCompiler.h"


class FModelViewViewModelBlueprintModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		IKismetCompilerInterface& KismetCompilerModule = FModuleManager::GetModuleChecked<IKismetCompilerInterface>("KismetCompiler");
		KismetCompilerModule.GetCompilers().Add(&ViewModelBlueprintCompiler);

		FKismetCompilerContext::RegisterCompilerForBP(UMVVMViewModelBlueprint::StaticClass(), &UMVVMViewModelBlueprint::GetCompilerForViewModelBlueprint);
	}

	virtual void ShutdownModule() override
	{
		if (IKismetCompilerInterface* KismetCompilerModule = FModuleManager::GetModulePtr<IKismetCompilerInterface>("KismetCompiler"))
		{
			KismetCompilerModule->GetCompilers().Remove(&ViewModelBlueprintCompiler);
		}
	}

private:
	UE::MVVM::FViewModelBlueprintCompiler ViewModelBlueprintCompiler;
};


IMPLEMENT_MODULE(FModelViewViewModelBlueprintModule, ModelViewViewModelBlueprint);
