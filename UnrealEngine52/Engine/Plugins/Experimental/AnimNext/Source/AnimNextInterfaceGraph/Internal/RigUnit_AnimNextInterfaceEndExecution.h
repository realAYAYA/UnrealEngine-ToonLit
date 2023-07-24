// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "AnimNextInterfaceExecuteContext.h"
#include "AnimNextInterfaceUnitContext.h"
#include "AnimNextInterfaceTypes.h"
#include "RigUnit_AnimNextInterfaceEndExecution.generated.h"

struct FRigUnitContext;

/** Event for writing back calculated results to external variables */
USTRUCT(meta = (DisplayName = "End Execute Anim Interface", Category = "Events", TitleColor = "1 0 0", NodeColor = "1 1 1"))
struct ANIMNEXTINTERFACEGRAPH_API FRigUnit_AnimNextInterfaceEndExecution : public FRigUnit_AnimNextInterfaceBase
{
	GENERATED_BODY()

	template<typename ValueType>
	static void SetResult(const FRigVMExecuteContext& InExecuteContext, const ValueType& InResult)
	{
		const FAnimNextInterfaceExecuteContext& AnimNextInterfaceExecuteContext = static_cast<const FAnimNextInterfaceExecuteContext&>(InExecuteContext);
		AnimNextInterfaceExecuteContext.GetContext().SetResult<ValueType>(InResult);
	}

public:
	// The execution result
	UPROPERTY(EditAnywhere, Transient, DisplayName = "End Execute", Category = "EndExecution", meta = (Input))
	FAnimNextInterfaceExecuteContext ExecuteContext;
};

/** Event for writing back a calculated bool */
USTRUCT(meta = (DisplayName = "End Execute Anim Interface Bool", Category = "Events", TitleColor = "1 0 0", NodeColor = "1 1 1"))
struct ANIMNEXTINTERFACEGRAPH_API FRigUnit_AnimNextInterfaceEndExecution_Bool : public FRigUnit_AnimNextInterfaceEndExecution
{
	GENERATED_BODY()
	
	FRigUnit_AnimNextInterfaceEndExecution_Bool()
		: FRigUnit_AnimNextInterfaceEndExecution()
		, Result(false)
	{}
	
	RIGVM_METHOD()
	virtual void Execute() override;

	virtual bool CanOnlyExistOnce() const override { return true; }
	
	UPROPERTY(EditAnywhere, Category = Result, meta = (Input))
	bool Result;
};

/** Event for writing back a calculated float */
USTRUCT(meta = (DisplayName = "End Execute Anim Interface Float", Category = "Events", TitleColor = "1 0 0", NodeColor = "1 1 1"))
struct ANIMNEXTINTERFACEGRAPH_API FRigUnit_AnimNextInterfaceEndExecution_Float : public FRigUnit_AnimNextInterfaceEndExecution
{
	GENERATED_BODY()

	FRigUnit_AnimNextInterfaceEndExecution_Float()
		: FRigUnit_AnimNextInterfaceEndExecution()
		, Result(0.f)
	{}

	RIGVM_METHOD()
	virtual void Execute() override;

	virtual bool CanOnlyExistOnce() const override { return true; }

	UPROPERTY(EditAnywhere, Category = Result, meta = (Input))
	float Result;
};
