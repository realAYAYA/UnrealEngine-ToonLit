// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "RigVMModel/RigVMGraph.h"
#include "RigVMModel/Nodes/RigVMUnitNode.h"
#include "RigVMModel/Nodes/RigVMVariableNode.h"
#include "RigVMModel/Nodes/RigVMParameterNode.h"
#include "RigVMModel/Nodes/RigVMCommentNode.h"
#include "RigVMModel/Nodes/RigVMRerouteNode.h"
#include "RigVMCore/RigVM.h"
#include "RigVMCore/RigVMStruct.h"
#include "RigVMCompiler/RigVMAST.h"
#include "Logging/TokenizedMessage.h"

#include "RigVMCompiler.generated.h"

USTRUCT(BlueprintType)
struct RIGVMDEVELOPER_API FRigVMCompileSettings
{
	GENERATED_BODY()

public:

	FRigVMCompileSettings();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FRigVMCompileSettings)
	bool SurpressInfoMessages;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FRigVMCompileSettings)
	bool SurpressWarnings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FRigVMCompileSettings)
	bool SurpressErrors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FRigVMCompileSettings)
	bool EnablePinWatches;

	UPROPERTY(Transient)
	bool IsPreprocessorPhase;

	UPROPERTY(EditAnywhere, Transient, BlueprintReadWrite, Category = FRigVMCompileSettings)
	FRigVMParserASTSettings ASTSettings;

	UPROPERTY()
	bool SetupNodeInstructionIndex;

	static FRigVMCompileSettings Fast()
	{
		FRigVMCompileSettings Settings;
		Settings.EnablePinWatches = true;
		Settings.IsPreprocessorPhase = false;
		Settings.ASTSettings = FRigVMParserASTSettings::Fast();
		return Settings;
	}

	static FRigVMCompileSettings Optimized()
	{
		FRigVMCompileSettings Settings;
		Settings.EnablePinWatches = false;
		Settings.IsPreprocessorPhase = false;
		Settings.ASTSettings = FRigVMParserASTSettings::Optimized();
		return Settings;
	}

	void Report(EMessageSeverity::Type InSeverity, UObject* InSubject, const FString& InMessage) const
	{
		ASTSettings.Report(InSeverity, InSubject, InMessage);
	}

	template <typename FmtType, typename... Types>
	void Reportf(EMessageSeverity::Type InSeverity, UObject* InSubject, const FmtType& Fmt, Types... Args) const
	{
		Report(InSeverity, InSubject, FString::Printf(Fmt, Args...));
	}
};

struct RIGVMDEVELOPER_API FRigVMCompilerWorkData
{
public:
	bool bSetupMemory;
	URigVM* VM;
	UScriptStruct* ExecuteContextStruct;
	FRigVMUserDataArray RigVMUserData;
	TMap<FString, FRigVMOperand>* PinPathToOperand;
	TMap<const FRigVMVarExprAST*, FRigVMOperand> ExprToOperand;
	TMap<const FRigVMExprAST*, bool> ExprComplete;
	TArray<const FRigVMExprAST*> ExprToSkip;
	TMap<FString, int32> ProcessedLinks;
	TMap<int32, FRigVMOperand> IntegerLiterals;
	FRigVMOperand ComparisonOperand;

	using FRigVMASTProxyArray = TArray<FRigVMASTProxy, TInlineAllocator<3>>; 
	using FRigVMASTProxySourceMap = TMap<FRigVMASTProxy, FRigVMASTProxy>;
	using FRigVMASTProxyTargetsMap =
		TMap<FRigVMASTProxy, FRigVMASTProxyArray>;
	TMap<FRigVMASTProxy, FRigVMASTProxyArray> CachedProxiesWithSharedOperand;
	const FRigVMASTProxySourceMap* ProxySources;
	FRigVMASTProxyTargetsMap ProxyTargets;

	TArray<URigVMPin*> WatchedPins;
	
	TMap<ERigVMMemoryType, TArray<FRigVMPropertyPathDescription>> PropertyPathDescriptions;
	TMap<ERigVMMemoryType, TArray<FRigVMPropertyDescription>> PropertyDescriptions;

	FRigVMOperand AddProperty(ERigVMMemoryType InMemoryType, const FName& InName, const FString& InCPPType, UObject* InCPPTypeObject, const FString& InDefaultValue = FString());
	FRigVMOperand FindProperty(ERigVMMemoryType InMemoryType, const FName& InName);
	FRigVMPropertyDescription GetProperty(const FRigVMOperand& InOperand);
	int32 FindOrAddPropertyPath(const FRigVMOperand& InOperand, const FString& InHeadCPPType, const FString& InSegmentPath);

	TSharedPtr<FRigVMParserAST> AST;
	
	struct FCopyOpInfo
	{
		FRigVMCopyOp Op;
		const FRigVMAssignExprAST* AssignExpr;
		const FRigVMVarExprAST* SourceExpr;
		const FRigVMVarExprAST* TargetExpr;
	};

	// operators that have been delayed for injection into the bytecode
	TMap<FRigVMOperand, FCopyOpInfo> DeferredCopyOps;
};

UCLASS(BlueprintType)
class RIGVMDEVELOPER_API URigVMCompiler : public UObject
{
	GENERATED_BODY()

public:

	URigVMCompiler();

	UPROPERTY(BlueprintReadWrite, Category = FRigVMCompiler)
	FRigVMCompileSettings Settings;

	UFUNCTION(BlueprintCallable, Category = FRigVMCompiler)
	bool Compile(TArray<URigVMGraph*> InGraphs, URigVMController* InController, URigVM* OutVM)
	{
		return Compile(InGraphs, InController, OutVM, TArray<FRigVMExternalVariable>(), TArray<FRigVMUserDataArray>(), nullptr);
	}

	bool Compile(TArray<URigVMGraph*> InGraphs, URigVMController* InController, URigVM* OutVM, const TArray<FRigVMExternalVariable>& InExternalVariables, const TArray<FRigVMUserDataArray>& InRigVMUserData, TMap<FString, FRigVMOperand>* OutOperands, TSharedPtr<FRigVMParserAST> InAST = TSharedPtr<FRigVMParserAST>());

	static UScriptStruct* GetScriptStructForCPPType(const FString& InCPPType);
	static FString GetPinHash(const URigVMPin* InPin, const FRigVMVarExprAST* InVarExpr, bool bIsDebugValue = false, const FRigVMASTProxy& InPinProxy = FRigVMASTProxy());

	// follows assignment expressions to find the source ref counted containers
	// since ref counted containers are not copied for assignments.
	// this is currently only used for arrays.
	static const FRigVMVarExprAST* GetSourceVarExpr(const FRigVMExprAST* InExpr);

	void MarkDebugWatch(bool bRequired, URigVMPin* InPin, URigVM* OutVM, TMap<FString, FRigVMOperand>* OutOperands, TSharedPtr<FRigVMParserAST> InRuntimeAST);

private:

	TArray<URigVMPin*> GetLinkedPins(URigVMPin* InPin, bool bInputs = true, bool bOutputs = true, bool bRecursive = true);
	uint16 GetElementSizeFromCPPType(const FString& InCPPType, UScriptStruct* InScriptStruct);

	static FString GetPinHashImpl(const URigVMPin* InPin, const FRigVMVarExprAST* InVarExpr, bool bIsDebugValue = false, const FRigVMASTProxy& InPinProxy = FRigVMASTProxy());

	void TraverseExpression(const FRigVMExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	void TraverseChildren(const FRigVMExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	void TraverseBlock(const FRigVMBlockExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	void TraverseEntry(const FRigVMEntryExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	int32 TraverseCallExtern(const FRigVMCallExternExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	void TraverseForLoop(const FRigVMCallExternExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	void TraverseNoOp(const FRigVMNoOpExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	void TraverseVar(const FRigVMVarExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	void TraverseLiteral(const FRigVMVarExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	void TraverseExternalVar(const FRigVMExternalVarExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	void TraverseAssign(const FRigVMAssignExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	void TraverseCopy(const FRigVMCopyExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	void TraverseCachedValue(const FRigVMCachedValueExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	void TraverseExit(const FRigVMExitExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	void TraverseBranch(const FRigVMBranchExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	void TraverseIf(const FRigVMIfExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	void TraverseSelect(const FRigVMSelectExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	void TraverseArray(const FRigVMArrayExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	void TraverseInvokeEntry(const FRigVMInvokeEntryExprAST* InExpr, FRigVMCompilerWorkData& WorkData);

	void AddCopyOperator(
		const FRigVMCopyOp& InOp,
		const FRigVMAssignExprAST* InAssignExpr,
		const FRigVMVarExprAST* InSourceExpr,
		const FRigVMVarExprAST* InTargetExpr,
		FRigVMCompilerWorkData& WorkData,
		bool bDelayCopyOperations = true);

	void AddCopyOperator(
		const FRigVMCompilerWorkData::FCopyOpInfo& CopyOpInfo,
		FRigVMCompilerWorkData& WorkData,
		bool bDelayCopyOperations = true);

	void InitializeLocalVariables(const FRigVMExprAST* InExpr, FRigVMCompilerWorkData& WorkData);

	FRigVMOperand FindOrAddRegister(const FRigVMVarExprAST* InVarExpr, FRigVMCompilerWorkData& WorkData, bool bIsDebugValue = false);
	const FRigVMCompilerWorkData::FRigVMASTProxyArray& FindProxiesWithSharedOperand(const FRigVMVarExprAST* InVarExpr, FRigVMCompilerWorkData& WorkData);

	bool ValidateNode(URigVMNode* InNode, bool bCheck = true);
	
	void ReportInfo(const FString& InMessage);
	void ReportWarning(const FString& InMessage);
	void ReportError(const FString& InMessage);

	template <typename FmtType, typename... Types>
	void ReportInfof(const FmtType& Fmt, Types... Args)
	{
		ReportInfo(FString::Printf(Fmt, Args...));
	}

	template <typename FmtType, typename... Types>
	void ReportWarningf(const FmtType& Fmt, Types... Args)
	{
		ReportWarning(FString::Printf(Fmt, Args...));
	}

	template <typename FmtType, typename... Types>
	void ReportErrorf(const FmtType& Fmt, Types... Args)
	{
		ReportError(FString::Printf(Fmt, Args...));
	}
	
	friend class FRigVMCompilerImportErrorContext;
};
