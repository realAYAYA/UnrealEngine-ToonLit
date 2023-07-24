// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextInterfaceGraph.h"
#include "RigVMCore/RigVMMemoryStorage.h"
#include "RigUnit_AnimNextInterfaceBeginExecution.h"
#include "AnimNextInterfaceUnitContext.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNextInterfaceGraph)

namespace UE::AnimNext::InterfaceGraph
{
const FName EntryPointName("GetData");
const FName ResultName("Result");
}

FName UAnimNextInterfaceGraph::GetReturnTypeNameImpl() const
{
	return ReturnTypeName;
}

const UScriptStruct* UAnimNextInterfaceGraph::GetReturnTypeStructImpl() const
{
	return ReturnTypeStruct;
}

bool UAnimNextInterfaceGraph::GetDataImpl(const UE::AnimNext::Interface::FContext& Context) const
{
	bool bResult = true;
	
	if(RigVM)
	{
		FAnimNextInterfaceExecuteContext& AnimNextInterfaceContext = RigVM->GetContext().GetPublicDataSafe<FAnimNextInterfaceExecuteContext>();

		AnimNextInterfaceContext.SetContextData(this, Context, bResult);

		bResult &= (RigVM->Execute(TArray<URigVMMemoryStorage*>(), FRigUnit_AnimNextInterfaceBeginExecution::EventName) != ERigVMExecuteResult::Failed);
	}
	
	return bResult;
}

TArray<FRigVMExternalVariable> UAnimNextInterfaceGraph::GetRigVMExternalVariables()
{
	return TArray<FRigVMExternalVariable>(); 
}

static TArray<UClass*> GetClassObjectsInPackage(UPackage* InPackage)
{
	TArray<UObject*> Objects;
	GetObjectsWithOuter(InPackage, Objects, false);

	TArray<UClass*> ClassObjects;
	for (UObject* Object : Objects)
	{
		if (UClass* Class = Cast<UClass>(Object))
		{
			ClassObjects.Add(Class);
		}
	}

	return ClassObjects;
}

void UAnimNextInterfaceGraph::PostRename(UObject* OldOuter, const FName OldName)
{
	Super::PostRename(OldOuter, OldName);

	// Whenever the asset is renamed/moved, generated classes parented to the old package
	// are not moved to the new package automatically (see FAssetRenameManager), so we
	// have to manually perform the move/rename, to avoid invalid reference to the old package

	// Note: while asset duplication doesn't duplicate the classes either, it is not a problem there
	// because we always recompile in post duplicate.
	TArray<UClass*> ClassObjects = GetClassObjectsInPackage(OldOuter->GetPackage());

	for (UClass* ClassObject : ClassObjects)
	{
		if (URigVMMemoryStorageGeneratorClass* MemoryClass = Cast<URigVMMemoryStorageGeneratorClass>(ClassObject))
		{
			MemoryClass->Rename(nullptr, GetPackage(), REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
		}
	}
}

void UAnimNextInterfaceGraph::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	Super::GetPreloadDependencies(OutDeps);

	TArray<UClass*> ClassObjects = GetClassObjectsInPackage(GetPackage());

	for (UClass* ClassObject : ClassObjects)
	{
		if (URigVMMemoryStorageGeneratorClass* MemoryClass = Cast<URigVMMemoryStorageGeneratorClass>(ClassObject))
		{
			OutDeps.Add(MemoryClass);
		}
	}
}
