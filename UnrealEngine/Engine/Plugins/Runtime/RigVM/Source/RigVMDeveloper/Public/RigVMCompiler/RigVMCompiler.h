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
#include "RigVMModel/Nodes/RigVMLibraryNode.h"

#include "RigVMCompiler.generated.h"

class URigVMFunctionReferenceNode;
struct FRigVMGraphFunctionData;
struct FRigVMGraphFunctionHeader;
struct FRigVMFunctionCompilationData;

USTRUCT(BlueprintType)
struct RIGVMDEVELOPER_API FRigVMCompileSettings
{
	GENERATED_BODY()

public:

	FRigVMCompileSettings();

	FRigVMCompileSettings(UScriptStruct* InExecuteContextScriptStruct);

	UScriptStruct* GetExecuteContextStruct() const { return ASTSettings.ExecuteContextStruct; }
	void SetExecuteContextStruct(UScriptStruct* InExecuteContextScriptStruct) { ASTSettings.ExecuteContextStruct = InExecuteContextScriptStruct; }

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
	
	UPROPERTY()
	bool ASTErrorsAsNotifications;

	static FRigVMCompileSettings Fast(UScriptStruct* InExecuteContextStruct = nullptr)
	{
		FRigVMCompileSettings Settings(InExecuteContextStruct);
		Settings.EnablePinWatches = true;
		Settings.IsPreprocessorPhase = false;
		Settings.ASTSettings = FRigVMParserASTSettings::Fast(InExecuteContextStruct);
		return Settings;
	}

	static FRigVMCompileSettings Optimized(UScriptStruct* InExecuteContextStruct = nullptr)
	{
		FRigVMCompileSettings Settings(InExecuteContextStruct);
		Settings.EnablePinWatches = false;
		Settings.IsPreprocessorPhase = false;
		Settings.ASTSettings = FRigVMParserASTSettings::Optimized(InExecuteContextStruct);
		return Settings;
	}

	void ReportInfo(const FString& InMessage) const;
	void ReportWarning(const FString& InMessage) const;
	void ReportError(const FString& InMessage) const;

	template <typename FmtType, typename... Types>
	void ReportInfof(const FmtType& Fmt, Types... Args) const
	{
		ReportInfo(FString::Printf(Fmt, Args...));
	}

	template <typename FmtType, typename... Types>
	void ReportWarningf(const FmtType& Fmt, Types... Args) const
	{
		ReportWarning(FString::Printf(Fmt, Args...));
	}

	template <typename FmtType, typename... Types>
	void ReportErrorf(const FmtType& Fmt, Types... Args) const
	{
		ReportError(FString::Printf(Fmt, Args...));
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

class RIGVMDEVELOPER_API FRigVMCompileSettingsDuringLoadGuard
{
public:

	FRigVMCompileSettingsDuringLoadGuard(FRigVMCompileSettings& InSettings)
		: ASTErrorsAsNotifications(InSettings.ASTErrorsAsNotifications, true)
	{}

private:

	TGuardValue<bool> ASTErrorsAsNotifications;
};

struct RIGVMDEVELOPER_API FRigVMCompilerWorkData
{
public:
	FRigVMCompileSettings Settings;
	bool bSetupMemory = false;
	URigVM* VM = nullptr;
	TArray<URigVMGraph*> Graphs;
	UScriptStruct* ExecuteContextStruct = nullptr;
	TMap<FString, FRigVMOperand>* PinPathToOperand = nullptr;
	FRigVMExtendedExecuteContext* Context = nullptr;
	TMap<const FRigVMVarExprAST*, FRigVMOperand> ExprToOperand;
	TMap<const FRigVMExprAST*, bool> ExprComplete;
	TArray<const FRigVMExprAST*> ExprToSkip;
	TArray<const FRigVMExprAST*> TraversalExpressions;
	TMap<FString, int32> ProcessedLinks;
	TMap<int32, FRigVMOperand> IntegerLiterals;
	FRigVMOperand ComparisonOperand;

	using FRigVMASTProxyArray = TArray<FRigVMASTProxy, TInlineAllocator<3>>; 
	using FRigVMASTProxySourceMap = TMap<FRigVMASTProxy, FRigVMASTProxy>;
	using FRigVMASTProxyTargetsMap =
		TMap<FRigVMASTProxy, FRigVMASTProxyArray>;
	TMap<FRigVMASTProxy, FRigVMASTProxyArray> CachedProxiesWithSharedOperand;
	const FRigVMASTProxySourceMap* ProxySources = nullptr;
	FRigVMASTProxyTargetsMap ProxyTargets;
	TMap<URigVMNode*, TArray<FRigVMBranchInfo>> BranchInfos;

	struct FFunctionRegisterData
	{
		TSoftObjectPtr<URigVMFunctionReferenceNode> ReferenceNode;
		ERigVMMemoryType MemoryType = ERigVMMemoryType::Invalid;
		int32 RegisterIndex = 0;

		friend inline uint32 GetTypeHash(const FFunctionRegisterData& Data)
		{
			uint32 Result = HashCombine(GetTypeHash(Data.ReferenceNode), GetTypeHash(Data.MemoryType));
			return HashCombine(Result, GetTypeHash(Data.RegisterIndex));
		}

		bool operator==(const FFunctionRegisterData& Other) const
		{
			return ReferenceNode == Other.ReferenceNode && MemoryType == Other.MemoryType && RegisterIndex == Other.RegisterIndex;
		}
	};
	TMap<FFunctionRegisterData, FRigVMOperand> FunctionRegisterToOperand; 

	TArray<URigVMPin*> WatchedPins;
	
	TMap<ERigVMMemoryType, TArray<FRigVMPropertyPathDescription>> PropertyPathDescriptions;
	TMap<ERigVMMemoryType, TArray<FRigVMPropertyDescription>> PropertyDescriptions;

	FRigVMOperand AddProperty(ERigVMMemoryType InMemoryType, const FName& InName, const FString& InCPPType, UObject* InCPPTypeObject, const FString& InDefaultValue = FString());
	FRigVMOperand FindProperty(ERigVMMemoryType InMemoryType, const FName& InName);
	FRigVMPropertyDescription GetProperty(const FRigVMOperand& InOperand);
	int32 FindOrAddPropertyPath(const FRigVMOperand& InOperand, const FString& InHeadCPPType, const FString& InSegmentPath);
	const FProperty* GetPropertyForOperand(const FRigVMOperand& InOperand) const;
	TRigVMTypeIndex GetTypeIndexForOperand(const FRigVMOperand& InOperand) const;

	TSharedPtr<FRigVMParserAST> AST;
	
	struct FCopyOpInfo
	{
		FRigVMCopyOp Op;
		const FRigVMAssignExprAST* AssignExpr = nullptr;
		const FRigVMVarExprAST* SourceExpr = nullptr;
		const FRigVMVarExprAST* TargetExpr = nullptr;
	};

	// operators that have been delayed for injection into the bytecode
	TMap<FRigVMOperand, FCopyOpInfo> DeferredCopyOps;

	struct FLazyBlockInfo
	{
		FLazyBlockInfo()
			: StartInstruction(INDEX_NONE)
			, EndInstruction(INDEX_NONE)
			, bProcessed(false)
		{}

		TOptional<uint32> Hash;
		FString BlockCombinationName;
		FRigVMOperand ExecuteStateOperand;
		int32 StartInstruction;
		int32 EndInstruction;
		TArray<const FRigVMExprAST*> Expressions;
		TArray<uint64> RunInstructionsToUpdate;
		bool bProcessed;
	};

	TMap<uint32, TSharedPtr<FLazyBlockInfo>> LazyBlocks;
	TArray<uint32> LazyBlocksToProcess;
	TOptional<uint32> CurrentBlockHash;

	void ReportInfo(const FString& InMessage) const;
	void ReportWarning(const FString& InMessage) const;
	void ReportError(const FString& InMessage) const;

	template <typename FmtType, typename... Types>
	void ReportInfof(const FmtType& Fmt, Types... Args) const
	{
		ReportInfo(FString::Printf(Fmt, Args...));
	}

	template <typename FmtType, typename... Types>
	void ReportWarningf(const FmtType& Fmt, Types... Args) const
	{
		ReportWarning(FString::Printf(Fmt, Args...));
	}

	template <typename FmtType, typename... Types>
	void ReportErrorf(const FmtType& Fmt, Types... Args) const
	{
		ReportError(FString::Printf(Fmt, Args...));
	}

	FRigVMReportDelegate OriginalReportDelegate;
	void OverrideReportDelegate(bool& bEncounteredASTError, bool& bSurpressedASTError);
	void RemoveOverrideReportDelegate();

	void Clear()
	{
		if (VM)
		{
			if (Context)
			{
				VM->ClearMemory(*Context);
				VM->Reset(*Context);
			}
			else
			{
				VM->Reset_Internal();
			}
		}
		Graphs.Reset();
		if (PinPathToOperand)
		{
			PinPathToOperand->Reset();
		}
		ExprToOperand.Reset();
		ExprComplete.Reset();
		ExprToSkip.Reset();
		ProcessedLinks.Reset();
		IntegerLiterals.Reset();
		CachedProxiesWithSharedOperand.Reset();
		ProxyTargets.Reset();
		BranchInfos.Reset();
		FunctionRegisterToOperand.Reset();
		WatchedPins.Reset();
		PropertyDescriptions.Reset();
		PropertyPathDescriptions.Reset();
		DeferredCopyOps.Reset();
	}
};

DECLARE_DELEGATE_RetVal_OneParam(const FRigVMFunctionCompilationData*, FRigVMCompiler_GetFunctionCompilationData, const FRigVMGraphFunctionHeader& Header);

UCLASS(BlueprintType)
class RIGVMDEVELOPER_API URigVMCompiler : public UObject
{
	GENERATED_BODY()

public:

	URigVMCompiler();

	UPROPERTY()
	FRigVMCompileSettings Settings_DEPRECATED;

	UFUNCTION(BlueprintCallable, Category = FRigVMCompiler, meta=(DeprecatedFunction, DeprecationMessage="Compile is deprecated, use CompileVM with Context parameter."))
	bool Compile(TArray<URigVMGraph*> InGraphs, URigVMController* InController, URigVM* OutVM) { return false; }

	UFUNCTION(BlueprintCallable, Category = FRigVMCompiler)
	bool CompileVM(TArray<URigVMGraph*> InGraphs, URigVMController* InController, URigVM* OutVM, FRigVMExtendedExecuteContext& Context)
	{
		return Compile(Settings_DEPRECATED, InGraphs, InController, OutVM, Context, TArray<FRigVMExternalVariable>(), nullptr);
	}

	UE_DEPRECATED(5.3, "Please use Compile with Context param.")
	bool Compile(TArray<URigVMGraph*> InGraphs, URigVMController* InController, URigVM* OutVM, const TArray<FRigVMExternalVariable>& InExternalVariables, TMap<FString, FRigVMOperand>* OutOperands, TSharedPtr<FRigVMParserAST> InAST = TSharedPtr<FRigVMParserAST>(), FRigVMFunctionCompilationData* OutFunctionCompilationData = nullptr) { return false; }

	UE_DEPRECATED(5.4, "Please use CompileFunction with Settings param.")
	bool Compile(TArray<URigVMGraph*> InGraphs, URigVMController* InController, URigVM* OutVM, FRigVMExtendedExecuteContext& OutVMContext, const TArray<FRigVMExternalVariable>& InExternalVariables, TMap<FString, FRigVMOperand>* OutOperands, TSharedPtr<FRigVMParserAST> InAST = TSharedPtr<FRigVMParserAST>(), FRigVMFunctionCompilationData* OutFunctionCompilationData = nullptr);

	bool Compile(const FRigVMCompileSettings& InSettings, TArray<URigVMGraph*> InGraphs, URigVMController* InController, URigVM* OutVM, FRigVMExtendedExecuteContext& OutVMContext, const TArray<FRigVMExternalVariable>& InExternalVariables, TMap<FString, FRigVMOperand>* OutOperands, TSharedPtr<FRigVMParserAST> InAST = TSharedPtr<FRigVMParserAST>(), FRigVMFunctionCompilationData* OutFunctionCompilationData = nullptr);

	UE_DEPRECATED(5.3, "Please use CompileFunction with Context param.")
	bool CompileFunction(const URigVMLibraryNode* InLibraryNode, URigVMController* InController, FRigVMFunctionCompilationData* OutFunctionCompilationData) { return false; }

	UE_DEPRECATED(5.4, "Please use CompileFunction with Settings param.")
	bool CompileFunction(const URigVMLibraryNode* InLibraryNode, URigVMController* InController, FRigVMFunctionCompilationData* OutFunctionCompilationData, FRigVMExtendedExecuteContext& OutVMContext);

	bool CompileFunction(const FRigVMCompileSettings& InSettings, const URigVMLibraryNode* InLibraryNode, URigVMController* InController, const TArray<FRigVMExternalVariable>& InExternalVariables, FRigVMFunctionCompilationData* OutFunctionCompilationData, FRigVMExtendedExecuteContext& OutVMContext);

	FRigVMCompiler_GetFunctionCompilationData GetFunctionCompilationData;
	TMap<FString, const FRigVMFunctionCompilationData*> CompiledFunctions;

	static UScriptStruct* GetScriptStructForCPPType(const FString& InCPPType);
	static FString GetPinHash(const URigVMPin* InPin, const FRigVMVarExprAST* InVarExpr, bool bIsDebugValue = false, const URigVMLibraryNode* FunctionCompiling = nullptr, const FRigVMASTProxy& InPinProxy = FRigVMASTProxy());

	// follows assignment expressions to find the source ref counted containers
	// since ref counted containers are not copied for assignments.
	// this is currently only used for arrays.
	static const FRigVMVarExprAST* GetSourceVarExpr(const FRigVMExprAST* InExpr);

	void MarkDebugWatch(const FRigVMCompileSettings& InSettings, bool bRequired, URigVMPin* InPin, URigVM* OutVM, TMap<FString, FRigVMOperand>* OutOperands, TSharedPtr<FRigVMParserAST> InRuntimeAST);

private:

	const URigVMLibraryNode* CurrentCompilationFunction = nullptr;
	TSet<const URigVMLibraryNode*> FunctionCompilationStack;

	TArray<URigVMPin*> GetLinkedPins(URigVMPin* InPin, bool bInputs = true, bool bOutputs = true, bool bRecursive = true);
	uint16 GetElementSizeFromCPPType(const FString& InCPPType, UScriptStruct* InScriptStruct);

	static FString GetPinHashImpl(const URigVMPin* InPin, const FRigVMVarExprAST* InVarExpr, bool bIsDebugValue = false, const URigVMLibraryNode* FunctionCompiling = nullptr, const FRigVMASTProxy& InPinProxy = FRigVMASTProxy());

	bool TraverseExpression(const FRigVMExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	bool TraverseChildren(const FRigVMExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	bool TraverseBlock(const FRigVMBlockExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	bool TraverseEntry(const FRigVMEntryExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	bool TraverseCallExtern(const FRigVMCallExternExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	bool TraverseInlineFunction(const FRigVMInlineFunctionExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	bool TraverseNoOp(const FRigVMNoOpExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	bool TraverseVar(const FRigVMVarExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	bool TraverseLiteral(const FRigVMVarExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	bool TraverseExternalVar(const FRigVMExternalVarExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	bool TraverseAssign(const FRigVMAssignExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	bool TraverseCopy(const FRigVMCopyExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	bool TraverseCachedValue(const FRigVMCachedValueExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	bool TraverseExit(const FRigVMExitExprAST* InExpr, FRigVMCompilerWorkData& WorkData);
	bool TraverseInvokeEntry(const FRigVMInvokeEntryExprAST* InExpr, FRigVMCompilerWorkData& WorkData);

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

	FRigVMOperand FindOrAddRegister(const FRigVMVarExprAST* InVarExpr, FRigVMCompilerWorkData& WorkData, bool bIsDebugValue = false);
	const FRigVMCompilerWorkData::FRigVMASTProxyArray& FindProxiesWithSharedOperand(const FRigVMVarExprAST* InVarExpr, FRigVMCompilerWorkData& WorkData);

	static FString GetPinNameWithDirectionPrefix(const URigVMPin* Pin);
	static int32 GetOperandFunctionInterfaceParameterIndex(const TArray<FString>& OperandsPinNames, const FRigVMFunctionCompilationData* FunctionCompilationData, const FRigVMOperand& Operand);

	bool ValidateNode(const FRigVMCompileSettings& InSettings, URigVMNode* InNode, bool bCheck = true);
	
	void ReportInfo(const FRigVMCompileSettings& InSettings, const FString& InMessage);
	void ReportWarning(const FRigVMCompileSettings& InSettings, const FString& InMessage);
	void ReportError(const FRigVMCompileSettings& InSettings, const FString& InMessage);

	template <typename FmtType, typename... Types>
	void ReportInfof(const FRigVMCompileSettings& InSettings, const FmtType& Fmt, Types... Args)
	{
		ReportInfo(InSettings, FString::Printf(Fmt, Args...));
	}

	template <typename FmtType, typename... Types>
	void ReportWarningf(const FRigVMCompileSettings& InSettings, const FmtType& Fmt, Types... Args)
	{
		ReportWarning(InSettings, FString::Printf(Fmt, Args...));
	}

	template <typename FmtType, typename... Types>
	void ReportErrorf(const FRigVMCompileSettings& InSettings, const FmtType& Fmt, Types... Args)
	{
		ReportError(InSettings, FString::Printf(Fmt, Args...));
	}
	
	friend class FRigVMCompilerImportErrorContext;
	friend class FFunctionCompilationScope;
};

class RIGVMDEVELOPER_API FFunctionCompilationScope
{
public:
	FFunctionCompilationScope(URigVMCompiler* InCompiler, const URigVMLibraryNode* InLibraryNode)
		: Compiler(InCompiler), LibraryNode(InLibraryNode)
	{
		InCompiler->FunctionCompilationStack.Add(InLibraryNode);
	}

	~FFunctionCompilationScope()
	{
		Compiler->FunctionCompilationStack.Remove(LibraryNode);
	}

private:
	URigVMCompiler* Compiler;
	const URigVMLibraryNode* LibraryNode;
};