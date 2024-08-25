// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextRigVMAsset.h"
#include "RigVMRuntimeDataRegistry.h"
#include "Graph/RigUnit_AnimNextBeginExecution.h"
#include "UObject/AssetRegistryTagsContext.h"

UAnimNextRigVMAsset::UAnimNextRigVMAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetRigVMExtendedExecuteContext(&ExtendedExecuteContext);
}

void UAnimNextRigVMAsset::BeginDestroy()
{
	Super::BeginDestroy();

	if (VM)
	{
		UE::AnimNext::FRigVMRuntimeDataRegistry::ReleaseAllVMRuntimeData(VM);
	}
}

void UAnimNextRigVMAsset::PostLoad()
{
	Super::PostLoad();

	ExtendedExecuteContext.InvalidateCachedMemory();

	VM = RigVM;

	// In packaged builds, initialize the VM
	// In editor, the VM will be recompiled and initialized at UAnimNextRigVMAssetEditorData::HandlePackageDone::RecompileVM
#if !WITH_EDITOR
	if(VM != nullptr)
	{
		VM->ClearExternalVariables(ExtendedExecuteContext);
		VM->Initialize(ExtendedExecuteContext);
		InitializeVM(FRigUnit_AnimNextBeginExecution::EventName);
	}
#endif
}

void UAnimNextRigVMAsset::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);

#if WITH_EDITORONLY_DATA
	if(EditorData)
	{
		EditorData->GetAssetRegistryTags(Context);
	}
#endif
}
