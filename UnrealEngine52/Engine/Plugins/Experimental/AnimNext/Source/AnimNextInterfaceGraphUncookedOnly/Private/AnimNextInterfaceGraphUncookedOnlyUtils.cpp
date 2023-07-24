// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextInterfaceUncookedOnlyUtils.h"
#include "AnimNextInterfaceGraph.h"
#include "AnimNextInterfaceGraph_EditorData.h"
#include "AnimNextInterfaceGraph_EdGraph.h"
#include "RigVMCompiler/RigVMCompiler.h"
#include "RigVMCore/RigVM.h"
#include "AnimNextInterfaceExecuteContext.h"

namespace UE::AnimNext::InterfaceGraphUncookedOnly
{

void FUtils::Compile(UAnimNextInterfaceGraph* InGraph)
{
	check(InGraph);
	
	UAnimNextInterfaceGraph_EditorData* EditorData = GetEditorData(InGraph);
	
	if(EditorData->bIsCompiling)
	{
		return;
	}
	
	TGuardValue<bool> CompilingGuard(EditorData->bIsCompiling, true);
	
	EditorData->bErrorsDuringCompilation = false;

	EditorData->RigGraphDisplaySettings.MinMicroSeconds = EditorData->RigGraphDisplaySettings.LastMinMicroSeconds = DBL_MAX;
	EditorData->RigGraphDisplaySettings.MaxMicroSeconds = EditorData->RigGraphDisplaySettings.LastMaxMicroSeconds = (double)INDEX_NONE;
	
	TGuardValue<bool> ReentrantGuardSelf(EditorData->bSuspendModelNotificationsForSelf, true);
	TGuardValue<bool> ReentrantGuardOthers(EditorData->bSuspendModelNotificationsForOthers, true);

	RecreateVM(InGraph);

	InGraph->VMRuntimeSettings = EditorData->VMRuntimeSettings;

	EditorData->CompileLog.Messages.Reset();
	EditorData->CompileLog.NumErrors = EditorData->CompileLog.NumWarnings = 0;

	URigVMCompiler* Compiler = URigVMCompiler::StaticClass()->GetDefaultObject<URigVMCompiler>();
	EditorData->VMCompileSettings.SetExecuteContextStruct(EditorData->RigVMClient.GetExecuteContextStruct());
	Compiler->Settings = (EditorData->bCompileInDebugMode) ? FRigVMCompileSettings::Fast(EditorData->VMCompileSettings.GetExecuteContextStruct()) : EditorData->VMCompileSettings;
	URigVMController* RootController = EditorData->GetRigVMClient()->GetOrCreateController(EditorData->GetRigVMClient()->GetDefaultModel());
	Compiler->Compile(EditorData->GetRigVMClient()->GetAllModels(false, false), RootController, InGraph->RigVM, InGraph->GetRigVMExternalVariables(), &EditorData->PinToOperandMap);

	if (EditorData->bErrorsDuringCompilation)
	{
		if(Compiler->Settings.SurpressErrors)
		{
			Compiler->Settings.Reportf(EMessageSeverity::Info, InGraph,TEXT("Compilation Errors may be suppressed for AnimNext Interface Graph: %s. See VM Compile Settings for more Details"), *InGraph->GetName());
		}
	}

	EditorData->bVMRecompilationRequired = false;
	if(InGraph->RigVM)
	{
		EditorData->VMCompiledEvent.Broadcast(InGraph, InGraph->RigVM);
	}

#if WITH_EDITOR
//	RefreshBreakpoints(EditorData);
#endif
}

void FUtils::RecreateVM(UAnimNextInterfaceGraph* InGraph)
{
	InGraph->RigVM = NewObject<URigVM>(InGraph, TEXT("VM"), RF_NoFlags);
	InGraph->RigVM->SetContextPublicDataStruct(FAnimNextInterfaceExecuteContext::StaticStruct());
	
	// Cooked platforms will load these pointers from disk
	if (!FPlatformProperties::RequiresCookedData())
	{
		// We dont support ERigVMMemoryType::Work memory as we dont operate on an instance
	//	InGraph->RigVM->GetMemoryByType(ERigVMMemoryType::Work, true);
		InGraph->RigVM->GetMemoryByType(ERigVMMemoryType::Literal, true);
		InGraph->RigVM->GetMemoryByType(ERigVMMemoryType::Debug, true);
	}

	InGraph->RigVM->Reset();
}

UAnimNextInterfaceGraph_EditorData* FUtils::GetEditorData(const UAnimNextInterfaceGraph* InAnimNextInterfaceGraph)
{
	check(InAnimNextInterfaceGraph);
	
	return CastChecked<UAnimNextInterfaceGraph_EditorData>(InAnimNextInterfaceGraph->EditorData);
}

}
