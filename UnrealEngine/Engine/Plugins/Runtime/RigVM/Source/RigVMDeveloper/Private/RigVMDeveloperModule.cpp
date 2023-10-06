// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RigVMDeveloper.h: Module implementation.
=============================================================================*/

#include "RigVMDeveloperModule.h"
#include "RigVMBlueprint.h"
#include "RigVMBlueprintCompiler.h"

DEFINE_LOG_CATEGORY(LogRigVMDeveloper);

class FRigVMDeveloperModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	FRigVMBlueprintCompiler RigVMBlueprintCompiler;
	static TSharedPtr<FKismetCompilerContext> GetRigVMCompiler(UBlueprint* BP, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions);
};

IMPLEMENT_MODULE(FRigVMDeveloperModule, RigVMDeveloper);

void FRigVMDeveloperModule::StartupModule()
{
	// Register blueprint compiler
	FKismetCompilerContext::RegisterCompilerForBP(URigVMBlueprint::StaticClass(), &FRigVMDeveloperModule::GetRigVMCompiler);
	IKismetCompilerInterface& KismetCompilerModule = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>("KismetCompiler");
	KismetCompilerModule.GetCompilers().Add(&RigVMBlueprintCompiler);
}

void FRigVMDeveloperModule::ShutdownModule()
{
	IKismetCompilerInterface* KismetCompilerModule = FModuleManager::GetModulePtr<IKismetCompilerInterface>("KismetCompiler");
	if (KismetCompilerModule)
	{
		KismetCompilerModule->GetCompilers().Remove(&RigVMBlueprintCompiler);
	}
}

TSharedPtr<FKismetCompilerContext> FRigVMDeveloperModule::GetRigVMCompiler(UBlueprint* BP, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions)
{
	return TSharedPtr<FKismetCompilerContext>(new FRigVMBlueprintCompilerContext(BP, InMessageLog, InCompileOptions));
}
