// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "DataInterfaceExecuteContext.generated.h"

class IDataInterface;

namespace UE::DataInterface
{
	struct FContext;
}

USTRUCT(BlueprintType)
struct FDataInterfaceExecuteContext : public FRigVMExecuteContext
{
	GENERATED_BODY()

	FDataInterfaceExecuteContext() = default;

	const UE::DataInterface::FContext& GetContext() const
	{
		check(DataInterfaceContext);
		return *DataInterfaceContext;
	}

	void SetResult(bool bInResult) const
	{
		check(ResultPtr);
		*ResultPtr &= bInResult;
	}

	const IDataInterface* GetInterface() const
	{
		check(Interface);
		return Interface;
	}

	FORCEINLINE void CopyFrom(const FDataInterfaceExecuteContext& Other)
	{
		FRigVMExecuteContext::CopyFrom(Other);
		DataInterfaceContext = Other.DataInterfaceContext;
		ResultPtr = Other.ResultPtr;
	}

private:
	const UE::DataInterface::FContext* DataInterfaceContext = nullptr;
	const IDataInterface* Interface = nullptr;
	bool* ResultPtr = nullptr;
};