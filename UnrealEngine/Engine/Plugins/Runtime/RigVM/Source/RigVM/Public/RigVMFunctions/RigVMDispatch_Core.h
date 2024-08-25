// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMCore/RigVMDispatchFactory.h"
#include "RigVMDispatch_Core.generated.h"

USTRUCT(meta=(Abstract, Category = "Core", NodeColor = "0.762745, 1,0, 0.329412"))
struct RIGVM_API FRigVMDispatch_CoreBase : public FRigVMDispatchFactory
{
	GENERATED_BODY()
};

/*
 * Compares any two values and return true if they are identical
 */
USTRUCT(meta=(DisplayName = "Equals", Keywords = "Same,=="))
struct RIGVM_API FRigVMDispatch_CoreEquals : public FRigVMDispatch_CoreBase
{
	GENERATED_BODY()

public:

	FRigVMDispatch_CoreEquals()
	{
		FactoryScriptStruct = StaticStruct();
	}

	virtual FName GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const override;
	virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const override;
	virtual FRigVMTemplateTypeMap OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;

protected:

	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override;
	static bool AdaptResult(bool bResult, const FRigVMExtendedExecuteContext& InContext);
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates);

	template<typename T>
	static void Equals(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates)
	{
		check(Handles[0].IsType<T>());
		check(Handles[1].IsType<T>());
		check(Handles[2].IsBool());
		const T& A = *(const T*)Handles[0].GetData();
		const T& B = *(const T*)Handles[1].GetData();
		bool& Result = *(bool*)Handles[2].GetData();
		Result = AdaptResult(A == B, InContext);
	}

	static void NameEquals(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates)
	{
		check(Handles[0].IsType<FName>());
		check(Handles[1].IsType<FName>());
		check(Handles[2].IsBool());
		const FName& A = *(const FName*)Handles[0].GetData();
		const FName& B = *(const FName*)Handles[1].GetData();
		bool& Result = *(bool*)Handles[2].GetData();
		Result = AdaptResult(A.IsEqual(B, ENameCase::CaseSensitive), InContext);
	}

	static void StringEquals(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates)
	{
		check(Handles[0].IsType<FString>());
		check(Handles[1].IsType<FString>());
		check(Handles[2].IsBool());
		const FString& A = *(const FString*)Handles[0].GetData();
		const FString& B = *(const FString*)Handles[1].GetData();
		bool& Result = *(bool*)Handles[2].GetData();
		Result = AdaptResult(A.Equals(B, ESearchCase::CaseSensitive), InContext);
	}

	template<typename T>
	static void MathTypeEquals(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates)
	{
		check(Handles[0].IsType<T>());
		check(Handles[1].IsType<T>());
		check(Handles[2].IsBool());
		const T& A = *(const T*)Handles[0].GetData();
		const T& B = *(const T*)Handles[1].GetData();
		bool& Result = *(bool*)Handles[2].GetData();
		Result = AdaptResult(A.Equals(B), InContext);
	}

	static const FName AName;
	static const FName BName;
	static const FName ResultName;
};

/*
 * Compares any two values and return true if they are identical
 */
USTRUCT(meta=(DisplayName = "Not Equals", Keywords = "Different,!=,Xor"))
struct RIGVM_API FRigVMDispatch_CoreNotEquals : public FRigVMDispatch_CoreEquals
{
	GENERATED_BODY()

	FRigVMDispatch_CoreNotEquals()
	{
		FactoryScriptStruct = StaticStruct();
	}

	virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const override;

	// we are inheriting everything from the equals dispatch,
	// and due to the check of the factory within FRigVMDispatch_CoreEquals::Execute we can
	// rely on that completely. we only need this class for the displayname and
	// operation specific StaticStruct().
};
