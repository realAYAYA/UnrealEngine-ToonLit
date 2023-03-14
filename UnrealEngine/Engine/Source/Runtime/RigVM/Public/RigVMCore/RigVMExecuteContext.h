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
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"
#include "Trace/Detail/Channel.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UnrealNames.h"
#include "UObject/UnrealType.h"

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

	FORCEINLINE FRigVMSlice()
	: LowerBound(0)
	, UpperBound(0)
	, Index(INDEX_NONE)
	{
		Reset();
	}

	FORCEINLINE FRigVMSlice(int32 InCount)
		: LowerBound(0)
		, UpperBound(InCount - 1)
		, Index(INDEX_NONE)
	{
		Reset();
	}

	FORCEINLINE FRigVMSlice(int32 InCount, const FRigVMSlice& InParent)
		: LowerBound(InParent.GetIndex() * InCount)
		, UpperBound((InParent.GetIndex() + 1) * InCount - 1)
		, Index(INDEX_NONE)
	{
		Reset();
	}

	FORCEINLINE bool IsValid() const
	{ 
		return Index != INDEX_NONE;
	}
	
	FORCEINLINE bool IsComplete() const
	{
		return Index > UpperBound;
	}

	FORCEINLINE int32 GetIndex() const
	{
		return Index;
	}

	FORCEINLINE void SetIndex(int32 InIndex)
	{
		Index = InIndex;
	}

	FORCEINLINE int32 GetRelativeIndex() const
	{
		return Index - LowerBound;
	}

	FORCEINLINE void SetRelativeIndex(int32 InIndex)
	{
		Index = InIndex + LowerBound;
	}

	FORCEINLINE float GetRelativeRatio() const
	{
		return float(GetRelativeIndex()) / float(FMath::Max<int32>(1, Num() - 1));
	}

	FORCEINLINE int32 Num() const
	{
		return 1 + UpperBound - LowerBound;
	}

	FORCEINLINE int32 TotalNum() const
	{
		return UpperBound + 1;
	}

	FORCEINLINE operator bool() const
	{
		return IsValid();
	}

	FORCEINLINE bool operator !() const
	{
		return !IsValid();
	}

	FORCEINLINE operator int32() const
	{
		return Index;
	}

	FORCEINLINE FRigVMSlice& operator++()
	{
		Index++;
		return *this;
	}

	FORCEINLINE FRigVMSlice operator++(int32)
	{
		FRigVMSlice TemporaryCopy = *this;
		++*this;
		return TemporaryCopy;
	}

	FORCEINLINE bool Next()
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

	FORCEINLINE void Reset()
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
struct RIGVM_API FRigVMExecuteContext
{
	GENERATED_BODY()

	FORCEINLINE FRigVMExecuteContext()
		: EventName(NAME_None)
		, FunctionName(NAME_None)
		, InstructionIndex(0)
		, RuntimeSettings()
	{
		Reset();
	}

	FORCEINLINE void Log(EMessageSeverity::Type InSeverity, const FString& InMessage) const
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
	FORCEINLINE void Logf(EMessageSeverity::Type InSeverity, const FmtType& Fmt, Types... Args) const
	{
		Log(InSeverity, FString::Printf(Fmt, Args...));
	}

	FORCEINLINE uint16 GetInstructionIndex() const { return InstructionIndex; }

	FORCEINLINE FName GetFunctionName() const { return FunctionName; }
	
	FORCEINLINE FName GetEventName() const { return EventName; }


protected:
	FORCEINLINE void CopyFrom(const FRigVMExecuteContext& Other)
	{
		EventName = Other.EventName;
		FunctionName = Other.FunctionName;
		InstructionIndex = Other.InstructionIndex;
		RuntimeSettings = Other.RuntimeSettings;
	}

private:

	FORCEINLINE void Reset()
	{
		InstructionIndex = 0;
	}
	
	FName EventName;
	
	FName FunctionName;
	
	uint16 InstructionIndex;

	FRigVMRuntimeSettings RuntimeSettings;

	friend struct FRigVMExtendedExecuteContext;
	friend class URigVM;
	friend class URigVMNativized;
	friend class UControlRig;
	friend class UAdditiveControlRig;
	friend struct FRigUnit_BeginExecution;
	friend struct FRigUnit_InverseExecution;
	friend struct FRigUnit_PrepareForExecution;
	friend struct FRigUnit_InteractionExecution;
	friend struct FRigUnit_UserDefinedEvent;
	friend struct FEngineTestRigVM_Begin;
	friend struct FEngineTestRigVM_Setup; 
};

/**
 * The execute context is used for mutable nodes to
 * indicate execution order.
 */
USTRUCT()
struct RIGVM_API FRigVMExtendedExecuteContext
{
	GENERATED_BODY()

	FORCEINLINE FRigVMExtendedExecuteContext()
		: VM(nullptr)
		, LastExecutionMicroSeconds()
		, Factory(nullptr)
	{
		Reset();
	}

	FORCEINLINE void Reset()
	{
		PublicData.Reset();
		VM = nullptr;
		Slices.Reset();
		Slices.Add(FRigVMSlice());
		SliceOffsets.Reset();
		Factory = nullptr;
	}

	FORCEINLINE void CopyFrom(const FRigVMExtendedExecuteContext& Other)
	{
		PublicData = Other.PublicData;
		VM = Other.VM;
		OpaqueArguments = Other.OpaqueArguments;
		Slices = Other.Slices;
		SliceOffsets = Other.SliceOffsets;
	}

	FORCEINLINE const FRigVMSlice& GetSlice() const
	{
		const int32 SliceOffset = (int32)SliceOffsets[PublicData.InstructionIndex];
		if (SliceOffset == 0)
		{
			return Slices.Last();
		}
		const int32 UpperBound = Slices.Num() - 1;
		return Slices[FMath::Clamp<int32>(UpperBound - SliceOffset, 0, UpperBound)];
	}

	FORCEINLINE void BeginSlice(int32 InCount, int32 InRelativeIndex = 0)
	{
		ensure(!IsSliceComplete());
		Slices.Add(FRigVMSlice(InCount, Slices.Last()));
		Slices.Last().SetRelativeIndex(InRelativeIndex);
	}

	FORCEINLINE void EndSlice()
	{
		ensure(Slices.Num() > 1);
		Slices.Pop();
	}

	FORCEINLINE void IncrementSlice()
	{
		FRigVMSlice& ActiveSlice = Slices.Last();
		ActiveSlice++;
	}

	FORCEINLINE bool IsSliceComplete() const
	{
		return GetSlice().IsComplete();
	}

	FORCEINLINE bool IsValidArrayIndex(int32& InOutIndex, const FScriptArrayHelper& InArrayHelper) const
	{
		return IsValidArrayIndex(InOutIndex, InArrayHelper.Num());
	}

	FORCEINLINE bool IsValidArrayIndex(int32& InOutIndex, int32 InArraySize) const
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
			PublicData.Logf(EMessageSeverity::Error, OutOfBoundsFormat, InOriginalIndex, InArraySize);
			return false;
		}
		return true;
	}

	FORCEINLINE bool IsValidArraySize(int32 InSize) const
	{
		if(InSize < 0 || InSize > PublicData.RuntimeSettings.MaximumArraySize)
		{
			static const TCHAR OutOfBoundsFormat[] = TEXT("Array Size (%d) larger than allowed maximum (%d).\nCheck VMRuntimeSettings in class settings.");
			PublicData.Logf(EMessageSeverity::Error, OutOfBoundsFormat, InSize, PublicData.RuntimeSettings.MaximumArraySize);
			return false;
		}
		return true;
	}

	FORCEINLINE void SetRuntimeSettings(FRigVMRuntimeSettings InRuntimeSettings)
	{
		PublicData.RuntimeSettings = InRuntimeSettings;
		check(PublicData.RuntimeSettings.MaximumArraySize > 0);
	}

	FRigVMExecuteContext PublicData;
	URigVM* VM;
	TArrayView<void*> OpaqueArguments;
	TArray<FRigVMSlice> Slices;
	TArray<uint16> SliceOffsets;
	double LastExecutionMicroSeconds;
	const FRigVMDispatchFactory* Factory;
};
