// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMBlueprintGeneratedClass.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "RigVMHost.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMBlueprintGeneratedClass)

URigVMBlueprintGeneratedClass::URigVMBlueprintGeneratedClass(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

uint8* URigVMBlueprintGeneratedClass::GetPersistentUberGraphFrame(UObject* Obj, UFunction* FuncToCheck) const
{
	if(!IsInGameThread())
	{
		// we cant use the persistent frame if we are executing in parallel (as we could potentially thunk to BP)
		return nullptr;
	}
	return Super::GetPersistentUberGraphFrame(Obj, FuncToCheck);
}

void URigVMBlueprintGeneratedClass::PostInitInstance(UObject* InObj, FObjectInstancingGraph* InstanceGraph)
{
	if (URigVMHost* Owner = Cast<URigVMHost>(InObj))
	{
		URigVMHost* CDO = nullptr;
		if (!Owner->HasAnyFlags(RF_ClassDefaultObject))
		{
			CDO = Cast<URigVMHost>(GetDefaultObject());;
		}
		Owner->PostInitInstance(CDO);
	}
}

void URigVMBlueprintGeneratedClass::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);

	if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::RigVMGeneratedClass)
	{
		return;
	}

	// If we're the blueprint skeleton class, we don't need the VM. We should be an empty vessel. Ideally there should
	// be a class flag to check for this, but there isn't, so we're left with name matching. 
	auto IsSkeletonClass = [](const UClass* InClass)
	{
#if WITH_EDITOR 
		return InClass->ClassGeneratedBy &&
			(InClass->GetName().StartsWith(TEXT("SKEL_")) || InClass->GetName().StartsWith(TEXT("REINST_SKEL_")));
#else
		// For non-editor targets, this is all irrelevant, since the skeleton class will never exist for them.
		return false;
#endif
	};

	URigVM* VM = NewObject<URigVM>(GetTransientPackage());

	if (!IsSkeletonClass(this))
	{
		if (const URigVMHost* CDO = Cast<URigVMHost>(GetDefaultObject(true)))
		{
			if (Ar.IsSaving() && CDO->VM)
			{
				VM->CopyFrom(CDO->VM);
			}
		}
	}
	
	VM->Serialize(Ar);

	if (!IsSkeletonClass(this))
	{
		if (const URigVMHost* CDO = Cast<URigVMHost>(GetDefaultObject(false)))
		{
			if (Ar.IsLoading())
			{
				CDO->VM->CopyFrom(VM);
			}
		}
	}

	Ar << GraphFunctionStore;
}

