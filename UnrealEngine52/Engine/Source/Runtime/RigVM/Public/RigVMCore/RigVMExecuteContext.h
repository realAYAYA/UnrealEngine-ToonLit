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
	{
		Reset();
	}

	FRigVMSlice(int32 InCount)
		: LowerBound(0)
		, UpperBound(InCount - 1)
		, Index(INDEX_NONE)
	{
		Reset();
	}

	FRigVMSlice(int32 InCount, const FRigVMSlice& InParent)
		: LowerBound(InParent.GetIndex() * InCount)
		, UpperBound((InParent.GetIndex() + 1) * InCount - 1)
		, Index(INDEX_NONE)
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
		return Index;
	}

	void SetIndex(int32 InIndex)
	{
		Index = InIndex;
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

	int32 Num() const
	{
		return 1 + UpperBound - LowerBound;
	}

	int32 TotalNum() const
	{
		return UpperBound + 1;
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
};

USTRUCT()
struct RIGVM_API FRigVMRuntimeSettings
{
	GENERATED_BODY()

	/**
	 * The largest allowed size for arrays within the Control Rig VM.
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
#if !UE_RIGVM_DEBUG_EXECUTION
		else
#endif
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
	FString DebugMemoryString;

	TArray<FString> PreviousWorkMemory;

	UEnum* InstanceOpCodeEnum;
#endif

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
 * The execute context is used for mutable nodes to
 * indicate execution order.
 */
USTRUCT()
struct RIGVM_API FRigVMExtendedExecuteContext
{
	GENERATED_BODY()

	FRigVMExtendedExecuteContext()
	: PublicDataScope(FRigVMExecuteContext::StaticStruct())
	, VM(nullptr)
	, LastExecutionMicroSeconds()
	, Factory(nullptr)
	{
		Reset();
		SetDefaultNameCache();
	}

	FRigVMExtendedExecuteContext(const UScriptStruct* InExecuteContextStruct)
		: PublicDataScope(InExecuteContextStruct)
		, VM(nullptr)
		, LastExecutionMicroSeconds()
		, Factory(nullptr)
	{
		if(InExecuteContextStruct)
		{
			check(InExecuteContextStruct->IsChildOf(FRigVMExecuteContext::StaticStruct()));
			Reset();
			SetDefaultNameCache();
		}
	}

	FRigVMExtendedExecuteContext(const FRigVMExtendedExecuteContext& InOther)
		: PublicDataScope()
		, VM(nullptr)
		, LastExecutionMicroSeconds()
		, Factory(nullptr)
	{
		*this = InOther;
	}

	virtual ~FRigVMExtendedExecuteContext() {}

	void Reset()
	{
		((FRigVMExecuteContext*)PublicDataScope.GetStructMemory())->Reset();
		VM = nullptr;
		Slices.Reset();
		Slices.Add(FRigVMSlice());
		SliceOffsets.Reset();
		Factory = nullptr;
	}

	FRigVMExtendedExecuteContext& operator =(const FRigVMExtendedExecuteContext& Other)
	{
		const UScriptStruct* OtherPublicDataStruct = Cast<UScriptStruct>(Other.PublicDataScope.GetStruct());
		check(OtherPublicDataStruct);
		if(PublicDataScope.GetStruct() != OtherPublicDataStruct)
		{
			PublicDataScope = FStructOnScope(OtherPublicDataStruct);
		}

		FRigVMExecuteContext* ThisPublicContext = (FRigVMExecuteContext*)PublicDataScope.GetStructMemory();
		const FRigVMExecuteContext* OtherPublicContext = (const FRigVMExecuteContext*)Other.PublicDataScope.GetStructMemory();
		ThisPublicContext->Copy(OtherPublicContext);

		if(OtherPublicContext->GetNameCache() == &Other.NameCache)
		{
			SetDefaultNameCache();
		}

		VM = Other.VM;
		Slices = Other.Slices;
		SliceOffsets = Other.SliceOffsets;
		return *this;
	}

	virtual void Initialize(UScriptStruct* InScriptStruct)
	{
		check(InScriptStruct->IsChildOf(FRigVMExecuteContext::StaticStruct()));
		PublicDataScope = FStructOnScope(InScriptStruct);
		SetDefaultNameCache();
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
		const int32 SliceOffset = (int32)SliceOffsets[GetPublicData<>().InstructionIndex];
		if (SliceOffset == 0)
		{
			return Slices.Last();
		}
		const int32 UpperBound = Slices.Num() - 1;
		return Slices[FMath::Clamp<int32>(UpperBound - SliceOffset, 0, UpperBound)];
	}

	void BeginSlice(int32 InCount, int32 InRelativeIndex = 0)
	{
		ensure(!IsSliceComplete());
		Slices.Add(FRigVMSlice(InCount, Slices.Last()));
		Slices.Last().SetRelativeIndex(InRelativeIndex);
	}

	void EndSlice()
	{
		ensure(Slices.Num() > 1);
		Slices.Pop();
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

	FStructOnScope PublicDataScope;
	URigVM* VM;
	TArray<FRigVMSlice> Slices;
	TArray<uint16> SliceOffsets;
	double LastExecutionMicroSeconds;
	const FRigVMDispatchFactory* Factory;
	FRigVMNameCache NameCache;
};
