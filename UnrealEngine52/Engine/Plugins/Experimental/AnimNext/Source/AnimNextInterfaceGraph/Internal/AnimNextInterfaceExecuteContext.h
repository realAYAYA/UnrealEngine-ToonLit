// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "Units/RigUnit.h"
#include "AnimNextInterfaceUnitContext.h"
#include "AnimNextInterfaceExecuteContext.generated.h"

class IAnimNextInterface;

namespace UE::AnimNext::Interface
{
	struct FContext;
}

USTRUCT(BlueprintType)
struct FAnimNextInterfaceExecuteContext : public FRigVMExecuteContext
{
	GENERATED_BODY()

	FAnimNextInterfaceExecuteContext()
		: FRigVMExecuteContext()
	, AnimNextInterfaceContext(nullptr)
	, Interface(nullptr)
	, ResultPtr(nullptr)
	, UnitContext()
	{
	}

	const UE::AnimNext::Interface::FContext& GetContext() const
	{
		check(AnimNextInterfaceContext);
		return *AnimNextInterfaceContext;
	}

	const FRigUnitContext& GetUnitContext() const
	{
		return UnitContext;
	}

	void SetContextData(const IAnimNextInterface* InInterface, const UE::AnimNext::Interface::FContext& InAnimNextInterfaceContext, bool& bInResult)
	{
		Interface = InInterface;
		AnimNextInterfaceContext = &InAnimNextInterfaceContext;
		ResultPtr = &bInResult;
	}

	void SetResult(bool bInResult) const
	{
		check(ResultPtr);
		*ResultPtr &= bInResult;
	}

	const IAnimNextInterface* GetInterface() const
	{
		check(Interface);
		return Interface;
	}
	
	virtual void Copy(const FRigVMExecuteContext* InOtherContext) override
	{
		Super::Copy(InOtherContext);

		const FAnimNextInterfaceExecuteContext* OtherContext = (const FAnimNextInterfaceExecuteContext*)InOtherContext; 
		AnimNextInterfaceContext = OtherContext->AnimNextInterfaceContext;
		Interface = OtherContext->Interface; 
		ResultPtr = OtherContext->ResultPtr; 
	}


private:
	const UE::AnimNext::Interface::FContext* AnimNextInterfaceContext;
	const IAnimNextInterface* Interface;
	bool* ResultPtr;
	FRigUnitContext UnitContext;
};

USTRUCT(meta=(ExecuteContext="FAnimNextInterfaceExecuteContext"))
struct FRigUnit_AnimNextInterfaceBase : public FRigUnit
{
	GENERATED_BODY()
};
