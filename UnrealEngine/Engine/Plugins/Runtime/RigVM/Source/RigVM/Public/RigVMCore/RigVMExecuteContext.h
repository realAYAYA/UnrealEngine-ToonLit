// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Logging/TokenizedMessage.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/AssertionMacros.h"
#include "RigVMDefines.h"
#include "RigVMExternalVariable.h"
#include "RigVMModule.h"
#include "Templates/SharedPointer.h"
#include "Trace/Detail/Channel.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UnrealNames.h"
#include "UObject/UnrealType.h"
#include "RigVMCore/RigVMDebugInfo.h"
#include "RigVMCore/RigVMNameCache.h"
#include "UObject/StructOnScope.h"
#include "RigVMLog.h"
#include "RigVMDrawInterface.h"
#include "RigVMDrawContainer.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"

#include "RigVMExecuteContext.generated.h"

class URigVM;
struct FRigVMDispatchFactory;
struct FRigVMExecuteContext;
struct FRigVMExtendedExecuteContext;

USTRUCT()
struct RIGVM_API FRigVMSlice
{
public:

	GENERATED_BODY()

	FRigVMSlice()
	: LowerBound(0)
	, UpperBound(0)
	, Index(INDEX_NONE)
	, Offset(0)
	, InstructionIndex(INDEX_NONE)
	{
		Reset();
	}

	FRigVMSlice(int32 InCount)
		: LowerBound(0)
		, UpperBound(InCount - 1)
		, Index(INDEX_NONE)
		, Offset(0)
		, InstructionIndex(INDEX_NONE)
	{
		Reset();
	}

	bool IsValid() const
	{ 
		return Index != INDEX_NONE;
	}
	
	bool IsComplete() const
	{
		return Index > UpperBound;
	}

	int32 GetIndex() const
	{
		return Index + Offset;
	}

	int32 GetRelativeIndex() const
	{
		return Index - LowerBound;
	}

	void SetRelativeIndex(int32 InIndex)
	{
		Index = InIndex + LowerBound;
	}

	float GetRelativeRatio() const
	{
		return float(GetRelativeIndex()) / float(FMath::Max<int32>(1, Num() - 1));
	}

	int32 GetOffset() const
	{
		return Offset;
	}

	void SetOffset(int32 InSliceOffset)
	{
		Offset = InSliceOffset;
	}

	int32 GetInstructionIndex() const
	{
		return InstructionIndex;
	}

	void SetInstructionIndex(int32 InInstructionIndex)
	{
		InstructionIndex = InInstructionIndex;
	}

	int32 Num() const
	{
		return 1 + UpperBound - LowerBound;
	}

	int32 TotalNum() const
	{
		return UpperBound + 1 + Offset;
	}

	operator bool() const
	{
		return IsValid();
	}

	bool operator !() const
	{
		return !IsValid();
	}

	operator int32() const
	{
		return Index;
	}

	FRigVMSlice& operator++()
	{
		Index++;
		return *this;
	}

	FRigVMSlice operator++(int32)
	{
		FRigVMSlice TemporaryCopy = *this;
		++*this;
		return TemporaryCopy;
	}

	bool Next()
	{
		if (!IsValid())
		{
			return false;
		}

		if (IsComplete())
		{
			return false;
		}

		Index++;
		return true;
	}

	void Reset()
	{
		if (UpperBound >= LowerBound)
		{
			Index = LowerBound;
		}
		else
		{
			Index = INDEX_NONE;
		}
	}

private:

	int32 LowerBound;
	int32 UpperBound;
	int32 Index;
	int32 Offset;
	int32 InstructionIndex;
};

USTRUCT()
struct RIGVM_API FRigVMRuntimeSettings
{
	GENERATED_BODY()

	/**
	 * The largest allowed size for arrays within the RigVM.
	 * Accessing or creating larger arrays will cause runtime errors in the rig.
	 */
	UPROPERTY(EditAnywhere, Category = "VM")
	int32 MaximumArraySize = 2048;

#if WITH_EDITORONLY_DATA
	// When enabled records the timing of each instruction / node
	// on each node and within the execution stack window.
	// Keep in mind when looking at nodes in a function the duration
	// represents the accumulated duration of all invocations
	// of the function currently running.
	UPROPERTY(EditAnywhere, Category = "VM")
	bool bEnableProfiling = false;
#endif

	/*
	 * The function to use for logging anything from the VM to the host
	 */
	using LogFunctionType = TFunction<void(EMessageSeverity::Type,const FRigVMExecuteContext*,const FString&)>;
	TSharedPtr<LogFunctionType> LogFunction = nullptr;

	void SetLogFunction(LogFunctionType InLogFunction)
	{
		LogFunction = MakeShared<LogFunctionType>(InLogFunction);
	}

	/*
	 * Validate the settings
	 */
	void Validate()
	{
		MaximumArraySize = FMath::Clamp(MaximumArraySize, 1, INT32_MAX);
	}
};

/**
 * The execute context is used for mutable nodes to
 * indicate execution order.
 */
USTRUCT(BlueprintType, meta=(DisplayName="Execute Context"))
struct FRigVMExecuteContext
{
	GENERATED_BODY()

	FRigVMExecuteContext()
		: EventName(NAME_None)
		, FunctionName(NAME_None)
		, InstructionIndex(0)
		, DeltaTime(0.0)
		, AbsoluteTime(0.0)
		, FramesPerSecond(1.0 / 60.0)
		, RuntimeSettings()
		, NameCache(nullptr)
#if WITH_EDITOR
		, LogPtr(nullptr)
#endif
		, DrawInterfacePtr(nullptr)
		, DrawContainerPtr(nullptr)
		, ToWorldSpaceTransform(FTransform::Identity)
		, OwningComponent(nullptr)
		, OwningActor(nullptr)
		, World(nullptr)
	{
	}

	virtual ~FRigVMExecuteContext() {}

	void Log(EMessageSeverity::Type InSeverity, const FString& InMessage) const
	{
		if(RuntimeSettings.LogFunction.IsValid())
		{
			(*RuntimeSettings.LogFunction)(InSeverity, this, InMessage);
		}
		else
		{
			if(InSeverity == EMessageSeverity::Error)
			{
				UE_LOG(LogRigVM, Error, TEXT("Instruction %d: %s"), InstructionIndex, *InMessage);
			}
			else if(InSeverity == EMessageSeverity::Warning)
			{
				UE_LOG(LogRigVM, Warning, TEXT("Instruction %d: %s"), InstructionIndex, *InMessage);
			}
			else
			{
				UE_LOG(LogRigVM, Display, TEXT("Instruction %d: %s"), InstructionIndex, *InMessage);
			}
		}
	}

	template <typename FmtType, typename... Types>
	void Logf(EMessageSeverity::Type InSeverity, const FmtType& Fmt, Types... Args) const
	{
		Log(InSeverity, FString::Printf(Fmt, Args...));
	}

	uint16 GetInstructionIndex() const { return InstructionIndex; }

	FName GetFunctionName() const { return FunctionName; }
	
	FName GetEventName() const { return EventName; }
	void SetEventName(const FName& InName) { EventName = InName; }

	double GetDeltaTime() const { return DeltaTime; }
	void SetDeltaTime(double InDeltaTime) { DeltaTime = InDeltaTime; }

	double GetAbsoluteTime() const { return AbsoluteTime; } 
	void SetAbsoluteTime(double InAbsoluteTime) { AbsoluteTime = InAbsoluteTime; }

	double GetFramesPerSecond() const { return FramesPerSecond; } 
	void SetFramesPerSecond(double InFramesPerSecond) { FramesPerSecond = InFramesPerSecond; }

	/** The current transform going from rig (global) space to world space */
	const FTransform& GetToWorldSpaceTransform() const { return ToWorldSpaceTransform; };

	/** The current component this VM is owned by */
	const USceneComponent* GetOwningComponent() const { return OwningComponent; }

	/** The current actor this VM is owned by */
	const AActor* GetOwningActor() const { return OwningActor; }

	/** The world this VM is running in */
	const UWorld* GetWorld() const { return World; }

	RIGVM_API void SetOwningComponent(const USceneComponent* InOwningComponent);

	RIGVM_API void SetOwningActor(const AActor* InActor);

	RIGVM_API void SetWorld(const UWorld* InWorld);

	void SetToWorldSpaceTransform(const FTransform& InToWorldSpaceTransform) { ToWorldSpaceTransform = InToWorldSpaceTransform; }

	/**
	 * Converts a transform from VM (global) space to world space
	 */
	FTransform ToWorldSpace(const FTransform& InTransform) const
	{
		return InTransform * ToWorldSpaceTransform;
	}

	/**
	 * Converts a transform from world space to VM (global) space
	 */
	FTransform ToVMSpace(const FTransform& InTransform) const
	{
		return InTransform.GetRelativeTransform(ToWorldSpaceTransform);
	}

	/**
	 * Converts a location from VM (global) space to world space
	 */
	FVector ToWorldSpace(const FVector& InLocation) const
	{
		return ToWorldSpaceTransform.TransformPosition(InLocation);
	}

	/**
	 * Converts a location from world space to VM (global) space
	 */
	FVector ToVMSpace(const FVector& InLocation) const
	{
		return ToWorldSpaceTransform.InverseTransformPosition(InLocation);
	}

	/**
	 * Converts a rotation from VM (global) space to world space
	 */
	FQuat ToWorldSpace(const FQuat& InRotation) const
	{
		return ToWorldSpaceTransform.TransformRotation(InRotation);
	}

	/**
	 * Converts a rotation from world space to VM (global) space
	 */
	FQuat ToVMSpace(const FQuat& InRotation) const
	{
		return ToWorldSpaceTransform.InverseTransformRotation(InRotation);
	}
	
	FRigVMNameCache* GetNameCache() const { return NameCache; }

#if WITH_EDITOR
	FRigVMLog* GetLog() const { return LogPtr; }
	void SetLog(FRigVMLog* InLog) { LogPtr = InLog; }
#endif

	FRigVMDrawInterface* GetDrawInterface() const { return DrawInterfacePtr; }
	void SetDrawInterface(FRigVMDrawInterface* InDrawInterface) { DrawInterfacePtr = InDrawInterface; }

	const FRigVMDrawContainer* GetDrawContainer() const { return DrawContainerPtr; }
	FRigVMDrawContainer* GetDrawContainer() { return DrawContainerPtr; }
	void SetDrawContainer(FRigVMDrawContainer* InDrawContainer) { DrawContainerPtr = InDrawContainer; }

	virtual void Initialize()
	{
		if(NameCache == nullptr)
		{
			static FRigVMNameCache StaticNameCache;
			NameCache = &StaticNameCache;
		}
	}

	virtual void Copy(const FRigVMExecuteContext* InOtherContext)
	{
		EventName = InOtherContext->EventName;
		FunctionName = InOtherContext->FunctionName;
		InstructionIndex = InOtherContext->InstructionIndex;
		RuntimeSettings = InOtherContext->RuntimeSettings;
		NameCache = InOtherContext->NameCache;
	}

	/**
	 * Serialize this type from another
	 */
	RIGVM_API bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot);

protected:

	virtual void Reset()
	{
		InstructionIndex = 0;
	}
	
	FName EventName;
	
	FName FunctionName;
	
	uint16 InstructionIndex;

	double DeltaTime;

	double AbsoluteTime;

	double FramesPerSecond;

	FRigVMRuntimeSettings RuntimeSettings;

	mutable FRigVMNameCache* NameCache;

#if WITH_EDITOR
	FRigVMLog* LogPtr;
#endif
	FRigVMDrawInterface* DrawInterfacePtr;
	FRigVMDrawContainer* DrawContainerPtr;

	/** The current transform going from rig (global) space to world space */
	FTransform ToWorldSpaceTransform;

	/** The current component this VM is owned by */
	const USceneComponent* OwningComponent;

	/** The current actor this VM is owned by */
	const AActor* OwningActor;

	/** The world this VM is running in */
	const UWorld* World;


#if UE_RIGVM_DEBUG_EXECUTION
public:
	bool bDebugExecution = false;
	FString DebugMemoryString;
protected:
	TArray<FString> PreviousWorkMemory;

	UEnum* InstanceOpCodeEnum;
#endif

private:
	// Pointer back to owner execute context. Required for lazy branches Execute
	FRigVMExtendedExecuteContext* ExtendedExecuteContext = nullptr; 

	friend struct TRigVMLazyValueBase;
	friend struct FRigVMPredicateBranch;
	friend struct FRigVMExtendedExecuteContext;
	friend class URigVM;
	friend class URigVMNativized;
};

template<>
struct TStructOpsTypeTraits<FRigVMExecuteContext> : public TStructOpsTypeTraitsBase2<FRigVMExecuteContext>
{
	enum
	{
		WithStructuredSerializeFromMismatchedTag = true,
	};
};

/**
 * Lazy branch data required by each instance of the VM
 */
struct RIGVM_API FRigVMLazyBranchInstanceData
{
	TArray<int32> LastVMNumExecutions;
};

struct RIGVM_API FRigVMExternalVariableRuntimeData
{
	explicit FRigVMExternalVariableRuntimeData(uint8* InMemory)
		: Memory(InMemory)
	{
	}

	uint8* Memory = nullptr;
};

/**
 * The execute context is used for mutable nodes to
 * indicate execution order.
 */
USTRUCT(Blueprintable)
struct RIGVM_API FRigVMExtendedExecuteContext
{
	GENERATED_BODY()

	FRigVMExtendedExecuteContext()
	: PublicDataScope(FRigVMExecuteContext::StaticStruct())
	{
		Reset();
		SetDefaultNameCache();
	}

	FRigVMExtendedExecuteContext(const UScriptStruct* InExecuteContextStruct)
		: PublicDataScope(InExecuteContextStruct)
	{
		if(InExecuteContextStruct)
		{
			check(InExecuteContextStruct->IsChildOf(FRigVMExecuteContext::StaticStruct()));
			Reset();
			SetDefaultNameCache();
		}
	}

	FRigVMExtendedExecuteContext(const FRigVMExtendedExecuteContext& InOther)
	{
		*this = InOther;
	}

	virtual ~FRigVMExtendedExecuteContext()
	{
		Reset();
	}

	// /** Full context reset */
	void Reset();

	/** Resets VM execution state */
	void ResetExecutionState();

	FRigVMExtendedExecuteContext& operator =(const FRigVMExtendedExecuteContext& Other);

	virtual void Initialize(const UScriptStruct* InScriptStruct);

	const UScriptStruct* GetContextPublicDataStruct() const
	{
		return Cast<UScriptStruct>(PublicDataScope.GetStruct());
	}

	void SetContextPublicDataStruct(const UScriptStruct* InScriptStruct)
	{
		if(GetContextPublicDataStruct() == InScriptStruct)
		{
			return;
		}
		Initialize(InScriptStruct);
	}

	template<typename ExecuteContextType = FRigVMExecuteContext>
	const ExecuteContextType& GetPublicData() const
	{
		check(PublicDataScope.GetStruct()->IsChildOf(ExecuteContextType::StaticStruct()));
		return *(const ExecuteContextType*)PublicDataScope.GetStructMemory();
	}

	template<typename ExecuteContextType = FRigVMExecuteContext>
	ExecuteContextType& GetPublicData()
	{
		check(PublicDataScope.GetStruct()->IsChildOf(ExecuteContextType::StaticStruct()));
		return *(ExecuteContextType*)PublicDataScope.GetStructMemory();
	}

	template<typename ExecuteContextType = FRigVMExecuteContext>
	ExecuteContextType& GetPublicDataSafe()
	{
		if(!PublicDataScope.GetStruct()->IsChildOf(ExecuteContextType::StaticStruct()))
		{
			Initialize(ExecuteContextType::StaticStruct());
		}
		return *(ExecuteContextType*)PublicDataScope.GetStructMemory();
	}

	const FRigVMSlice& GetSlice() const
	{
		return Slices.Last();
	}

	void BeginSlice(int32 InCount, int32 InRelativeIndex = 0)
	{
		ensure(!IsSliceComplete());
		Slices.Add(FRigVMSlice(InCount));
		Slices.Last().SetRelativeIndex(InRelativeIndex);
		Slices.Last().SetInstructionIndex(GetPublicData<>().InstructionIndex);
		Slices.Last().SetOffset(SliceOffsets[GetPublicData<>().InstructionIndex]);
	}

	void EndSlice()
	{
		ensure(Slices.Num() > 1);

		// if this slice has reached its upperbound
		// we want to make sure to increment the offset
		// for the instruction the slice originated from,
		// so that next time around the slice indices are not
		// reused.
		const FRigVMSlice PoppedSlice = Slices.Pop();
		if(PoppedSlice.GetRelativeIndex() == PoppedSlice.Num() - 1)
		{
			if(SliceOffsets.IsValidIndex(PoppedSlice.GetInstructionIndex()))
			{
				SliceOffsets[PoppedSlice.GetInstructionIndex()] += PoppedSlice.Num();
			}
		}
	}

	void IncrementSlice()
	{
		FRigVMSlice& ActiveSlice = Slices.Last();
		ActiveSlice++;
	}

	bool IsSliceComplete() const
	{
		return GetSlice().IsComplete();
	}

	bool IsValidArrayIndex(int32& InOutIndex, const FScriptArrayHelper& InArrayHelper) const
	{
		return IsValidArrayIndex(InOutIndex, InArrayHelper.Num());
	}

	bool IsValidArrayIndex(int32& InOutIndex, int32 InArraySize) const
	{
		const int32 InOriginalIndex = InOutIndex;

		// we support wrapping the index around similar to python
		if(InOutIndex < 0)
		{
			InOutIndex = InArraySize + InOutIndex;
		}

		if(InOutIndex < 0 || InOutIndex >= InArraySize)
		{
			static const TCHAR OutOfBoundsFormat[] = TEXT("Array Index (%d) out of bounds (count %d).");
			GetPublicData<>().Logf(EMessageSeverity::Error, OutOfBoundsFormat, InOriginalIndex, InArraySize);
			return false;
		}
		return true;
	}

	bool IsValidArraySize(int32 InSize) const
	{
		if(InSize < 0 || InSize > GetPublicData<>().RuntimeSettings.MaximumArraySize)
		{
			static const TCHAR OutOfBoundsFormat[] = TEXT("Array Size (%d) larger than allowed maximum (%d).\nCheck VMRuntimeSettings in class settings.");
			GetPublicData<>().Logf(EMessageSeverity::Error, OutOfBoundsFormat, InSize, GetPublicData<>().RuntimeSettings.MaximumArraySize);
			return false;
		}
		return true;
	}

	void SetRuntimeSettings(FRigVMRuntimeSettings InRuntimeSettings)
	{
		GetPublicData<>().RuntimeSettings = InRuntimeSettings;
		check(GetPublicData<>().RuntimeSettings.MaximumArraySize > 0);
	}

	void SetDefaultNameCache()
	{
		SetNameCache(&NameCache);
	}

	void SetNameCache(FRigVMNameCache* InNameCache)
	{
		GetPublicData<>().NameCache = InNameCache;
	}

	void InvalidateCachedMemory()
	{
		CachedMemory.Reset();
		CachedMemoryHandles.Reset();
		LazyBranchInstanceData.Reset();
	}

	uint32 GetNumExecutions() const
	{
		return NumExecutions;
	}

	UPROPERTY(transient)
	uint32 VMHash = MAX_uint32;

	FStructOnScope PublicDataScope;
	URigVM* VM = nullptr;
	TArray<FRigVMSlice> Slices;
	TArray<uint16> SliceOffsets;
	double LastExecutionMicroSeconds = 0.0;
	const FRigVMDispatchFactory* Factory = nullptr;
	FRigVMNameCache NameCache;

	UPROPERTY(transient)
	uint32 NumExecutions = 0;

	TArray<FRigVMMemoryHandle> CachedMemoryHandles;
	// changes to the layout of cached memory array should be reflected in GetContainerIndex()
	TArray<URigVMMemoryStorage*> CachedMemory;

	int32 ExecutingThreadId = INDEX_NONE;

	TArray<int32> EntriesBeingExecuted;

	ERigVMExecuteResult CurrentExecuteResult = ERigVMExecuteResult::Failed;
	FName CurrentEntryName = NAME_None;
	bool bCurrentlyRunningRootEntry = false;
	TArrayView<URigVMMemoryStorage*> CurrentMemory;

	UPROPERTY(transient)
	TObjectPtr<URigVM> DeferredVMToCopy = nullptr;
	const FRigVMExtendedExecuteContext* DeferredVMContextToCopy = nullptr;

	/** Bindable event for external objects to be notified when the VM reaches an Exit Operation */
	DECLARE_EVENT_OneParam(URigVM, FExecutionReachedExitEvent, const FName&);
	FExecutionReachedExitEvent OnExecutionReachedExit;
	FExecutionReachedExitEvent& ExecutionReachedExit() { return OnExecutionReachedExit; }

	TArray<FRigVMLazyBranchInstanceData> LazyBranchInstanceData;
	TArray<FRigVMExternalVariableRuntimeData> ExternalVariableRuntimeData;

#if WITH_EDITOR

	FRigVMDebugInfo* DebugInfo = nullptr;
	FRigVMBreakpoint HaltedAtBreakpoint;
	int32 HaltedAtBreakpointHit = INDEX_NONE;
	ERigVMBreakpointAction CurrentBreakpointAction = ERigVMBreakpointAction::None;

	// stores the number of times each instruction was visited
	TArray<int32> InstructionVisitedDuringLastRun;
	TArray<uint64> InstructionCyclesDuringLastRun;
	TArray<int32> InstructionVisitOrder;

	const void SetFirstEntryEventInEventQueue(const FName& InFirstEventName) { FirstEntryEventInQueue = InFirstEventName; }
	const FName& GetFirstEntryEventInEventQueue() { return FirstEntryEventInQueue; }

	// A RigVMHost can run multiple events per evaluation, such as the Backward&Forward Solve Mode,
	// store the first event such that we know when to reset data for a new round of rig evaluation
	FName FirstEntryEventInQueue = NAME_None;

	uint64 StartCycles = 0;
	uint64 OverallCycles = 0;

	DECLARE_EVENT_ThreeParams(URigVM, FExecutionHaltedEvent, int32, UObject*, const FName&);
	FExecutionHaltedEvent OnExecutionHalted;

	FExecutionHaltedEvent& ExecutionHalted()
	{
		return OnExecutionHalted;
	}

	void SetDebugInfo(FRigVMDebugInfo* InDebugInfo)
	{
		DebugInfo = InDebugInfo;
	}

#endif // WITH_EDITOR

};
