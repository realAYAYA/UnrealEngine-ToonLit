// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMCore/RigVMDispatchFactory.h"
#include "RigVMDispatch_Core.h"
#include "RigVMCore/RigVMByteCode.h"
#include "RigVMDispatch_Array.generated.h"

USTRUCT(meta=(Abstract, Category = "Array", Keywords = "List,Collection", NodeColor = "1,1,1,1"))
struct RIGVM_API FRigVMDispatch_ArrayBase : public FRigVMDispatch_CoreBase
{
	GENERATED_BODY()

public:
	virtual ERigVMOpCode GetOpCode() const { return ERigVMOpCode::Invalid; }
	static UScriptStruct* GetFactoryDispatchForOpCode(ERigVMOpCode InOpCode);
	static FName GetFactoryNameForOpCode(ERigVMOpCode InOpCode);
#if WITH_EDITOR
	virtual FString GetArgumentDefaultValue(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#endif
	virtual bool IsSingleton() const override { return true; } 

protected:
	static FRigVMTemplateArgumentInfo CreateArgumentInfo(const FName& InName, ERigVMPinDirection InDirection);
	static TMap<uint32, int32> GetArrayHash(FScriptArrayHelper& InArrayHelper, const FArrayProperty* InArrayProperty);
	
	static const FName& ExecuteName;
	static const FName ArrayName;
	static const FName ValuesName;
	static const FName NumName;
	static const FName IndexName;
	static const FName ElementName;
	static const FName SuccessName;
	static const FName OtherName;
	static const FName CloneName;
	static const FName CountName;
	static const FName RatioName;
	static const FName ResultName;
	static const FName& CompletedName;

	friend class URigVMController;
};

USTRUCT(meta=(Abstract))
struct RIGVM_API FRigVMDispatch_ArrayBaseMutable : public FRigVMDispatch_ArrayBase
{
	GENERATED_BODY()

public:
	virtual const TArray<FRigVMExecuteArgument>& GetExecuteArguments_Impl(const FRigVMDispatchContext& InContext) const override;
};

USTRUCT(meta=(DisplayName = "Make Array", Keywords = "Make,MakeArray,Constant,Reroute"))
struct RIGVM_API FRigVMDispatch_ArrayMake : public FRigVMDispatch_ArrayBase
{
	GENERATED_BODY()

public:
	FRigVMDispatch_ArrayMake()
	{
		FactoryScriptStruct = StaticStruct();
	}

	virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const override;
	virtual FRigVMTemplateTypeMap OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#if WITH_EDITOR
	virtual FText GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const override;
	virtual FText GetArgumentTooltip(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
	virtual FString GetArgumentMetaData(const FName& InArgumentName, const FName& InMetaDataKey) const override;
	virtual FString GetArgumentDefaultValue(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#endif
	virtual FName GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const override;

protected:
	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override { return &FRigVMDispatch_ArrayMake::Execute; }
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates);
};

USTRUCT(meta=(DisplayName = "Reset"))
struct RIGVM_API FRigVMDispatch_ArrayReset : public FRigVMDispatch_ArrayBaseMutable
{
	GENERATED_BODY()

public:
	FRigVMDispatch_ArrayReset()
	{
		FactoryScriptStruct = StaticStruct();
	}

	virtual ERigVMOpCode GetOpCode() const override { return ERigVMOpCode::ArrayReset; }
	virtual FName GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const override;
	virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const override;
	virtual FRigVMTemplateTypeMap OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#if WITH_EDITOR
	virtual FText GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const override;
	virtual FText GetArgumentTooltip(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#endif

protected:
	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override { return &FRigVMDispatch_ArrayReset::Execute; }
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates);
};

USTRUCT(meta=(DisplayName = "Num"))
struct RIGVM_API FRigVMDispatch_ArrayGetNum : public FRigVMDispatch_ArrayBase
{
	GENERATED_BODY()

public:
	FRigVMDispatch_ArrayGetNum()
	{
		FactoryScriptStruct = StaticStruct();
	}

	virtual ERigVMOpCode GetOpCode() const override { return ERigVMOpCode::ArrayGetNum; }
	virtual FName GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const override;
	virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const override;
	virtual FRigVMTemplateTypeMap OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#if WITH_EDITOR
	virtual FText GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const override;
	virtual FText GetArgumentTooltip(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#endif

protected:
	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override { return &FRigVMDispatch_ArrayGetNum::Execute; }
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates);
};

USTRUCT(meta=(DisplayName = "Set Num"))
struct RIGVM_API FRigVMDispatch_ArraySetNum : public FRigVMDispatch_ArrayBaseMutable
{
	GENERATED_BODY()

public:
	FRigVMDispatch_ArraySetNum()
	{
		FactoryScriptStruct = StaticStruct();
	}

	virtual ERigVMOpCode GetOpCode() const override { return ERigVMOpCode::ArraySetNum; }
	virtual FName GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const override;
	virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const override;
	virtual FRigVMTemplateTypeMap OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#if WITH_EDITOR
	virtual FText GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const override;
	virtual FText GetArgumentTooltip(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#endif

protected:
	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override { return &FRigVMDispatch_ArraySetNum::Execute; }
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates);
};

USTRUCT(meta=(DisplayName = "At", Keywords = "Get Index,At Index,[]"))
struct RIGVM_API FRigVMDispatch_ArrayGetAtIndex : public FRigVMDispatch_ArrayBase
{
	GENERATED_BODY()

public:
	FRigVMDispatch_ArrayGetAtIndex()
	{
		FactoryScriptStruct = StaticStruct();
	}

	virtual ERigVMOpCode GetOpCode() const override { return ERigVMOpCode::ArrayGetAtIndex; }
	virtual FName GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const override;
	virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const override;
	virtual FRigVMTemplateTypeMap OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#if WITH_EDITOR
	virtual FText GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const override;
	virtual FText GetArgumentTooltip(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#endif

protected:
	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override { return &FRigVMDispatch_ArrayGetAtIndex::Execute; }
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates);
};

USTRUCT(meta=(DisplayName = "Set At"))
struct RIGVM_API FRigVMDispatch_ArraySetAtIndex : public FRigVMDispatch_ArrayBaseMutable
{
	GENERATED_BODY()

public:
	FRigVMDispatch_ArraySetAtIndex()
	{
		FactoryScriptStruct = StaticStruct();
	}

	virtual ERigVMOpCode GetOpCode() const override { return ERigVMOpCode::ArraySetAtIndex; }
	virtual FName GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const override;
	virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const override;
	virtual FRigVMTemplateTypeMap OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#if WITH_EDITOR
	virtual FText GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const override;
	virtual FText GetArgumentTooltip(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#endif

protected:
	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override { return &FRigVMDispatch_ArraySetAtIndex::Execute; }
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates);
};

USTRUCT(meta=(DisplayName = "Add"))
struct RIGVM_API FRigVMDispatch_ArrayAdd : public FRigVMDispatch_ArraySetAtIndex
{
	GENERATED_BODY()

public:
	FRigVMDispatch_ArrayAdd()
	{
		FactoryScriptStruct = StaticStruct();
	}

	virtual ERigVMOpCode GetOpCode() const override { return ERigVMOpCode::ArrayAdd; }
	virtual FName GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const override;
	virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const override;
#if WITH_EDITOR
	virtual FText GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const override;
	virtual FText GetArgumentTooltip(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#endif

protected:
	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override { return &FRigVMDispatch_ArrayAdd::Execute; }
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates);
};

USTRUCT(meta=(DisplayName = "Insert"))
struct RIGVM_API FRigVMDispatch_ArrayInsert : public FRigVMDispatch_ArraySetAtIndex
{
	GENERATED_BODY()

public:
	FRigVMDispatch_ArrayInsert()
	{
		FactoryScriptStruct = StaticStruct();
	}

	virtual ERigVMOpCode GetOpCode() const override { return ERigVMOpCode::ArrayInsert; }
#if WITH_EDITOR
   	virtual FText GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const override;
   	virtual FText GetArgumentTooltip(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#endif

protected:
	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override { return &FRigVMDispatch_ArrayInsert::Execute; }
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates);
};

USTRUCT(meta=(DisplayName = "Remove"))
struct RIGVM_API FRigVMDispatch_ArrayRemove : public FRigVMDispatch_ArrayBaseMutable
{
	GENERATED_BODY()

public:
	FRigVMDispatch_ArrayRemove()
	{
		FactoryScriptStruct = StaticStruct();
	}

	virtual ERigVMOpCode GetOpCode() const override { return ERigVMOpCode::ArrayRemove; }
	virtual FName GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const override;
	virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const override;
	virtual FRigVMTemplateTypeMap OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#if WITH_EDITOR
   	virtual FText GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const override;
   	virtual FText GetArgumentTooltip(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#endif

protected:
	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override { return &FRigVMDispatch_ArrayRemove::Execute; }
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates);
};

USTRUCT(meta=(DisplayName = "Reverse"))
struct RIGVM_API FRigVMDispatch_ArrayReverse : public FRigVMDispatch_ArrayReset
{
	GENERATED_BODY()

public:
	FRigVMDispatch_ArrayReverse()
	{
		FactoryScriptStruct = StaticStruct();
	}

	virtual ERigVMOpCode GetOpCode() const override { return ERigVMOpCode::ArrayReverse; }
#if WITH_EDITOR
   	virtual FText GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const override;
   	virtual FText GetArgumentTooltip(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#endif

protected:

	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override { return &FRigVMDispatch_ArrayReverse::Execute; }
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates);
};

USTRUCT(meta=(DisplayName = "Find"))
struct RIGVM_API FRigVMDispatch_ArrayFind : public FRigVMDispatch_ArrayBase
{
	GENERATED_BODY()

public:
	FRigVMDispatch_ArrayFind()
	{
		FactoryScriptStruct = StaticStruct();
	}

	virtual ERigVMOpCode GetOpCode() const override { return ERigVMOpCode::ArrayFind; }
	virtual FName GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const override;
	virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const override;
	virtual FRigVMTemplateTypeMap OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#if WITH_EDITOR
   	virtual FText GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const override;
   	virtual FText GetArgumentTooltip(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#endif

protected:
	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override { return &FRigVMDispatch_ArrayFind::Execute; }
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates);
};

USTRUCT(meta=(DisplayName = "Append"))
struct RIGVM_API FRigVMDispatch_ArrayAppend : public FRigVMDispatch_ArrayBaseMutable
{
	GENERATED_BODY()

public:
	FRigVMDispatch_ArrayAppend()
	{
		FactoryScriptStruct = StaticStruct();
	}

	virtual ERigVMOpCode GetOpCode() const override { return ERigVMOpCode::ArrayAppend; }
	virtual FName GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const override;
	virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const override;
	virtual FRigVMTemplateTypeMap OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#if WITH_EDITOR
   	virtual FText GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const override;
   	virtual FText GetArgumentTooltip(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#endif

protected:
	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override { return &FRigVMDispatch_ArrayAppend::Execute; }
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates);
};

USTRUCT(meta=(DisplayName = "Clone"))
struct RIGVM_API FRigVMDispatch_ArrayClone : public FRigVMDispatch_ArrayBase
{
	GENERATED_BODY()

public:
	FRigVMDispatch_ArrayClone()
	{
		FactoryScriptStruct = StaticStruct();
	}

	virtual ERigVMOpCode GetOpCode() const override { return ERigVMOpCode::ArrayClone; }
	virtual FName GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const override;
	virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const override;
	virtual FRigVMTemplateTypeMap OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#if WITH_EDITOR
   	virtual FText GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const override;
   	virtual FText GetArgumentTooltip(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#endif

protected:
	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override { return &FRigVMDispatch_ArrayClone::Execute; }
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates);
};

USTRUCT(meta=(DisplayName = "Union"))
struct RIGVM_API FRigVMDispatch_ArrayUnion : public FRigVMDispatch_ArrayAppend
{
	GENERATED_BODY()

public:
	FRigVMDispatch_ArrayUnion()
	{
		FactoryScriptStruct = StaticStruct();
	}

	virtual ERigVMOpCode GetOpCode() const override { return ERigVMOpCode::ArrayUnion; }
#if WITH_EDITOR
   	virtual FText GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const override;
   	virtual FText GetArgumentTooltip(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#endif

protected:
	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override { return &FRigVMDispatch_ArrayUnion::Execute; }
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates);
};

USTRUCT(meta=(DisplayName = "Difference"))
struct RIGVM_API FRigVMDispatch_ArrayDifference : public FRigVMDispatch_ArrayBase
{
	GENERATED_BODY()

public:
	FRigVMDispatch_ArrayDifference()
	{
		FactoryScriptStruct = StaticStruct();
	}

	virtual ERigVMOpCode GetOpCode() const override { return ERigVMOpCode::ArrayDifference; }
	virtual FName GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const override;
	virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const override;
	virtual FRigVMTemplateTypeMap OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#if WITH_EDITOR
   	virtual FText GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const override;
   	virtual FText GetArgumentTooltip(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#endif

protected:
	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override { return &FRigVMDispatch_ArrayDifference::Execute; }
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates);
};

USTRUCT(meta=(DisplayName = "Intersection"))
struct RIGVM_API FRigVMDispatch_ArrayIntersection : public FRigVMDispatch_ArrayDifference
{
	GENERATED_BODY()

public:
	FRigVMDispatch_ArrayIntersection()
	{
		FactoryScriptStruct = StaticStruct();
	}

	virtual ERigVMOpCode GetOpCode() const override { return ERigVMOpCode::ArrayIntersection; }
#if WITH_EDITOR
   	virtual FText GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const override;
   	virtual FText GetArgumentTooltip(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#endif

protected:
	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override { return &FRigVMDispatch_ArrayIntersection::Execute; }
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates);
};

USTRUCT(meta=(DisplayName = "For Each", Icon="EditorStyle|GraphEditor.Macro.ForEach_16x"))
struct RIGVM_API FRigVMDispatch_ArrayIterator : public FRigVMDispatch_ArrayBaseMutable
{
	GENERATED_BODY()

public:
	FRigVMDispatch_ArrayIterator()
	{
		FactoryScriptStruct = StaticStruct();
	}

	virtual ERigVMOpCode GetOpCode() const override { return ERigVMOpCode::ArrayIterator; }
	virtual FName GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const override;
	virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const override;
	virtual const TArray<FRigVMExecuteArgument>& GetExecuteArguments_Impl(const FRigVMDispatchContext& InContext) const override;
	virtual const TArray<FName>& GetControlFlowBlocks_Impl(const FRigVMDispatchContext& InContext) const override;
	virtual const bool IsControlFlowBlockSliced(const FName& InBlockName) const override;
	virtual FRigVMTemplateTypeMap OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#if WITH_EDITOR
   	virtual FText GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const override;
   	virtual FText GetArgumentTooltip(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#endif

protected:
	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override { return &FRigVMDispatch_ArrayIterator::Execute; }
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates);
};
