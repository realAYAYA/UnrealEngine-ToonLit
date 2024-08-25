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
#include "RigVMCore/RigVMMemoryStorage.h"
#include "RigVMCore/RigVMMemoryStorageStruct.h"
#include "RigVMExecuteContext.h"
#include "RigVMMemory.h"
#include "RigVMMemoryDeprecated.h"
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
class URigVMHost;
struct FFrame;
struct FRigVMDispatchFactory;

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

	friend FArchive& operator<<(FArchive& Ar, FRigVMParameter& P)
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
	void CopyDataForSerialization(URigVM* InVM);

	virtual void PostLoad() override;

#if WITH_EDITORONLY_DATA
	static void DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass);
#endif

	bool IsContextValidForExecution(FRigVMExtendedExecuteContext& Context) const;

	// returns true if this is a nativized VM
	virtual bool IsNativized() const { return false; }

	// Stores the current VM hash
	virtual void SetVMHash(uint32 InVMHash);

	// returns the cached VM hash
	virtual uint32 GetVMHash() const;

	// Generates a unique hash to compare VMs
	virtual uint32 ComputeVMHash() const;

	UE_DEPRECATED(5.3, "Please, use GetNativizedClass with FRigVMExternalVariableDef array.")
	UClass* GetNativizedClass(const TArray<FRigVMExternalVariable>& InExternalVariables) { return nullptr; }

	// returns the VM's matching nativized class if it exists
	UClass* GetNativizedClass(const TArray<FRigVMExternalVariableDef>& InExternalVariables = TArray<FRigVMExternalVariableDef>());
	
	UE_DEPRECATED(5.4, "Please, use Reset with Context param")
	virtual void Reset(bool IsIgnoringArchetypeRef = false) {}

	// resets the container and maintains all memory
	virtual void Reset(FRigVMExtendedExecuteContext& Context);

	UE_DEPRECATED(5.3, "Please, use Empty with Context param")
	virtual void Empty() {}

	// resets the container and removes all memory
	virtual void Empty(FRigVMExtendedExecuteContext& Context);

	UE_DEPRECATED(5.4, "CopyFrom has been deprecated")
	virtual void CopyFrom(URigVM* InVM, bool bDeferCopy = false, bool bReferenceLiteralMemory = false, bool bReferenceByteCode = false, bool bCopyExternalVariables = false, bool bCopyDynamicRegisters = false) {}
	
	// sets the max array size allowed by this VM
	UE_DEPRECATED(5.3, "Please, use Context.SetRuntimeSettings")
	void SetRuntimeSettings(FRigVMRuntimeSettings InRuntimeSettings) {}

	UE_DEPRECATED(5.3, "Please, use Initialize with Context param")
	virtual bool Initialize(TArrayView<URigVMMemoryStorage*> Memory) { return false; }

	UE_DEPRECATED(5.4, "Please, use Initialize with just Context param")
	virtual bool Initialize(FRigVMExtendedExecuteContext& Context, TArrayView<URigVMMemoryStorage*> Memory) { return false; }

	// Prepares caches and memory for execution
	virtual bool Initialize(FRigVMExtendedExecuteContext& Context);

	// Initializes cached memory handles and optionally copies work memory from the CDO to the Context
	virtual bool InitializeInstance(FRigVMExtendedExecuteContext& Context, bool bCopyMemory = true);

	UE_DEPRECATED(5.3, "Please, use Execute with Context param")
	virtual ERigVMExecuteResult Execute(TArrayView<URigVMMemoryStorage*> Memory, const FName& InEntryName = NAME_None) { return ERigVMExecuteResult::Failed; }

	UE_DEPRECATED(5.4, "Please, use ExecuteVM with Context param")
	virtual ERigVMExecuteResult Execute(FRigVMExtendedExecuteContext& Context, TArrayView<URigVMMemoryStorage*> Memory, const FName& InEntryName = NAME_None) { return ERigVMExecuteResult::Failed; }

	UE_DEPRECATED(5.3, "Please, use Execute with Context param")
	virtual bool Execute(const FName& InEntryName = NAME_None) { return false; }

	// Executes the VM.
	// You can optionally provide optional additional operands.
	virtual ERigVMExecuteResult ExecuteVM(FRigVMExtendedExecuteContext& Context, const FName& InEntryName = NAME_None);

	UE_DEPRECATED(5.4, "Please, use ExecuteVM with Context param")
	UFUNCTION(BlueprintCallable, Category = RigVM, meta = (DeprecatedFunction, DeprecationMessage = "This function has been deprecated and it is no longer supported."))
	virtual bool Execute(FRigVMExtendedExecuteContext& Context, const FName& InEntryName = NAME_None) { return false; }

	UE_DEPRECATED(5.3, "Please, use ExecuteLazyBranch with Context param")
	ERigVMExecuteResult ExecuteLazyBranch(const FRigVMBranchInfo& InBranchToRun) { return ERigVMExecuteResult::Failed; }

	// Executes a single branch on the VM. We assume that the memory is already set correctly at this point.
	ERigVMExecuteResult ExecuteBranch(FRigVMExtendedExecuteContext& Context, const FRigVMBranchInfo& InBranchToRun);

private:
	ERigVMExecuteResult ExecuteInstructions(FRigVMExtendedExecuteContext& Context, int32 InFirstInstruction, int32 InLastInstruction);

public:

	// Add a function for execute instructions to this VM.
	// Execute instructions can then refer to the function by index.
	UFUNCTION()
	virtual int32 AddRigVMFunction(UScriptStruct* InRigVMStruct, const FName& InMethodName);
	virtual int32 AddRigVMFunction(const FString& InFunctionName);

	// Returns the name of a function given its index
	UFUNCTION()
	virtual FString GetRigVMFunctionName(int32 InFunctionIndex) const;

	UE_DEPRECATED(5.4, "GetMemoryByType has been deprecated from the VM. Please, use GetWorkMemory from VMHost or the version with a Context param")
	virtual URigVMMemoryStorage* GetMemoryByType(ERigVMMemoryType InMemoryType, bool bCreateIfNeeded = true) { return nullptr; }

	// Returns a memory storage by type
	virtual FRigVMMemoryStorageStruct* GetMemoryByType(FRigVMExtendedExecuteContext& Context, ERigVMMemoryType InMemoryType);
	virtual const FRigVMMemoryStorageStruct* GetMemoryByType(const FRigVMExtendedExecuteContext& Context, ERigVMMemoryType InMemoryType) const;

	UE_DEPRECATED(5.4, "This version of GetLiteralMemory has been deprecated from the VM. Please, use GetLiteralMemory from Host or version with no parameter.")
	URigVMMemoryStorage* GetLiteralMemory(bool bCreateIfNeeded) { return nullptr; }

	// The default const literal memory
	FRigVMMemoryStorageStruct* GetLiteralMemory()
	{
		return &LiteralMemoryStorage;
	}
	const FRigVMMemoryStorageStruct* GetLiteralMemory() const
	{
		return &LiteralMemoryStorage;
	}

	UE_DEPRECATED(5.4, "GetWorkMemory has been deprecated from the VM. Please, use GetWorkMemory from VMHost or the version with a Context param")
	URigVMMemoryStorage* GetWorkMemory(bool bCreateIfNeeded = true) { return nullptr; }

	// The instance mutable work memory the VM will use to execute
	FRigVMMemoryStorageStruct* GetWorkMemory(FRigVMExtendedExecuteContext& Context)
	{
		return GetMemoryByType(Context, ERigVMMemoryType::Work);
	}
	const FRigVMMemoryStorageStruct* GetWorkMemory(const FRigVMExtendedExecuteContext& Context) const
	{
		return GetMemoryByType(Context, ERigVMMemoryType::Work);
	}

	UE_DEPRECATED(5.4, "GetDebugMemory has been deprecated from the VM. Please, use GetDebugMemory from VMHost or the version with a Context param")
	URigVMMemoryStorage* GetDebugMemory(bool bCreateIfNeeded = true) { return nullptr; }

	// The instance debug watch memory
	FRigVMMemoryStorageStruct* GetDebugMemory(FRigVMExtendedExecuteContext& Context)
	{
		return GetMemoryByType(Context, ERigVMMemoryType::Debug);
	}
	const FRigVMMemoryStorageStruct* GetDebugMemory(const FRigVMExtendedExecuteContext& Context) const
	{
		return GetMemoryByType(Context, ERigVMMemoryType::Debug);
	}

	UE_DEPRECATED(5.4, "Please, use GetInstanceMemoryArray with a Context param")
	TArray<URigVMMemoryStorage*> GetLocalMemoryArray() { return TArray<URigVMMemoryStorage*>(); }

	// Returns the VM Literal memory and context instanced memory.
	TArray<FRigVMMemoryStorageStruct*> GetInstanceMemory(FRigVMExtendedExecuteContext& Context)
	{
		TArray<FRigVMMemoryStorageStruct*> LocalMemory;
		LocalMemory.Add(GetWorkMemory(Context));
		LocalMemory.Add(GetLiteralMemory());
		LocalMemory.Add(GetDebugMemory(Context));
		return LocalMemory;
	}

	// The default non mutable reference literal memory (generated by the compiler and copied to the instances as initial state)
	const FRigVMMemoryStorageStruct& GetDefaultLiteralMemory() const
	{
		return LiteralMemoryStorage;
	}
	FRigVMMemoryStorageStruct& GetDefaultLiteralMemory()
	{
		return LiteralMemoryStorage;
	}

	// The default non mutable reference work memory (generated by the compiler and copied to the instances as initial state)
	const FRigVMMemoryStorageStruct& GetDefaultWorkMemory() const
	{
		return DefaultWorkMemoryStorage;
	}
	FRigVMMemoryStorageStruct& GetDefaultWorkMemory()
	{
		return DefaultWorkMemoryStorage;
	}

	// The default non mutable reference debug memory (generated by the compiler and copied to the instances as initial state)
	const FRigVMMemoryStorageStruct& GetDefaultDebugMemory() const
	{
		return DefaultDebugMemoryStorage;
	}
	FRigVMMemoryStorageStruct& GetDefaultDebugMemory()
	{
		return DefaultDebugMemoryStorage;
	}

	// Generates the Default memory and copy it to the Context if required by the type of memory
	virtual void GenerateMemoryType(FRigVMExtendedExecuteContext& Context, ERigVMMemoryType InMemoryType, const TArray<FRigVMPropertyDescription>* InProperties);

	// Used by the compiler and EngineTests to generate the default VM memory storages
	virtual void GenerateDefaultMemoryType(ERigVMMemoryType InMemoryType, const TArray<FRigVMPropertyDescription>* InProperties);

	// Returns a default memory storage by type
	virtual const FRigVMMemoryStorageStruct* GetDefaultMemoryByType(ERigVMMemoryType InMemoryType) const;
	virtual FRigVMMemoryStorageStruct* GetDefaultMemoryByType(ERigVMMemoryType InMemoryType);


public:

	UE_DEPRECATED(5.4, "Please, use GetLocalMemoryArray with a Context param")
	virtual void ClearMemory() {}

	virtual void ClearMemory(FRigVMExtendedExecuteContext& Context);

	UPROPERTY()
	FRigVMMemoryStorageStruct LiteralMemoryStorage;

	UPROPERTY()
	FRigVMMemoryStorageStruct DefaultWorkMemoryStorage;

	UPROPERTY()
	FRigVMMemoryStorageStruct DefaultDebugMemoryStorage;

#if WITH_EDITORONLY_DATA
	// Deprecated 5.4
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Please, use DefaultWorkMemoryStorage for compiling and WorkMemoryStorage in the ExtendedExecuteContext for intance execution"))
	TObjectPtr<URigVMMemoryStorage> WorkMemoryStorageObject_DEPRECATED;
#endif

#if WITH_EDITORONLY_DATA
	// Deprecated 5.4
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Please, use LiteralMemoryStorage"))
	TObjectPtr<URigVMMemoryStorage> LiteralMemoryStorageObject_DEPRECATED;
#endif

#if WITH_EDITORONLY_DATA
	// Deprecated 5.4, 
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Please use DefaultDebugMemoryStorage for compiling and DebugMemoryStorage in the ExtendedExecuteContext for intance execution"))
	TObjectPtr<URigVMMemoryStorage> DebugMemoryStorageObject_DEPRECATED;
#endif

	TArray<FRigVMPropertyPathDescription> ExternalPropertyPathDescriptions;
	TArray<FRigVMPropertyPath> ExternalPropertyPaths;

	// The byte code of the VM
	UPROPERTY()
	FRigVMByteCode ByteCodeStorage;
	FRigVMByteCode* ByteCodePtr;
	FRigVMByteCode& GetByteCode() { return *ByteCodePtr; }
	const FRigVMByteCode& GetByteCode() const { return *ByteCodePtr; }

	// Returns the instructions of the VM
	virtual const FRigVMInstructionArray& GetInstructions();

	// Returns true if this VM's bytecode contains a given entry
	virtual bool ContainsEntry(const FName& InEntryName) const;

	// Returns the index of an entry
	virtual int32 FindEntry(const FName& InEntryName) const;

	// Returns a list of all valid entry names for this VM's bytecode
	virtual const TArray<FName>& GetEntryNames() const;

	UE_DEPRECATED(5.3, "Please, use CanExecuteEntry with Context param")
	bool CanExecuteEntry(const FName& InEntryName, bool bLogErrorForMissingEntry = true) const { return false; }

	// returns false if an entry can not be executed
	bool CanExecuteEntry(const FRigVMExtendedExecuteContext& Context, const FName& InEntryName, bool bLogErrorForMissingEntry = true) const;

#if WITH_EDITOR
	
	UE_DEPRECATED(5.3, "Please, use WasInstructionVisitedDuringLastRun with Context param")
	bool WasInstructionVisitedDuringLastRun(int32 InIndex) const { return false; }

	// Returns true if the given instruction has been visited during the last run
	bool WasInstructionVisitedDuringLastRun(const FRigVMExtendedExecuteContext& Context, int32 InIndex) const
	{
		return GetInstructionVisitedCount(Context, InIndex) > 0;
	}

	UE_DEPRECATED(5.3, "Please, use GetInstructionVisitedCount with Context param")
	int32 GetInstructionVisitedCount(int32 InIndex) const { return 0; }

	// Returns the number of times an instruction has been hit
	int32 GetInstructionVisitedCount(const FRigVMExtendedExecuteContext& Context, int32 InIndex) const
	{
		if (const FRigVMInstructionVisitInfo* InstructionVisitInfo = Context.GetRigVMInstructionVisitInfo())
		{
			return InstructionVisitInfo->GetInstructionVisitedCountDuringLastRun(InIndex);
		}
		return 0;
	}

	UE_DEPRECATED(5.3, "Please, use GetInstructionCycles with Context param")
	uint64 GetInstructionCycles(int32 InIndex) const { return 0LL; }

	// Returns accumulated cycles spent in an instruction during the last run
	// This requires bEnabledProfiling to be turned on in the runtime settings.
	// If there is no information available this function returns UINT64_MAX.
	uint64 GetInstructionCycles(const FRigVMExtendedExecuteContext& Context, int32 InIndex) const
	{
		if (const FRigVMProfilingInfo* RigVMProfilingInfo = Context.GetRigVMProfilingInfo())
		{
			return RigVMProfilingInfo->GetInstructionCyclesDuringLastRun(InIndex);
		}
		return UINT64_MAX;
	}

	UE_DEPRECATED(5.3, "Please, use GetInstructionMicroSeconds with Context param")
	double GetInstructionMicroSeconds(int32 InIndex) const { return 0.0; }

	// Returns accumulated duration of the instruction in microseconds during the last run
	// Note: this requires bEnabledProfiling to be turned on in the runtime settings.
	// If there is no information available this function returns -1.0.
	double GetInstructionMicroSeconds(const FRigVMExtendedExecuteContext& Context, int32 InIndex) const
	{
		const uint64 Cycles = GetInstructionCycles(Context, InIndex);
		if(Cycles == UINT64_MAX)
		{
			return -1.0;
		}
		return double(Cycles) * FPlatformTime::GetSecondsPerCycle() * 1000.0 * 1000.0;
	}

	UE_DEPRECATED(5.3, "Please, use GetInstructionVisitOrder with Context param")
	const TArray<int32> GetInstructionVisitOrder() const { return TArray<int32>(); }

	// Returns the order of all instructions during the last run
	const TArray<int32> GetInstructionVisitOrder(const FRigVMExtendedExecuteContext& Context) const
	{
		if (const FRigVMInstructionVisitInfo* InstructionVisitInfo = Context.GetRigVMInstructionVisitInfo())
		{
			return InstructionVisitInfo->GetInstructionVisitOrder();
		}

		return TArray<int32>();
	}

	bool ResumeExecution(FRigVMExtendedExecuteContext& Context, const FName& InEntryName = NAME_None);

	UE_DEPRECATED(5.3, "Please, use ResumeExecution with Context param")
	bool ResumeExecution() { return false; }
	UE_DEPRECATED(5.3, "Please, use ResumeExecution with Context param")
	bool ResumeExecution(TArrayView<URigVMMemoryStorage*> Memory, const FName& InEntryName = NAME_None) { return false; }
	UE_DEPRECATED(5.4, "Please, use ResumeExecution with Context param and entry name")
	bool ResumeExecution(FRigVMExtendedExecuteContext& Context, TArrayView<URigVMMemoryStorage*> Memory, const FName& InEntryName = NAME_None) { return false; }
	UE_DEPRECATED(5.4, "Please, use ResumeExecution in the Host RigVMDebugInfo")
	bool ResumeExecution(FRigVMExtendedExecuteContext& Context) { return false; }

#endif // WITH_EDITOR

	// Returns the parameters of the VM
	const TArray<FRigVMParameter>& GetParameters() const;

	// Returns a parameter given it's name
	FRigVMParameter GetParameterByName(const FName& InParameterName);

	UE_DEPRECATED(5.4, "Please, use AddParameter with WorkMemory param")
	FRigVMParameter AddParameter(ERigVMParameterType InType, const FName& InParameterName, const FName& InWorkMemoryPropertyName) { return FRigVMParameter(); }

	FRigVMParameter AddParameter(FRigVMExtendedExecuteContext& Context, ERigVMParameterType InType, const FName& InParameterName, const FName& InWorkMemoryPropertyName)
	{
		check(GetWorkMemory(Context));

		if(ParametersNameMap.Contains(InParameterName))
		{
			return FRigVMParameter();
		}

		const FProperty* Property = GetWorkMemory(Context)->FindPropertyByName(InWorkMemoryPropertyName);
		const int32 PropertyIndex = GetWorkMemory(Context)->GetPropertyIndex(Property);

		UScriptStruct* Struct = nullptr;
		if(const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			Struct = StructProperty->Struct;
		}
		
		FRigVMParameter Parameter(InType, InParameterName, PropertyIndex, Property->GetCPPType(), Struct);
		ParametersNameMap.Add(Parameter.Name, Parameters.Add(Parameter));
		return Parameter;
	}

	UE_DEPRECATED(5.4, "Please, use GetParameterArraySize with WorkMemory param")
	int32 GetParameterArraySize(const FRigVMParameter& InParameter) { return 0; }

	// Retrieve the array size of the parameter
	int32 GetParameterArraySize(FRigVMExtendedExecuteContext& Context, const FRigVMParameter& InParameter)
	{
		const int32 PropertyIndex = InParameter.GetRegisterIndex();
		const FProperty* Property = GetWorkMemory(Context)->GetProperties()[PropertyIndex];
		const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property);
		if(ArrayProperty)
		{
			FScriptArrayHelper ArrayHelper(ArrayProperty, GetWorkMemory(Context)->GetData<uint8>(PropertyIndex));
			return ArrayHelper.Num();
		}
		return 1;
	}

	UE_DEPRECATED(5.4, "Please, use GetParameterArraySize with WorkMemory param")
	int32 GetParameterArraySize(int32 InParameterIndex) { return 0; }

	// Retrieve the array size of the parameter
	int32 GetParameterArraySize(FRigVMExtendedExecuteContext& Context, int32 InParameterIndex)
	{
		return GetParameterArraySize(Context, Parameters[InParameterIndex]);
	}

	UE_DEPRECATED(5.4, "Please, use GetParameterArraySize with WorkMemory param")
	int32 GetParameterArraySize(const FName& InParameterName) { return 0; }

	// Retrieve the array size of the parameter
	int32 GetParameterArraySize(FRigVMExtendedExecuteContext& Context, const FName& InParameterName)
	{
		const int32 ParameterIndex = ParametersNameMap.FindChecked(InParameterName);
		return GetParameterArraySize(Context, ParameterIndex);
	}

	//UE_DEPRECATED(5.4, "Please, use GetParameterValue with WorkMemory param")
	//template<class T> T GetParameterValue(const FRigVMParameter& InParameter, int32 InArrayIndex = 0, T DefaultValue = T{}) { return DefaultValue; }

	// Retrieve the value of a parameter
	template<class T>
	T GetParameterValue(FRigVMExtendedExecuteContext& Context, const FRigVMParameter& InParameter, int32 InArrayIndex = 0, T DefaultValue = T{})
	{
		if (InParameter.GetRegisterIndex() != INDEX_NONE)
		{
			if(GetWorkMemory(Context)->IsArray(InParameter.GetRegisterIndex()))
			{
				TArray<T>& Storage = *GetWorkMemory(Context)->GetData<TArray<T>>(InParameter.GetRegisterIndex());
				if(Storage.IsValidIndex(InArrayIndex))
				{
					return Storage[InArrayIndex];
				}
			}
			else
			{
				return *GetWorkMemory(Context)->GetData<T>(InParameter.GetRegisterIndex());
			}
			
			return *GetWorkMemory(Context)->GetData<T>(InParameter.GetRegisterIndex());
		}
		return DefaultValue;
	}

	//UE_DEPRECATED(5.4, "Please, use GetParameterValue with WorkMemory param")
	//template<class T> T GetParameterValue(int32 InParameterIndex, int32 InArrayIndex = 0, T DefaultValue = T{}) { return DefaultValue; }

	// Retrieve the value of a parameter given its index
	template<class T>
	T GetParameterValue(FRigVMExtendedExecuteContext& Context, int32 InParameterIndex, int32 InArrayIndex = 0, T DefaultValue = T{})
	{
		return GetParameterValue<T>(Context, Parameters[InParameterIndex], InArrayIndex, DefaultValue);
	}

	//UE_DEPRECATED(5.4, "Please, use GetParameterValue with WorkMemory param")
	//template<class T> T GetParameterValue(const FName& InParameterName, int32 InArrayIndex = 0, T DefaultValue = T{}) { return DefaultValue; }

	// Retrieve the value of a parameter given its name
	template<class T>
	T GetParameterValue(FRigVMExtendedExecuteContext& Context, const FName& InParameterName, int32 InArrayIndex = 0, T DefaultValue = T{})
	{
		int32 ParameterIndex = ParametersNameMap.FindChecked(InParameterName);
		return GetParameterValue<T>(Context, ParameterIndex, InArrayIndex, DefaultValue);
	}

	//UE_DEPRECATED(5.4, "Please, use SetParameterValue with WorkMemory param")
	//template<class T> void SetParameterValue(const FRigVMParameter& InParameter, const T& InNewValue, int32 InArrayIndex = 0) {}

	// Set the value of a parameter
	template<class T>
	void SetParameterValue(FRigVMExtendedExecuteContext& Context, const FRigVMParameter& InParameter, const T& InNewValue, int32 InArrayIndex = 0)
	{
		if (InParameter.GetRegisterIndex() != INDEX_NONE)
		{
			if(GetWorkMemory(Context)->IsArray(InParameter.GetRegisterIndex()))
			{
				TArray<T>& Storage = *GetWorkMemory(Context)->GetData<TArray<T>>(InParameter.GetRegisterIndex());
				if(Storage.IsValidIndex(InArrayIndex))
				{
					Storage[InArrayIndex] = InNewValue;
				}
			}
			else
			{
				T& Storage = *GetWorkMemory(Context)->GetData<T>(InParameter.GetRegisterIndex());
				Storage = InNewValue;
			}
		}
	}

	//UE_DEPRECATED(5.4, "Please, use SetParameterValue with WorkMemory param")
	//template<class T> void SetParameterValue(int32 ParameterIndex, const T& InNewValue, int32 InArrayIndex = 0) {}

	// Set the value of a parameter given its index
	template<class T>
	void SetParameterValue(FRigVMExtendedExecuteContext& Context, int32 ParameterIndex, const T& InNewValue, int32 InArrayIndex = 0)
	{
		return SetParameterValue<T>(Context, Parameters[ParameterIndex], InNewValue, InArrayIndex);
	}

	//UE_DEPRECATED(5.4, "Please, use SetParameterValue with WorkMemory param")
	//template<class T> void SetParameterValue(const FName& InParameterName, const T& InNewValue, int32 InArrayIndex = 0) {}

	// Set the value of a parameter given its name
	template<class T>
	void SetParameterValue(FRigVMExtendedExecuteContext& Context, const FName& InParameterName, const T& InNewValue, int32 InArrayIndex = 0)
	{
		int32 ParameterIndex = ParametersNameMap.FindChecked(InParameterName);
		return SetParameterValue<T>(Context, ParameterIndex, InNewValue, InArrayIndex);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM, meta = (DeprecatedFunction, DeprecationMessage = "This function has been deprecated and it is no longer supported, please, update your code."))
	bool GetParameterValueBool(const FName& InParameterName, int32 InArrayIndex = 0) { return false; }

	UFUNCTION(BlueprintCallable, Category = RigVM, meta = (DeprecatedFunction, DeprecationMessage = "This function has been deprecated and it is no longer supported, please, update your code."))
	float GetParameterValueFloat(const FName& InParameterName, int32 InArrayIndex = 0) { return 0.f; }

	UFUNCTION(BlueprintCallable, Category = RigVM, meta = (DeprecatedFunction, DeprecationMessage = "This function has been deprecated and it is no longer supported, please, update your code."))
	double GetParameterValueDouble(const FName& InParameterName, int32 InArrayIndex = 0) { return 0.0; }

	UFUNCTION(BlueprintCallable, Category = RigVM, meta = (DeprecatedFunction, DeprecationMessage = "This function has been deprecated and it is no longer supported, please, update your code."))
	int32 GetParameterValueInt(const FName& InParameterName, int32 InArrayIndex = 0) { return 0; }

	UFUNCTION(BlueprintCallable, Category = RigVM, meta = (DeprecatedFunction, DeprecationMessage = "This function has been deprecated and it is no longer supported, please, update your code."))
	FName GetParameterValueName(const FName& InParameterName, int32 InArrayIndex = 0) { return NAME_None; }

	UFUNCTION(BlueprintCallable, Category = RigVM, meta = (DeprecatedFunction, DeprecationMessage = "This function has been deprecated and it is no longer supported, please, update your code."))
	FString GetParameterValueString(const FName& InParameterName, int32 InArrayIndex = 0) { return FString(); }

	UFUNCTION(BlueprintCallable, Category = RigVM, meta = (DeprecatedFunction, DeprecationMessage = "This function has been deprecated and it is no longer supported, please, update your code."))
	FVector2D GetParameterValueVector2D(const FName& InParameterName, int32 InArrayIndex = 0) { return FVector2D::ZeroVector; }

	UFUNCTION(BlueprintCallable, Category = RigVM, meta = (DeprecatedFunction, DeprecationMessage = "This function has been deprecated and it is no longer supported, please, update your code."))
	FVector GetParameterValueVector(const FName& InParameterName, int32 InArrayIndex = 0) { return FVector::ZeroVector; }

	UFUNCTION(BlueprintCallable, Category = RigVM, meta = (DeprecatedFunction, DeprecationMessage = "This function has been deprecated and it is no longer supported, please, update your code."))
	FQuat GetParameterValueQuat(const FName& InParameterName, int32 InArrayIndex = 0) { return FQuat::Identity; }

	UFUNCTION(BlueprintCallable, Category = RigVM, meta = (DeprecatedFunction, DeprecationMessage = "This function has been deprecated and it is no longer supported, please, update your code."))
	FTransform GetParameterValueTransform(const FName& InParameterName, int32 InArrayIndex = 0) { return FTransform::Identity; }

	UFUNCTION(BlueprintCallable, Category = RigVM, meta = (DeprecatedFunction, DeprecationMessage = "This function has been deprecated and it is no longer supported, please, update your code."))
	void SetParameterValueBool(const FName& InParameterName, bool InValue, int32 InArrayIndex = 0) {}

	UFUNCTION(BlueprintCallable, Category = RigVM, meta = (DeprecatedFunction, DeprecationMessage = "This function has been deprecated and it is no longer supported, please, update your code."))
	void SetParameterValueFloat(const FName& InParameterName, float InValue, int32 InArrayIndex = 0) {}

	UFUNCTION(BlueprintCallable, Category = RigVM, meta = (DeprecatedFunction, DeprecationMessage = "This function has been deprecated and it is no longer supported, please, update your code."))
	void SetParameterValueDouble(const FName& InParameterName, double InValue, int32 InArrayIndex = 0) {}

	UFUNCTION(BlueprintCallable, Category = RigVM, meta = (DeprecatedFunction, DeprecationMessage = "This function has been deprecated and it is no longer supported, please, update your code."))
	void SetParameterValueInt(const FName& InParameterName, int32 InValue, int32 InArrayIndex = 0) {}

	UFUNCTION(BlueprintCallable, Category = RigVM, meta = (DeprecatedFunction, DeprecationMessage = "This function has been deprecated and it is no longer supported, please, update your code."))
	void SetParameterValueName(const FName& InParameterName, const FName& InValue, int32 InArrayIndex = 0) {}

	UFUNCTION(BlueprintCallable, Category = RigVM, meta = (DeprecatedFunction, DeprecationMessage = "This function has been deprecated and it is no longer supported, please, update your code."))
	void SetParameterValueString(const FName& InParameterName, const FString& InValue, int32 InArrayIndex = 0) {}

	UFUNCTION(BlueprintCallable, Category = RigVM, meta = (DeprecatedFunction, DeprecationMessage = "This function has been deprecated and it is no longer supported, please, update your code."))
	void SetParameterValueVector2D(const FName& InParameterName, const FVector2D& InValue, int32 InArrayIndex = 0) {}

	UFUNCTION(BlueprintCallable, Category = RigVM, meta = (DeprecatedFunction, DeprecationMessage = "This function has been deprecated and it is no longer supported, please, update your code."))
	void SetParameterValueVector(const FName& InParameterName, const FVector& InValue, int32 InArrayIndex = 0) {}

	UFUNCTION(BlueprintCallable, Category = RigVM, meta = (DeprecatedFunction, DeprecationMessage = "This function has been deprecated and it is no longer supported, please, update your code."))
	void SetParameterValueQuat(const FName& InParameterName, const FQuat& InValue, int32 InArrayIndex = 0) {}

	UFUNCTION(BlueprintCallable, Category = RigVM, meta = (DeprecatedFunction, DeprecationMessage = "This function has been deprecated and it is no longer supported, please, update your code."))
	void SetParameterValueTransform(const FName& InParameterName, const FTransform& InValue, int32 InArrayIndex = 0) {}

	UE_DEPRECATED(5.3, "Please, use ClearExternalVariables with Context param")
	void ClearExternalVariables() {}

	// Clears the external variables of the VM
	void ClearExternalVariables(FRigVMExtendedExecuteContext& Context)
	{
		ExternalVariables.Reset();
		Context.ExternalVariableRuntimeData.Reset();
	}

	// Returns the external variables of the VM (without variable memory, as it is stored in Context)
	const TArray<FRigVMExternalVariableDef>& GetExternalVariableDefs() const
	{
		return ExternalVariables;
	}

	UE_DEPRECATED(5.3, "Please use GetExternalVariables with Context parameter.")
	const TArray<FRigVMExternalVariable> GetExternalVariables() const { return TArray<FRigVMExternalVariable>(); }

	// Returns the external variables of the VM with variable memory
	const TArray<FRigVMExternalVariable> GetExternalVariables(const FRigVMExtendedExecuteContext& Context) const
	{
		TArray<FRigVMExternalVariable> ExternalVariablesWithInstanceData;

		const int32 Num = ExternalVariables.Num();
		for (int i=0; i<Num; ++i)
		{
			const FRigVMExternalVariableDef& ExternalVariable = ExternalVariables[i];
			const FRigVMExternalVariableRuntimeData& ExternalVariableData = Context.ExternalVariableRuntimeData[i];

			ExternalVariablesWithInstanceData.Add(FRigVMExternalVariable(ExternalVariable, ExternalVariableData.Memory));
		}

		return ExternalVariablesWithInstanceData;
	}

	// Returns an external variable Def given it's name
	FRigVMExternalVariableDef GetExternalVariableDefByName(const FName& InExternalVariableName);

	UE_DEPRECATED(5.3, "Please use GetExternalVariableByName with ExtendedExecuteContext parameter.")
	FRigVMExternalVariable GetExternalVariableByName(const FName& InExternalVariableName) { return FRigVMExternalVariable(); }

	// Returns an external variable given it's name
	FRigVMExternalVariable GetExternalVariableByName(const FRigVMExtendedExecuteContext& Context, const FName& InExternalVariableName);

	UE_DEPRECATED(5.3, "Please use AddExternalVariable with ExtendedExecuteContext parameter.")
	FRigVMOperand AddExternalVariable(const FRigVMExternalVariable& InExternalVariable) { return FRigVMOperand(); }

	// Sets the external variables without the instance data
	void SetExternalVariableDefs(const TArray<FRigVMExternalVariable>& InExternalVariables)
	{
		ExternalVariables.Reset(InExternalVariables.Num());
		for (const FRigVMExternalVariableDef& ExternalVariable : InExternalVariables)
		{
			ExternalVariables.Add(ExternalVariable);
		}

		RefreshExternalPropertyPaths();
	}

	// Sets the external variables instance data required for execution
	void SetExternalVariablesInstanceData(FRigVMExtendedExecuteContext& Context, const TArray<FRigVMExternalVariable>& InExternalVariables, bool bAllowNullMemory = false)
	{
		const int32 NumExternalVariables = InExternalVariables.Num();
		check(ExternalVariables.Num() == NumExternalVariables);
		
		Context.ExternalVariableRuntimeData.Reset(NumExternalVariables);
		
		for (int32 i = 0; i < NumExternalVariables; i++)
		{
			const FRigVMExternalVariable& InExternalVariable = InExternalVariables[i];
			FRigVMExternalVariableDef& ExternalVariableDef = ExternalVariables[i];

			// Only check name and property, to allow the case where an UUserStruct is deleted while used inside a Rig
			check(ExternalVariableDef.Name == InExternalVariable.Name);
			check(ExternalVariableDef.Property == InExternalVariable.Property);
			check(bAllowNullMemory || InExternalVariable.Memory != nullptr);
			
			Context.ExternalVariableRuntimeData.Add(FRigVMExternalVariableRuntimeData(InExternalVariable.Memory));
		}
	}

	// Adds a new external / unowned variable to the VM
	FRigVMOperand AddExternalVariable(FRigVMExtendedExecuteContext& Context, const FRigVMExternalVariable& InExternalVariable, bool bAllowNullMemory = false)
	{
		check(bAllowNullMemory || InExternalVariable.Memory != nullptr);

		const int32 VariableIndex = ExternalVariables.Add(InExternalVariable);
		Context.ExternalVariableRuntimeData.Add(FRigVMExternalVariableRuntimeData(InExternalVariable.Memory));

		return FRigVMOperand(ERigVMMemoryType::External, VariableIndex);
	}

	UE_DEPRECATED(5.4, "Please use SetPropertyValueFromString with ExtendedExecuteContext parameter.")
	void SetPropertyValueFromString(const FRigVMOperand& InOperand, const FString& InDefaultValue) {}

	void SetPropertyValueFromString(FRigVMExtendedExecuteContext& Context, const FRigVMOperand& InOperand, const FString& InDefaultValue);

	// returns the statistics information
	UFUNCTION(BlueprintPure, Category = "RigVM", meta=(DeprecatedFunction))
	FRigVMStatistics GetStatistics() const
	{
		FRigVMStatistics Statistics;
		if(GetLiteralMemory())
		{
			//Statistics.LiteralMemory = GetLiteralMemory()->GetStatistics();
		}
		//if(GetWorkMemory(Context))
		//{
		//	Statistics.WorkMemory = GetWorkMemory(Context)->GetStatistics();
		//}

		Statistics.ByteCode = ByteCodePtr->GetStatistics();
		Statistics.BytesForCaching = FirstHandleForInstruction.GetAllocatedSize(); // +Context.CachedMemoryHandles.GetAllocatedSize(); // Requires context, but fn deprecated already
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
	UE_DEPRECATED(5.4, "Please use DumpByteCodeAsTextArray with ExtendedExecuteContext parameter.")
	TArray<FString> DumpByteCodeAsTextArray(const TArray<int32>& InInstructionOrder = TArray<int32>(), bool bIncludeLineNumbers = true, TFunction<FString(const FString& RegisterName, const FString& RegisterOffsetName)> OperandFormatFunction = nullptr) { return TArray<FString>(); }

	// returns the instructions as text, OperandFormatFunction is an optional argument that allows you to override how operands are displayed, for example, see SRigVMExecutionStackView::PopulateStackView 
	TArray<FString> DumpByteCodeAsTextArray(FRigVMExtendedExecuteContext& Context, const TArray<int32> & InInstructionOrder = TArray<int32>(), bool bIncludeLineNumbers = true, TFunction<FString(const FString& RegisterName, const FString& RegisterOffsetName)> OperandFormatFunction = nullptr);

	UE_DEPRECATED(5.4, "Please use DumpByteCodeAsText with ExtendedExecuteContext parameter.")
	FString DumpByteCodeAsText(const TArray<int32>& InInstructionOrder = TArray<int32>(), bool bIncludeLineNumbers = true) { return FString(); }

	FString DumpByteCodeAsText(FRigVMExtendedExecuteContext& Context, const TArray<int32>& InInstructionOrder = TArray<int32>(), bool bIncludeLineNumbers = true);
#endif

#if WITH_EDITOR
	UE_DEPRECATED(5.4, "Please use GetOperandLabel with ExtendedExecuteContext parameter.")
	FString GetOperandLabel(const FRigVMOperand& InOperand, TFunction<FString(const FString& RegisterName, const FString& RegisterOffsetName)> FormatFunction = nullptr) { return FString(); }

	// FormatFunction is an optional argument that allows you to override how operands are displayed, for example, see SRigVMExecutionStackView::PopulateStackView
	FString GetOperandLabel(FRigVMExtendedExecuteContext& Context, const FRigVMOperand & InOperand, TFunction<FString(const FString& RegisterName, const FString& RegisterOffsetName)> FormatFunction = nullptr);
#endif

	UE_DEPRECATED(5.3, "Please use ExecutionReachedExit in the ExtendedExecuteContext.")
	FExecutionReachedExitEvent& ExecutionReachedExit() 
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return OnExecutionReachedExit;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
#if WITH_EDITOR
	UE_DEPRECATED(5.3, "Please use ExecutionHalted in the ExtendedExecuteContext.")
	FExecutionHaltedEvent& ExecutionHalted()
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return OnExecutionHalted;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	UE_DEPRECATED(5.3, "Please use SetDebugInfo in the ExtendedExecuteContext.")
	void SetDebugInfo(FRigVMDebugInfo* InDebugInfo) { }

	UE_DEPRECATED(5.3, "Please use GetHaltedAtBreakpoint in the RigVMHost.")
	const FRigVMBreakpoint& GetHaltedAtBreakpoint() const { static FRigVMBreakpoint Dummy; return Dummy; }

	UE_DEPRECATED(5.3, "Please use SetBreakpointAction in the RigVMHost.")
	void SetBreakpointAction(const ERigVMBreakpointAction& Action) { }
#endif

	UE_DEPRECATED(5.3, "Please use GetNumExecutions() in the ExtendedExecuteContext.")
	uint32 GetNumExecutions() const { return 0; }

	UE_DEPRECATED(5.3, "Please, use an external Context in the RigVMHost.")
	const FRigVMExtendedExecuteContext* GetContext() const 
	{
		return nullptr;
	}

	UE_DEPRECATED(5.3, "Please, use an external Context in the RigVMHost.")
	FRigVMExtendedExecuteContext* GetContext()
	{
		return nullptr;
	}

	template<typename ExecuteContextType = FRigVMExecuteContext>
	UE_DEPRECATED(5.3, "Please, use an external Context in the RigVMHost.")
	const ExecuteContextType& GetPublicData() const
	{
		static ExecuteContextType Dummy;
		return Dummy;
	}

	template<typename ExecuteContextType = FRigVMExecuteContext>
	UE_DEPRECATED(5.3, "Please, use an external Context in the RigVMHost.")
	ExecuteContextType& GetPublicData()
	{
		static ExecuteContextType Dummy;
		return Dummy;
	}
	
	UE_DEPRECATED(5.3, "Please, use an external Context in the RigVMHost.")
	const UScriptStruct* GetContextPublicDataStruct() const
	{
		return nullptr;
	}
	UE_DEPRECATED(5.3, "Please, use an external Context in the RigVMHost.")
	void SetContextPublicDataStruct(UScriptStruct* InScriptStruct) {}

private:
	bool ResolveFunctionsIfRequired();
	void RefreshInstructionsIfRequired();

public:
	UE_DEPRECATED(5.4, "Please use InvalidateCachedMemory with ExtendedExecuteContext parameter.")
	void InvalidateCachedMemory() {}
	void InvalidateCachedMemory(FRigVMExtendedExecuteContext& Context);

private:
	void InstructionOpEval(FRigVMExtendedExecuteContext& Context, int32 InstructionIndex, int32 InHandleBaseIndex, const TFunctionRef<void(FRigVMExtendedExecuteContext& Context, int32 InHandleIndex, const FRigVMBranchInfoKey& InBranchInfoKey, const FRigVMOperand& InArg)>& InOpFunc);
	void PrepareMemoryForExecution(FRigVMExtendedExecuteContext& Context);
	void CacheMemoryHandlesIfRequired(FRigVMExtendedExecuteContext& Context);
	void RebuildByteCodeOnLoad();

	UPROPERTY(transient)
	FRigVMInstructionArray Instructions;

protected:

	virtual void SetInstructionIndex(FRigVMExtendedExecuteContext& Context, uint16 InInstructionIndex) { Context.GetPublicData<>().InstructionIndex = InInstructionIndex; }

#if WITH_EDITORONLY_DATA
	UPROPERTY(transient)
	uint32 NumExecutions_DEPRECATED = 0;
#endif

	std::atomic<int32> ActiveExecutions;

public:
	int32 GetActiveExecutions() const
	{
		return ActiveExecutions.load();
	}

	bool ValidateBytecode();
	void RefreshArgumentNameCaches();

private:

#if WITH_EDITOR
	bool ShouldHaltAtInstruction(FRigVMExtendedExecuteContext& Context, const FName& InEventName, const uint16 InstructionIndex);
#endif

	UPROPERTY()
	TArray<FName> FunctionNamesStorage;
	TArray<FName>* FunctionNamesPtr;
	TArray<FName>& GetFunctionNames() { return *FunctionNamesPtr; }
	const TArray<FName>& GetFunctionNames() const { return *FunctionNamesPtr; }

	TArray<const FRigVMFunction*> FunctionsStorage;
	TArray<const FRigVMFunction*>* FunctionsPtr;
public:
	TArray<const FRigVMFunction*>& GetFunctions() { return *FunctionsPtr; }
	const TArray<const FRigVMFunction*>& GetFunctions() const { return *FunctionsPtr; }

private:
	TArray<const FRigVMDispatchFactory*> FactoriesStorage;
	TArray<const FRigVMDispatchFactory*>* FactoriesPtr;
	TArray<const FRigVMDispatchFactory*>& GetFactories() { return *FactoriesPtr; }
	const TArray<const FRigVMDispatchFactory*>& GetFactories() const { return *FactoriesPtr; }

	UPROPERTY()
	TArray<FRigVMParameter> Parameters;

	TMap<FName, int32> ParametersNameMap;

	TArray<uint32> FirstHandleForInstruction;

	int32 MemoryHandleCount = 0;

	TArray<FRigVMExternalVariableDef> ExternalVariables;
	TArray<FRigVMLazyBranch> LazyBranches;
	
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
	
	// debug watch register memory needs to be cleared for each execution
	void ClearDebugMemory(FRigVMExtendedExecuteContext& Context);

	void CacheSingleMemoryHandle(FRigVMExtendedExecuteContext& Context, int32 InHandleIndex, const FRigVMBranchInfoKey& InBranchInfoKey, const FRigVMOperand& InArg, bool bForExecute = false);

	void CopyOperandForDebuggingIfNeeded(FRigVMExtendedExecuteContext& Context, const FRigVMOperand& InArg, const FRigVMMemoryHandle& InHandle)
	{
#if WITH_EDITOR
		const FRigVMOperand KeyOperand(InArg.GetMemoryType(), InArg.GetRegisterIndex()); // no register offset
		if(const TArray<FRigVMOperand>* DebugOperandsPtr = OperandToDebugRegisters.Find(KeyOperand))
		{
			const TArray<FRigVMOperand>& DebugOperands = *DebugOperandsPtr;
			for(const FRigVMOperand& DebugOperand : DebugOperands)
			{
				CopyOperandForDebuggingImpl(Context, InArg, InHandle, DebugOperand);
			}
		}
#endif
	}

	void CopyOperandForDebuggingImpl(FRigVMExtendedExecuteContext& Context, const FRigVMOperand& InArg, const FRigVMMemoryHandle& InHandle, const FRigVMOperand& InDebugOperand);

	FRigVMCopyOp GetCopyOpForOperands(const FRigVMOperand& InSource, const FRigVMOperand& InTarget);
	void RefreshExternalPropertyPaths();
	
	TMap<FRigVMOperand, TArray<FRigVMOperand>> OperandToDebugRegisters;

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

#if WITH_EDITOR
	struct FInstructionBracketGuard
	{
	public:
		FInstructionBracketGuard(FRigVMExtendedExecuteContext& InOutContext, int32 InFirstInstruction, int32 InLastInstruction)
		: Context(InOutContext)
		{
			const TTuple<int32, int32> Tuple(InFirstInstruction, InLastInstruction);
			if(Context.InstructionBrackets.Contains(Tuple))
			{
				static constexpr TCHAR Format[] = TEXT("Re-Entry of Instructions %d - %d.");
				ErrorMessage = FString::Printf(Format, Tuple.Get<0>(), Tuple.Get<1>());
			}
			Context.InstructionBrackets.Add(Tuple);
		}

		~FInstructionBracketGuard()
		{
			Context.InstructionBrackets.Pop();
		}
		
	private:

		FRigVMExtendedExecuteContext& Context;
		FString ErrorMessage;

		friend class URigVM;
	};
#endif

private:

	UPROPERTY()
	uint32 CachedVMHash = 0;

	mutable TArray<FName> EntryNames;

	UE_DEPRECATED(5.3, "Please use OnExecutionReachedExit in the ExtendedExecuteContext.")
	FExecutionReachedExitEvent OnExecutionReachedExit;

#if WITH_EDITOR
	UE_DEPRECATED(5.3, "Please use OnExecutionHalted in the ExtendedExecuteContext.")
	FExecutionHaltedEvent OnExecutionHalted;
#endif

protected:
	const URigVMHost* GetHostCDO() const;

	void Reset_Internal();
	void ClearMemory_Internal();
	void InvalidateCachedMemory_Internal();

	void SetupInstructionTracking(FRigVMExtendedExecuteContext& Context, int32 InInstructionCount);
	void StartProfiling(FRigVMExtendedExecuteContext& Context);
	void StopProfiling(FRigVMExtendedExecuteContext& Context);

	FCriticalSection ResolveFunctionsMutex;

	friend class URigVMCompiler;
	friend struct FRigVMCompilerWorkData;
	friend struct FRigVMCodeGenerator;
};

