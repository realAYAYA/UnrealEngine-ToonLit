// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigBlueprintGeneratedClass.h"
#include "Units/Control/RigUnit_Control.h"
#include "ControlRigObjectVersion.h"
#include "ControlRig.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigBlueprintGeneratedClass)

UControlRigBlueprintGeneratedClass::UControlRigBlueprintGeneratedClass(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

uint8* UControlRigBlueprintGeneratedClass::GetPersistentUberGraphFrame(UObject* Obj, UFunction* FuncToCheck) const
{
	if(!IsInGameThread())
	{
		// we cant use the persistent frame if we are executing in parallel (as we could potentially thunk to BP)
		return nullptr;
	}
	return Super::GetPersistentUberGraphFrame(Obj, FuncToCheck);
}

void UControlRigBlueprintGeneratedClass::PostInitInstance(UObject* InObj, FObjectInstancingGraph* InstanceGraph)
{
	UControlRig* ControlRig = Cast<UControlRig>(InObj);
	check(ControlRig);

	UControlRig* CDO = nullptr;
	if(!ControlRig->HasAnyFlags(RF_ClassDefaultObject))
	{
		CDO = Cast<UControlRig>(GetDefaultObject());;
	}

	ControlRig->PostInitInstance(CDO);
}

void UControlRigBlueprintGeneratedClass::Serialize(FArchive& Ar)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FControlRigObjectVersion::GUID);

	if (Ar.CustomVer(FControlRigObjectVersion::GUID) < FControlRigObjectVersion::SwitchedToRigVM)
	{
		return;
	}

	URigVM* VM = NewObject<URigVM>(GetTransientPackage());

	if (UControlRig* CDO = Cast<UControlRig>(GetDefaultObject(true)))
	{
		if (Ar.IsSaving() && CDO->VM)
		{
			VM->CopyFrom(CDO->VM);
		}
	}
	
	VM->Serialize(Ar);

	if (UControlRig* CDO = Cast<UControlRig>(GetDefaultObject(false)))
	{
		if (Ar.IsLoading())
		{
			CDO->VM->CopyFrom(VM);
		}
	}
}

