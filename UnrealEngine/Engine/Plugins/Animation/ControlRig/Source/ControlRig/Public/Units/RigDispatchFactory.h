// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ControlRigDefines.h"
#include "RigVMCore/RigVMDispatchFactory.h"
#include "RigUnitContext.h"
#include "RigDispatchFactory.generated.h"

/** Base class for all rig dispatch factories */
USTRUCT(BlueprintType, meta=(Abstract, ExecuteContextType=FControlRigExecuteContext))
struct CONTROLRIG_API FRigDispatchFactory : public FRigVMDispatchFactory
{
	GENERATED_BODY()

	virtual UScriptStruct* GetExecuteContextStruct() const override
	{
		return FControlRigExecuteContext::StaticStruct();
	}

	virtual void RegisterDependencyTypes() const override
	{
		FRigVMRegistry::Get().FindOrAddType(FControlRigExecuteContext::StaticStruct());
		FRigVMRegistry::Get().FindOrAddType(FRigElementKey::StaticStruct());
    	FRigVMRegistry::Get().FindOrAddType(FCachedRigElement::StaticStruct());
	}

#if WITH_EDITOR

	virtual FString GetArgumentDefaultValue(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;

#endif

	static const FRigUnitContext& GetRigUnitContext(const FRigVMExtendedExecuteContext& InContext)
	{
		return InContext.GetPublicData<FControlRigExecuteContext>().UnitContext;
	}
};

