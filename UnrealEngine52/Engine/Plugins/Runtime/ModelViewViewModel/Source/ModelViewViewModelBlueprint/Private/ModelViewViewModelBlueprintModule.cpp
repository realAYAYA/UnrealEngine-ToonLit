// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModelViewViewModelBlueprintModule.h"
#include "Modules/ModuleManager.h"

#include "ViewModel/MVVMViewModelBlueprint.h"
#include "ViewModel/MVVMViewModelBlueprintCompiler.h"


class FModelViewViewModelBlueprintModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
#if UE_MVVM_WITH_VIEWMODEL_EDITOR
		IKismetCompilerInterface& KismetCompilerModule = FModuleManager::GetModuleChecked<IKismetCompilerInterface>("KismetCompiler");
		KismetCompilerModule.GetCompilers().Add(&ViewModelBlueprintCompiler);

		FKismetCompilerContext::RegisterCompilerForBP(UMVVMViewModelBlueprint::StaticClass(), &UMVVMViewModelBlueprint::GetCompilerForViewModelBlueprint);
#endif
	}

	virtual void ShutdownModule() override
	{
#if UE_MVVM_WITH_VIEWMODEL_EDITOR
		if (IKismetCompilerInterface* KismetCompilerModule = FModuleManager::GetModulePtr<IKismetCompilerInterface>("KismetCompiler"))
		{
			KismetCompilerModule->GetCompilers().Remove(&ViewModelBlueprintCompiler);
		}
#endif
	}

private:
	UE::MVVM::FViewModelBlueprintCompiler ViewModelBlueprintCompiler;
};


IMPLEMENT_MODULE(FModelViewViewModelBlueprintModule, ModelViewViewModelBlueprint);
