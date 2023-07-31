// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Math/Quat.h"
#include "Math/Transform.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "Misc/AssertionMacros.h"
#include "RigVMByteCode.h"
#include "RigVMCore/RigVMExternalVariable.h"
#include "RigVMCore/RigVMFunction.h"
#include "RigVMCore/RigVMMemoryCommon.h"
#include "RigVMCore/RigVMPropertyPath.h"
#include "RigVMExecuteContext.h"
#include "RigVMMemory.h"
#include "RigVMMemoryDeprecated.h"
#include "RigVMMemoryStorage.h"
#include "RigVMRegistry.h"
#include "RigVMStatistics.h"
#include "Templates/Function.h"
#include "UObject/Class.h"
#include "UObject/Field.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealNames.h"
#include "UObject/UnrealType.h"
#if WITH_EDITOR
#include "HAL/PlatformTime.h"
#include "RigVMDebugInfo.h"
#endif
#include "RigVM.generated.h"

class FArchive;
struct FFrame;
struct FRigVMDispatchFactory;

// Possible actions when the VM is halted at a breakpoint
UENUM()
enum class ERigVMBreakpointAction : uint8
{
	None,
	Resume,
	StepOver,
	StepInto,
	StepOut,
	Max UMETA(Hidden),
};


// The type of parameter for a VM
UENUM(BlueprintType)
enum class ERigVMParameterType : uint8
{
	Input,
	Output,
	Invalid
};

/**
 * The RigVMParameter define an input or output of the RigVM.
 * Parameters are mapped to work state memory registers and can be
 * used to set input parameters as well as retrieve output parameters.
 */
USTRUCT(BlueprintType)
struct RIGVM_API FRigVMParameter
{
	GENERATED_BODY()

public:

	FRigVMParameter()
		: Type(ERigVMParameterType::Invalid)
		, Name(NAME_None)
		, RegisterIndex(INDEX_NONE)
		, CPPType()
		, ScriptStruct(nullptr)
		, ScriptStructPath()
	{
	}

	void Serialize(FArchive& Ar);
	void Save(FArchive& Ar);
	void Load(FArchive& Ar);
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FRigVMParameter& P)
	{
		P.Serialize(Ar);
		return Ar;
	}
	
	// returns true if the parameter is valid
	bool IsValid() const { return Type != ERigVMParameterType::Invalid; }

	// returns the type of this parameter
	ERigVMParameterType GetType() const { return Type; }

	// returns the name of this parameters
	const FName& GetName() const { return Name; }

	// returns the register index of this parameter in the work memory
	int32 GetRegisterIndex() const { return RegisterIndex; }

	// returns the cpp type of the parameter
	FString GetCPPType() const { return CPPType; }
	
	// Returns the script struct used by this parameter (in case it is a struct)
	UScriptStruct* GetScriptStruct() const;

private:

	FRigVMParameter(ERigVMParameterType InType, const FName& InName, int32 InRegisterIndex, const FString& InCPPType, UScriptStruct* InScriptStruct)
		: Type(InType)
		, Name(InName)
		, RegisterIndex(InRegisterIndex)
		, CPPType(InCPPType)
		, ScriptStruct(InScriptStruct)
		, ScriptStructPath(NAME_None)
	{
		if (ScriptStruct)
		{
			ScriptStructPath = *ScriptStruct->GetPathName();
		}
	}

	UPROPERTY()
	ERigVMParameterType Type;

	UPROPERTY()
	FName Name;

	UPROPERTY()
	int32 RegisterIndex;

	UPROPERTY()
	FString CPPType;

	UPROPERTY(transient)
	TObjectPtr<UScriptStruct> ScriptStruct;

	UPROPERTY()
	FName ScriptStructPath;

	friend class URigVM;
	friend class URigVMCompiler;
};

/**
 * The RigVM is the main object for evaluating FRigVMByteCode instructions.
 * It combines the byte code, a list of required function pointers for 
 * execute instructions and required memory in one class.
 */
UCLASS(BlueprintType)
class RIGVM_API URigVM : public UObject
{
	GENERATED_BODY()

public:

	/** Bindable event for external objects to be notified when the VM reaches an Exit Operation */
	DECLARE_EVENT_OneParam(URigVM, FExecutionReachedExitEvent, const FName&);
#if WITH_EDITOR
	DECLARE_EVENT_ThreeParams(URigVM, FExecutionHaltedEvent, int32, UObject*, const FName&);
#endif

	URigVM();
	virtual ~URigVM();

	// UObject interface
	virtual void Serialize(FArchive& Ar);
	virtual void Save(FArchive& Ar);
	virtual void Load(FArchive& Ar);
	virtual void PostLoad() override;
#if WITH_EDITORONLY_DATA
	static void DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass);
#endif


	// returns true if this is a nativized VM
	virtual bool IsNativized() const { return false; }

	// returns a unique hash to compare VMs
	virtual uint32 GetVMHash() const;

	// returns the VM's matching nativized class if it exists
	UClass* GetNativizedClass(const TArray<FRigVMExternalVariable>& InExternalVariables = TArray<FRigVMExternalVariable>());
	
	// resets the container and maintains all memory
	virtual void Reset(bool IsIgnoringArchetypeRef = false);

	// resets the container and removes all memory
	virtual void Empty();

	// resets the container and clones the input VM
	virtual void CopyFrom(URigVM* InVM, bool bDeferCopy = false, bool bReferenceLiteralMemory = false, bool bReferenceByteCode = false, bool bCopyExternalVariables = false, bool bCopyDynamicRegisters = false);

	// sets the max array size allowed by this VM
	FORCEINLINE void SetRuntimeSettings(FRigVMRuntimeSettings InRuntimeSettings)
	{
		Context.SetRuntimeSettings(InRuntimeSettings);
	}

	// Initializes all execute ops and their memory.
	virtual bool Initialize(TArrayView<URigVMMemoryStorage*> Memory, TArrayView<void*> AdditionalArguments, bool bInitializeMemory = true);

	// Executes the VM.
	// You can optionally provide external memory to the execution
	// and provide optional additional operands.
	virtual bool Execute(TArrayView<URigVMMemoryStorage*> Memory, TArrayView<void*> AdditionalArguments, const FName& InEntryName = NAME_None);

	// Executes the VM.
	// You can optionally provide external memory to the execution
	// and provide optional additional operands.
	UFUNCTION(BlueprintCallable, Category = RigVM)
	virtual bool Execute(const FName& InEntryName = NAME_None);

	// Add a function for execute instructions to this VM.
	// Execute instructions can then refer to the function by index.
	UFUNCTION()
	virtual int32 AddRigVMFunction(UScriptStruct* InRigVMStruct, const FName& InMethodName);
	virtual int32 AddRigVMFunction(const FString& InFunctionName);

	// Returns the name of a function given its index
	UFUNCTION()
	virtual FString GetRigVMFunctionName(int32 InFunctionIndex) const;

	// Returns a memory storage by type
	virtual URigVMMemoryStorage* GetMemoryByType(ERigVMMemoryType InMemoryType, bool bCreateIfNeeded = true);
	
	// The default mutable work memory
	FORCEINLINE URigVMMemoryStorage* GetWorkMemory(bool bCreateIfNeeded = true) { return GetMemoryByType(ERigVMMemoryType::Work, bCreateIfNeeded); }

	// The default const literal memory
	FORCEINLINE URigVMMemoryStorage* GetLiteralMemory(bool bCreateIfNeeded = true) { return GetMemoryByType(ERigVMMemoryType::Literal, bCreateIfNeeded); }

	// The default debug watch memory
	FORCEINLINE URigVMMemoryStorage* GetDebugMemory(bool bCreateIfNeeded = true) { return GetMemoryByType(ERigVMMemoryType::Debug, bCreateIfNeeded); }

	// returns all memory storages as an array
	FORCEINLINE TArray<URigVMMemoryStorage*> GetLocalMemoryArray()
	{
		TArray<URigVMMemoryStorage*> LocalMemory;
		LocalMemory.Add(GetWorkMemory(true));
		LocalMemory.Add(GetLiteralMemory(true));
		LocalMemory.Add(GetDebugMemory(true));
		return LocalMemory;
	}

	// Removes all memory from this VM
	virtual void ClearMemory();

	UPROPERTY()
	TObjectPtr<URigVMMemoryStorage> WorkMemoryStorageObject;

	UPROPERTY()
	TObjectPtr<URigVMMemoryStorage> LiteralMemoryStorageObject;

	UPROPERTY()
	TObjectPtr<URigVMMemoryStorage> DebugMemoryStorageObject;

	TArray<FRigVMPropertyPathDescription> ExternalPropertyPathDescriptions;
	TArray<FRigVMPropertyPath> ExternalPropertyPaths;

	// The byte code of the VM
	UPROPERTY()
	FRigVMByteCode ByteCodeStorage;
	FRigVMByteCode* ByteCodePtr;
	FORCEINLINE FRigVMByteCode& GetByteCode() { return *ByteCodePtr; }
	FORCEINLINE const FRigVMByteCode& GetByteCode() const { return *ByteCodePtr; }

	// Returns the instructions of the VM
	virtual const FRigVMInstructionArray& GetInstructions();

	// Returns true if this VM's bytecode contains a given entry
	virtual bool ContainsEntry(const FName& InEntryName) const;

	// Returns the index of an entry
	virtual int32 FindEntry(const FName& InEntryName) const;

	// Returns a list of all valid entry names for this VM's bytecode
	virtual const TArray<FName>& GetEntryNames() const;

	// returns false if an entry can not be executed
	bool CanExecuteEntry(const FName& InEntryName) const;

#if WITH_EDITOR
	
	// Returns true if the given instruction has been visited during the last run
	FORCEINLINE bool WasInstructionVisitedDuringLastRun(int32 InIndex) const
	{
		return GetInstructionVisitedCount(InIndex) > 0;
	}

	// Returns the number of times an instruction has been hit
	FORCEINLINE int32 GetInstructionVisitedCount(int32 InIndex) const
	{
		if (InstructionVisitedDuringLastRun.IsValidIndex(InIndex))
		{
			return InstructionVisitedDuringLastRun[InIndex];
		}
		return 0;
	}

	// Returns accumulated cycles spent in an instruction during the last run
	// This requires bEnabledProfiling to be turned on in the runtime settings.
	// If there is no information available this function returns UINT64_MAX.
	FORCEINLINE uint64 GetInstructionCycles(int32 InIndex) const
	{
		if (InstructionCyclesDuringLastRun.IsValidIndex(InIndex))
		{
			return InstructionCyclesDuringLastRun[InIndex];
		}
		return UINT64_MAX;
	}

	// Returns accumulated duration of the instruction in microseconds during the last run
	// Note: this requires bEnabledProfiling to be turned on in the runtime settings.
	// If there is no information available this function returns -1.0.
	FORCEINLINE double GetInstructionMicroSeconds(int32 InIndex) const
	{
		const uint64 Cycles = GetInstructionCycles(InIndex);
		if(Cycles == UINT64_MAX)
		{
			return -1.0;
		}
		return double(Cycles) * FPlatformTime::GetSecondsPerCycle() * 1000.0 * 1000.0;
	}

	// Returns the order of all instructions during the last run
	FORCEINLINE const TArray<int32> GetInstructionVisitOrder() const { return InstructionVisitOrder; }

	FORCEINLINE const void SetFirstEntryEventInEventQueue(const FName& InFirstEventName) { FirstEntryEventInQueue = InFirstEventName; }

	bool ResumeExecution(TArrayView<URigVMMemoryStorage*> Memory, TArrayView<void*> AdditionalArguments, const FName& InEntryName = NAME_None);
	bool ResumeExecution();
#endif

	// Returns the parameters of the VM
	const TArray<FRigVMParameter>& GetParameters() const;

	// Returns a parameter given it's name
	FRigVMParameter GetParameterByName(const FName& InParameterName);

	FORCEINLINE_DEBUGGABLE FRigVMParameter AddParameter(ERigVMParameterType InType, const FName& InParameterName, const FName& InWorkMemoryPropertyName)
	{
		check(GetWorkMemory());

		if(ParametersNameMap.Contains(InParameterName))
		{
			return FRigVMParameter();
		}

		const FProperty* Property = GetWorkMemory()->FindPropertyByName(InWorkMemoryPropertyName);
		const int32 PropertyIndex = GetWorkMemory()->GetPropertyIndex(Property);

		UScriptStruct* Struct = nullptr;
		if(const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			Struct = StructProperty->Struct;
		}
		
		FRigVMParameter Parameter(InType, InParameterName, PropertyIndex, Property->GetCPPType(), Struct);
		ParametersNameMap.Add(Parameter.Name, Parameters.Add(Parameter));
		return Parameter;
	}

	// Retrieve the array size of the parameter
	FORCEINLINE int32 GetParameterArraySize(const FRigVMParameter& InParameter)
	{
		const int32 PropertyIndex = InParameter.GetRegisterIndex();
		const FProperty* Property = GetWorkMemory()->GetProperties()[PropertyIndex];
		const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property);
		if(ArrayProperty)
		{
			FScriptArrayHelper ArrayHelper(ArrayProperty, GetWorkMemory()->GetData<uint8>(PropertyIndex));
			return ArrayHelper.Num();
		}
		return 1;
	}

	// Retrieve the array size of the parameter
	int32 GetParameterArraySize(int32 InParameterIndex)
	{
		return GetParameterArraySize(Parameters[InParameterIndex]);
	}

	// Retrieve the array size of the parameter
	int32 GetParameterArraySize(const FName& InParameterName)
	{
		int32 ParameterIndex = ParametersNameMap.FindChecked(InParameterName);
		return GetParameterArraySize(ParameterIndex);
	}
	
	// Retrieve the value of a parameter
	template<class T>
	T GetParameterValue(const FRigVMParameter& InParameter, int32 InArrayIndex = 0, T DefaultValue = T{})
	{
		if (InParameter.GetRegisterIndex() != INDEX_NONE)
		{
			if(GetWorkMemory()->IsArray(InParameter.GetRegisterIndex()))
			{
				TArray<T>& Storage = *GetWorkMemory()->GetData<TArray<T>>(InParameter.GetRegisterIndex());
				if(Storage.IsValidIndex(InArrayIndex))
				{
					return Storage[InArrayIndex];
				}
			}
			else
			{
				return *GetWorkMemory()->GetData<T>(InParameter.GetRegisterIndex());
			}
			
			return *GetWorkMemory()->GetData<T>(InParameter.GetRegisterIndex());
		}
		return DefaultValue;
	}

	// Retrieve the value of a parameter given its index
	template<class T>
	T GetParameterValue(int32 InParameterIndex, int32 InArrayIndex = 0, T DefaultValue = T{})
	{
		return GetParameterValue<T>(Parameters[InParameterIndex], InArrayIndex, DefaultValue);
	}

	// Retrieve the value of a parameter given its name
	template<class T>
	T GetParameterValue(const FName& InParameterName, int32 InArrayIndex = 0, T DefaultValue = T{})
	{
		int32 ParameterIndex = ParametersNameMap.FindChecked(InParameterName);
		return GetParameterValue<T>(ParameterIndex, InArrayIndex, DefaultValue);
	}

	// Set the value of a parameter
	template<class T>
	void SetParameterValue(const FRigVMParameter& InParameter, const T& InNewValue, int32 InArrayIndex = 0)
	{
		if (InParameter.GetRegisterIndex() != INDEX_NONE)
		{
			if(GetWorkMemory()->IsArray(InParameter.GetRegisterIndex()))
			{
				TArray<T>& Storage = *GetWorkMemory()->GetData<TArray<T>>(InParameter.GetRegisterIndex());
				if(Storage.IsValidIndex(InArrayIndex))
				{
					Storage[InArrayIndex] = InNewValue;
				}
			}
			else
			{
				T& Storage = *GetWorkMemory()->GetData<T>(InParameter.GetRegisterIndex());
				Storage = InNewValue;
			}
		}
	}

	// Set the value of a parameter given its index
	template<class T>
	void SetParameterValue(int32 ParameterIndex, const T& InNewValue, int32 InArrayIndex = 0)
	{
		return SetParameterValue<T>(Parameters[ParameterIndex], InNewValue, InArrayIndex);
	}

	// Set the value of a parameter given its name
	template<class T>
	void SetParameterValue(const FName& InParameterName, const T& InNewValue, int32 InArrayIndex = 0)
	{
		int32 ParameterIndex = ParametersNameMap.FindChecked(InParameterName);
		return SetParameterValue<T>(ParameterIndex, InNewValue, InArrayIndex);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	bool GetParameterValueBool(const FName& InParameterName, int32 InArrayIndex = 0)
	{
		return GetParameterValue<bool>(InParameterName, InArrayIndex);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	float GetParameterValueFloat(const FName& InParameterName, int32 InArrayIndex = 0)
	{
		return GetParameterValue<float>(InParameterName, InArrayIndex);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	double GetParameterValueDouble(const FName& InParameterName, int32 InArrayIndex = 0)
	{
		return GetParameterValue<double>(InParameterName, InArrayIndex);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	int32 GetParameterValueInt(const FName& InParameterName, int32 InArrayIndex = 0)
	{
		return GetParameterValue<int32>(InParameterName, InArrayIndex);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	FName GetParameterValueName(const FName& InParameterName, int32 InArrayIndex = 0)
	{
		return GetParameterValue<FName>(InParameterName, InArrayIndex);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	FString GetParameterValueString(const FName& InParameterName, int32 InArrayIndex = 0)
	{
		return GetParameterValue<FString>(InParameterName, InArrayIndex);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	FVector2D GetParameterValueVector2D(const FName& InParameterName, int32 InArrayIndex = 0)
	{
		return GetParameterValue<FVector2D>(InParameterName, InArrayIndex, FVector2D::ZeroVector);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	FVector GetParameterValueVector(const FName& InParameterName, int32 InArrayIndex = 0)
	{
		return GetParameterValue<FVector>(InParameterName, InArrayIndex, FVector::ZeroVector);	// LWC_TODO: Store double FVector
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	FQuat GetParameterValueQuat(const FName& InParameterName, int32 InArrayIndex = 0)
	{
		return GetParameterValue<FQuat>(InParameterName, InArrayIndex, FQuat::Identity);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	FTransform GetParameterValueTransform(const FName& InParameterName, int32 InArrayIndex = 0)
	{
		return GetParameterValue<FTransform>(InParameterName, InArrayIndex, FTransform::Identity);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	void SetParameterValueBool(const FName& InParameterName, bool InValue, int32 InArrayIndex = 0)
	{
		SetParameterValue<bool>(InParameterName, InValue, InArrayIndex);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	void SetParameterValueFloat(const FName& InParameterName, float InValue, int32 InArrayIndex = 0)
	{
		SetParameterValue<float>(InParameterName, InValue, InArrayIndex);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	void SetParameterValueDouble(const FName& InParameterName, double InValue, int32 InArrayIndex = 0)
	{
		SetParameterValue<double>(InParameterName, InValue, InArrayIndex);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	void SetParameterValueInt(const FName& InParameterName, int32 InValue, int32 InArrayIndex = 0)
	{
		SetParameterValue<int32>(InParameterName, InValue, InArrayIndex);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	void SetParameterValueName(const FName& InParameterName, const FName& InValue, int32 InArrayIndex = 0)
	{
		SetParameterValue<FName>(InParameterName, InValue, InArrayIndex);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	void SetParameterValueString(const FName& InParameterName, const FString& InValue, int32 InArrayIndex = 0)
	{
		SetParameterValue<FString>(InParameterName, InValue, InArrayIndex);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	void SetParameterValueVector2D(const FName& InParameterName, const FVector2D& InValue, int32 InArrayIndex = 0)
	{
		SetParameterValue<FVector2D>(InParameterName, InValue, InArrayIndex);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	void SetParameterValueVector(const FName& InParameterName, const FVector& InValue, int32 InArrayIndex = 0)
	{
		SetParameterValue<FVector>(InParameterName, InValue, InArrayIndex);	// LWC_TODO: Store double FVector
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	void SetParameterValueQuat(const FName& InParameterName, const FQuat& InValue, int32 InArrayIndex = 0)
	{
		SetParameterValue<FQuat>(InParameterName, InValue, InArrayIndex);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	void SetParameterValueTransform(const FName& InParameterName, const FTransform& InValue, int32 InArrayIndex = 0)
	{
		SetParameterValue<FTransform>(InParameterName, InValue, InArrayIndex);
	}

	// Returns the external variables of the VM
	void ClearExternalVariables() { ExternalVariables.Reset(); }

	// Returns the external variables of the VM
	const TArray<FRigVMExternalVariable>& GetExternalVariables() const { return ExternalVariables; }

	// Returns an external variable given it's name
	FRigVMExternalVariable GetExternalVariableByName(const FName& InExternalVariableName);

	// Adds a new external / unowned variable to the VM
	FORCEINLINE_DEBUGGABLE FRigVMOperand AddExternalVariable(const FRigVMExternalVariable& InExternalVariable)
	{
		int32 VariableIndex = ExternalVariables.Add(InExternalVariable);
		return FRigVMOperand(ERigVMMemoryType::External, VariableIndex);
	}

	void SetPropertyValueFromString(const FRigVMOperand& InOperand, const FString& InDefaultValue);

	// returns the statistics information
	UFUNCTION(BlueprintPure, Category = "RigVM", meta=(DeprecatedFunction))
	FRigVMStatistics GetStatistics() const
	{
		FRigVMStatistics Statistics;
		if(LiteralMemoryStorageObject)
		{
			Statistics.LiteralMemory = LiteralMemoryStorageObject->GetStatistics();
		}
		if(WorkMemoryStorageObject)
		{
			Statistics.WorkMemory = WorkMemoryStorageObject->GetStatistics();
		}

		Statistics.ByteCode = ByteCodePtr->GetStatistics();
		Statistics.BytesForCaching = FirstHandleForInstruction.GetAllocatedSize() + CachedMemoryHandles.GetAllocatedSize();
		Statistics.BytesForCDO =
			Statistics.LiteralMemory.TotalBytes +
			Statistics.WorkMemory.TotalBytes +
			Statistics.ByteCode.DataBytes +
			Statistics.BytesForCaching;
		
		Statistics.BytesPerInstance =
			Statistics.WorkMemory.DataBytes +
			Statistics.BytesForCaching;

		return Statistics;
	}


#if WITH_EDITOR
	// returns the instructions as text, OperandFormatFunction is an optional argument that allows you to override how operands are displayed, for example, see SControlRigStackView::PopulateStackView 
	TArray<FString> DumpByteCodeAsTextArray(const TArray<int32> & InInstructionOrder = TArray<int32>(), bool bIncludeLineNumbers = true, TFunction<FString(const FString& RegisterName, const FString& RegisterOffsetName)> OperandFormatFunction = nullptr);
	FString DumpByteCodeAsText(const TArray<int32>& InInstructionOrder = TArray<int32>(), bool bIncludeLineNumbers = true);
#endif

#if WITH_EDITOR
	// FormatFunction is an optional argument that allows you to override how operands are displayed, for example, see SControlRigStackView::PopulateStackView
	FString GetOperandLabel(const FRigVMOperand & InOperand, TFunction<FString(const FString& RegisterName, const FString& RegisterOffsetName)> FormatFunction = nullptr);
#endif

	FExecutionReachedExitEvent& ExecutionReachedExit() { return OnExecutionReachedExit; }
#if WITH_EDITOR
	FExecutionHaltedEvent& ExecutionHalted() { return OnExecutionHalted; }

	void SetDebugInfo(FRigVMDebugInfo* InDebugInfo) { DebugInfo = InDebugInfo; }

	const FRigVMBreakpoint& GetHaltedAtBreakpoint() const { return HaltedAtBreakpoint; }

	void SetBreakpointAction(const ERigVMBreakpointAction& Action) { CurrentBreakpointAction = Action; }
#endif

	uint32 GetNumExecutions() const { return NumExecutions; }
	const FRigVMExtendedExecuteContext& GetContext() const { return Context; }

private:

	void ResolveFunctionsIfRequired();
	void RefreshInstructionsIfRequired();
public:
	void InvalidateCachedMemory();
	
private:
	void CacheMemoryHandlesIfRequired(TArrayView<URigVMMemoryStorage*> InMemory);
	void RebuildByteCodeOnLoad();

	UPROPERTY(transient)
	FRigVMInstructionArray Instructions;

protected:
	UPROPERTY(transient)
	FRigVMExtendedExecuteContext Context;

	FORCEINLINE void SetInstructionIndex(uint16 InInstructionIndex) { Context.PublicData.InstructionIndex = InInstructionIndex; }

private:
	UPROPERTY(transient)
	uint32 NumExecutions;

#if WITH_EDITOR
	FRigVMDebugInfo* DebugInfo;
	FRigVMBreakpoint HaltedAtBreakpoint;
	int32 HaltedAtBreakpointHit;
	ERigVMBreakpointAction CurrentBreakpointAction;

	bool ShouldHaltAtInstruction(const FName& InEventName, const uint16 InstructionIndex);
#endif

	UPROPERTY()
	TArray<FName> FunctionNamesStorage;
	TArray<FName>* FunctionNamesPtr;
	FORCEINLINE TArray<FName>& GetFunctionNames() { return *FunctionNamesPtr; }
	FORCEINLINE const TArray<FName>& GetFunctionNames() const { return *FunctionNamesPtr; }

	TArray<FRigVMFunctionPtr> FunctionsStorage;
	TArray<FRigVMFunctionPtr>* FunctionsPtr;
	FORCEINLINE TArray<FRigVMFunctionPtr>& GetFunctions() { return *FunctionsPtr; }
	FORCEINLINE const TArray<FRigVMFunctionPtr>& GetFunctions() const { return *FunctionsPtr; }

	TArray<const FRigVMDispatchFactory*> FactoriesStorage;
	TArray<const FRigVMDispatchFactory*>* FactoriesPtr;
	FORCEINLINE TArray<const FRigVMDispatchFactory*>& GetFactories() { return *FactoriesPtr; }
	FORCEINLINE const TArray<const FRigVMDispatchFactory*>& GetFactories() const { return *FactoriesPtr; }

	UPROPERTY()
	TArray<FRigVMParameter> Parameters;

	UPROPERTY()
	TMap<FName, int32> ParametersNameMap;

	TArray<uint32> FirstHandleForInstruction;
	TArray<FRigVMMemoryHandle> CachedMemoryHandles;
	// changes to the layout of cached memory array should be reflected in GetContainerIndex()
	TArray<URigVMMemoryStorage*> CachedMemory;
	TArray<FRigVMExternalVariable> ExternalVariables;

	// this function should be kept in sync with FRigVMOperand::GetContainerIndex()
	static int32 GetContainerIndex(ERigVMMemoryType InType)
	{
		if(InType == ERigVMMemoryType::External)
		{
			return (int32)ERigVMMemoryType::Work;
		}
		
		if(InType == ERigVMMemoryType::Debug)
		{
			return 2;
		}
		return (int32)InType;
	}
	
#if WITH_EDITOR

	// stores the number of times each instruction was visited
	TArray<int32> InstructionVisitedDuringLastRun;
	TArray<uint64> InstructionCyclesDuringLastRun;
	TArray<int32> InstructionVisitOrder;
	
	// Control Rig can run multiple events per evaluation, such as the Backward&Forward Solve Mode,
	// store the first event such that we know when to reset data for a new round of rig evaluation
	FName FirstEntryEventInQueue;
#endif
	
	// debug watch register memory needs to be cleared for each execution
	void ClearDebugMemory();
	
	void CacheSingleMemoryHandle(int32 InHandleIndex, const FRigVMOperand& InArg, bool bForExecute = false);

	FORCEINLINE_DEBUGGABLE void CopyOperandForDebuggingIfNeeded(const FRigVMOperand& InArg, const FRigVMMemoryHandle& InHandle)
	{
#if WITH_EDITOR
		const FRigVMOperand KeyOperand(InArg.GetMemoryType(), InArg.GetRegisterIndex()); // no register offset
		if(const TArray<FRigVMOperand>* DebugOperandsPtr = OperandToDebugRegisters.Find(KeyOperand))
		{
			const TArray<FRigVMOperand>& DebugOperands = *DebugOperandsPtr;
			for(const FRigVMOperand& DebugOperand : DebugOperands)
			{
				CopyOperandForDebuggingImpl(InArg, InHandle, DebugOperand);
			}
		}
#endif
	}

	bool ValidateAllOperandsDuringLoad();

	void CopyOperandForDebuggingImpl(const FRigVMOperand& InArg, const FRigVMMemoryHandle& InHandle, const FRigVMOperand& InDebugOperand);

	FRigVMCopyOp GetCopyOpForOperands(const FRigVMOperand& InSource, const FRigVMOperand& InTarget);
	void RefreshExternalPropertyPaths();
	static void CopyArray(FScriptArrayHelper& TargetHelper, FRigVMMemoryHandle& TargetHandle, FScriptArrayHelper& SourceHelper, FRigVMMemoryHandle& SourceHandle);
	
	TMap<FRigVMOperand, TArray<FRigVMOperand>> OperandToDebugRegisters;

	int32 ExecutingThreadId;
	
protected:

	struct FEntryExecuteGuard
	{
	public:
		FEntryExecuteGuard(TArray<int32>& InOutStack, int32 InEntryIndex)
		: Stack(InOutStack)
		{
			Stack.Push(InEntryIndex);
		}

		~FEntryExecuteGuard()
		{
			Stack.Pop();
		}
	private:

		TArray<int32>& Stack;
	};
	
	TArray<int32> EntriesBeingExecuted;
	
private:
	
	mutable TArray<FName> EntryNames;

	UPROPERTY(transient)
	TObjectPtr<URigVM> DeferredVMToCopy;

	void CopyDeferredVMIfRequired();

	FExecutionReachedExitEvent OnExecutionReachedExit;
#if WITH_EDITOR
	FExecutionHaltedEvent OnExecutionHalted;
#endif

	friend class URigVMCompiler;
	friend struct FRigVMCodeGenerator;
};
