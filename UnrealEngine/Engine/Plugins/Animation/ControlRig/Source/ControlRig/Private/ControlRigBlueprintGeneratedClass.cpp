// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigBlueprintGeneratedClass.h"
#include "Units/Control/RigUnit_Control.h"
#include "ControlRigObjectVersion.h"
#include "ControlRig.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigBlueprintGeneratedClass)

UControlRigBlueprintGeneratedClass::UControlRigBlueprintGeneratedClass(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UControlRigBlueprintGeneratedClass::Serialize(FArchive& Ar)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	// don't use URigVMBlueprintGeneratedClass
	// to avoid backwards compat issues.
	UBlueprintGeneratedClass::Serialize(Ar);

	Ar.UsingCustomVersion(FControlRigObjectVersion::GUID);

	if (Ar.CustomVer(FControlRigObjectVersion::GUID) < FControlRigObjectVersion::SwitchedToRigVM)
	{
		return;
	}

	// for debugging purposes we'll give this VM a name that's useful.
	static TAtomic<uint32> NumVMs{ 0 };
	static constexpr TCHAR Format[] = TEXT("%s_VM_%zu");
	const FString VMDebugName = FString::Printf(Format, *GetName(), uint32(++NumVMs));
	URigVM* VM = NewObject<URigVM>(GetTransientPackage(), *VMDebugName);

	if (UControlRig* CDO = Cast<UControlRig>(GetDefaultObject(true)))
	{
		if (Ar.IsSaving() && CDO->VM)
		{
			VM->CopyDataForSerialization(CDO->VM);
		}
	}
	
	VM->Serialize(Ar);

	if (UControlRig* CDO = Cast<UControlRig>(GetDefaultObject(false)))
	{
		if (Ar.IsLoading() && CDO->VM)
		{
			CDO->VM->CopyDataForSerialization(VM);
		}
	}

	if (Ar.CustomVer(FControlRigObjectVersion::GUID) < FControlRigObjectVersion::StoreFunctionsInGeneratedClass)
	{
		return;
	}
	
	Ar << GraphFunctionStore;
}

