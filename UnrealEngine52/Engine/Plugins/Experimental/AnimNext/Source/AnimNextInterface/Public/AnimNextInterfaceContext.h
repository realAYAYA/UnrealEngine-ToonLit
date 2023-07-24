// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "AnimNextInterfaceParam.h"
#include "AnimNextInterfaceState.h"
#include "AnimNextInterfaceParamStorage.h"
#include "AnimNextInterfaceKey.h"
#include "Misc/MemStack.h"

class IAnimNextInterface;
class IAnimNextInterfaceParameters;

namespace UE::AnimNext::Interface
{

struct FState;
enum class EStatePersistence : uint8;

// Helper to provide a non-callstack bridge for interfacing with anim interface contexts
// Declaring one of these on the stack will enable all calls in its scope to access the passed-in context
// via FThreadContext::Get()
struct ANIMNEXTINTERFACE_API FThreadContext
{
	FThreadContext(const FContext& InContext);
	~FThreadContext();

	static const FContext& Get();
};

// Context providing methods for mutating & interrogating the anim interface runtime
struct ANIMNEXTINTERFACE_API FContext
{
private:
	friend class ::IAnimNextInterface;
	friend struct FState;

public:
	static constexpr int32 MAX_BATCH_ELEMENTS = 64;

	// Root public constructor. Constructs a context given a state.
	FContext(float InDeltaTime, FState& InState, FParamStorage& InParamStorage, IAnimNextInterfaceParameters* InParameters = nullptr);
	
	FContext(const FContext& other) = delete;
	FContext& operator=(const FContext&) = delete;

	FContext(FContext&& Other)
		: FContext()
	{
		Swap(*this, Other);
	}

	FContext& operator= (FContext&& Other)
	{
		Swap(*this, Other);
		return *this;
	}

	~FContext();

public:
	// --- Sub Context Creation --- 

	// Create a sub context from this one that includes the provided Result
	FContext WithResult(FParam& InResult) const;

	// Create a sub context from this one that includes the provided parameter
	FContext WithParameter(FName ParameterId, const FParam& InParameter) const;

	// Create a sub context from this one that includes the provided parameters
	FContext WithParameters(TArrayView<const TPair<FName, FParam>> InParameters) const;

	// Create a sub context from this one that includes the provided Result and parameters
	FContext WithResultAndParameters(FParam& InResult, TArrayView<const TPair<FName, FParam>> InParameters) const;

	// Create a sub context from this one that includes the provided interface parameter
	FContext WithParameters(IAnimNextInterfaceParameters* InParameters) const;

public:

	// --- Interface for direct Param storage (prototype) ---

	FContext CreateSubContext() const;

	enum class EParamType : uint8
	{
		None		= 0,
		Input		= 1 << 0,
		Output		= 1 << 1,
	};

	// Add a parameter by value, copying it to the shared storage
	template<typename ValueType>
	FHParam AddValue(EParamType InParamType, FName ParameterId, ValueType &Value)
	{
		// Values are added mutable even if source is const
		const FParam::EFlags Flags = FParam::EFlags::Value | FParam::EFlags::Mutable;

		return AddParameter(InParamType, ParameterId, Flags, &Value);
	}

	// Add a parameter by value, copying it to the shared storage
	template<typename ValueType>
	FHParam AddValue(EParamType InParamType, FName ParameterId, ValueType&& Value)
	{
		// Values are added mutable even if source is const
		const FParam::EFlags Flags = FParam::EFlags::Value | FParam::EFlags::Mutable;

		return AddParameter(InParamType, ParameterId, Flags, &Value);
	}

	// Add a parameter by value, copying it to the shared storage
	template<typename ValueType>
	FHParam AddValue(EParamType InParamType, FName ParameterId, ValueType* Value)
	{
		// Values are added mutable even if source is const
		const FParam::EFlags Flags = FParam::EFlags::Value | FParam::EFlags::Mutable;

		return AddParameter(InParamType, ParameterId, Flags, Value);
	}

	// Add a parameter by reference, adding just a pointer to the shared storage
	template<typename ValueType>
	FHParam AddReference(EParamType InParamType, FName ParameterId, ValueType& Value)
	{
		const FParam::EFlags Flags = FParam::EFlags::Reference;

		return AddParameter(InParamType, ParameterId, Flags, &Value);
	}

	// Add a parameter by reference, adding just a pointer to the shared storage
	template<typename ValueType>
	FHParam AddReference(EParamType InParamType, FName ParameterId, ValueType *Value)
	{
		FParam::EFlags Flags = FParam::EFlags::Reference;

		return AddParameter(InParamType, ParameterId, Flags, Value);
	}

	template<typename ValueType>
	FHParam AddParameter(EParamType InParamType, FName ParameterId, FParam::EFlags Flags, ValueType* Value)
	{
		FHParam ParamHandle;

		check(InParamType == EParamType::Input || InParamType == EParamType::Output);
		check(EnumHasAnyFlags(Flags, FParam::EFlags::Value) || EnumHasAnyFlags(Flags, FParam::EFlags::Reference));

		// check if it is a simple FHParam copy
		if constexpr (TIsDerivedFrom<ValueType, FHParam>::Value)
		{
			const FParam* ExistingParam = ParamStorage->GetParam(Value->ParamHandle);
			check(ExistingParam != nullptr);

			const bool bIsReadOnly = EnumHasAnyFlags(ExistingParam->Flags, FParam::EFlags::Mutable) == false;

			// Verify that we want to create an Input param or if we want an output, the source is not read only
			check(InParamType == EParamType::Input || bIsReadOnly == false);

			// For now create a copy of the Param (TODO : change ref to value or value to ref, if the caller requests it?)
			ParamHandle = *Value;
		}
		else
		{
			using MutableValueType = typename TRemoveConst<ValueType>::Type;

			// If non const set the mutable flag
			if constexpr (TIsConst<ValueType>::Value == false)
			{
				EnumAddFlags(Flags, FParam::EFlags::Mutable);
			}

			// deal with containers
			if constexpr (TModels<Private::CSizedContainerWithAccessibleDataAsRawPtr, ValueType>::Value)
			{
				MutableValueType* ParamData = const_cast<MutableValueType>(static_cast<ValueType*>(Value->GetData()));
				ParamHandle = EnumHasAnyFlags(Flags, FParam::EFlags::Value) 
					? ParamHandle = ParamStorage->AddValue(ParamData, Value->Num(), Flags)
					: ParamStorage->AddReference(ParamData, Value->Num(), Flags);
			}
			// and finally with single params
			else
			{
				ParamHandle = EnumHasAnyFlags(Flags, FParam::EFlags::Value)
					? ParamHandle = ParamStorage->AddValue(const_cast<MutableValueType*>(Value), 1, Flags)
					: ParamStorage->AddReference(const_cast<MutableValueType*>(Value), 1, Flags);
			}
		}

		AdditionalParameterHandles.Add(ParameterId, ParamHandle);

		return ParamHandle;
	}

	// Get a TParam from a handle
	template<typename ValueType>
	TParam<ValueType> GetParameterChecked(const FHParam &InParamHandle) const
	{
		const FParam* Param = ParamStorage->GetParam(InParamHandle.ParamHandle);

		TParam<ValueType> RetVal(Param, Param->GetFlags());

		check(RetVal.IsValid());							// found something
		checkSlow(Private::CheckParam<ValueType>(RetVal));	// the type found is valid for what was requested

		return RetVal;
	}

	// Get a parameter from a handle as a specified type
	template <typename ValueType>
	ValueType& GetParameterAs(const FHParam& InParamHandle, int32* OutNumElems = nullptr)
	{
		const FParam* Param = ParamStorage->GetParam(InParamHandle.ParamHandle);

		check(Param != nullptr);
		check(Param->GetType().GetTypeId() == Private::TParamType<ValueType>::GetType().GetTypeId());

		// If a non cost is requested, check the param has mutable flag
		if constexpr (TIsConst<ValueType>::Value == false)
		{
			check(Param->IsMutable());
		}

		ValueType* ValueData = EnumHasAnyFlags(Param->Flags, FParam::EFlags::Embedded)
			? static_cast<ValueType*>((void*)&Param->Data)
			: static_cast<ValueType*>(Param->Data);

		if (OutNumElems != nullptr)
		{
			*OutNumElems = Param->GetNumElements();
		}

		return *ValueData;
	}

	// --- Parameter management --- 

	// Get a parameter if exist, returns false if not
	bool GetParameter(FName InKey, FParam& OutParam) const;

	// Get a parameter as a specified type, checking it exist and the type
	template<typename ValueType>
	TParam<ValueType> GetParameterChecked(FName InKey) const
	{
		TParam<ValueType> RetVal(GetBatchedFlags());

		GetParameter(InKey, RetVal);

		check(RetVal.IsValid());							// found something
		checkSlow(Private::CheckParam<ValueType>(RetVal));	// the type found is valid for what was requested

		return RetVal;
	}

	// Get a parameter as a specified type, returning a TOptional
	template<typename ValueType>
	TOptional<TParam<ValueType>> GetParameter(FName InKey) const
	{
		TOptional<TParam<ValueType>> RetVal;

		TParam<ValueType> Param(FParam::EFlags::Mutable | GetBatchedFlags());
		if (GetParameter(InKey, Param))
		{
			checkSlow(Private::CheckParam<ValueType>(Param));

			RetVal = Param;
		}

		return RetVal;
	}

public:
	// --- Result Management ---

	// Get the ResultParam, checking it has been set
	FParam& GetResultParam()
	{
		check(Result != nullptr);
		return *Result;
	}

	// Get the ResultParam, checking it has been set
	FParam& GetResultParam() const
	{
		check(Result != nullptr);
		return *Result;
	}

	// Set a HParam as a result (for compatibility reasons)
	void SetHParamAsResult(const FHParam& InHParam)
	{
		Result = ParamStorage->GetParam(InHParam.ParamHandle);
	}

	// Set a result value directly
	// The receiver Result Param must be set prior to this call
	template<typename ValueType>
	void SetResult(const ValueType& InValue) const
	{
		// Support containers with Num() and GetData()
		if constexpr (TModels<Private::CSizedContainerWithAccessibleDataAsRawPtr, ValueType>::Value)
		{
			checkSlow(Private::CheckParam<ValueType>(*Result));
			checkSlow(InValue.Num() == Result->NumElements);

			TArrayView<ValueType> Results(*static_cast<ValueType*>(Result->Data), Result->NumElements);
			const ValueType* Data = InValue.GetData();
			for(int32 ResultIndex = 0; ResultIndex < Result->NumElements; ++ResultIndex)
			{
				Results[ResultIndex] = Data[ResultIndex];
			}
		}
		else
		{
			checkSlow(Private::CheckParam<ValueType>(*Result));
			checkSlow(Result->NumElements == 1);

			TArrayView<ValueType> Results(static_cast<ValueType*>(Result->Data), Result->NumElements);
			for(ValueType& ResultElement : Results)
			{
				ResultElement = InValue;
			}
		}
	}

	// Get the current result as a TParam
	template<typename ValueType>
	TParam<ValueType> GetResult() const
	{
		// Types must match
		checkSlow(Private::CheckParam<ValueType>(*Result));
		return TParam<ValueType>(*Result, GetBatchedFlags());
	}

	// Get the current result as a specified raw type pointer
	template<typename ValueType>
	ValueType* GetResultChecked() const
	{
		// Types must match
		checkSlow(Private::CheckParam<ValueType>(*Result));
		return static_cast<ValueType*>(Result->Data);
	}

public:
	// --- State management ---

	// Gets (and allocates, if necessary) state for the specified node given this calling context
	template<typename ValueType, EStatePersistence Persistence = EStatePersistence::Relevancy>
	TParam<ValueType> GetState(const FInterfaceKeyWithId& InKey) const
	{
		return State->GetState<ValueType, Persistence>(InKey, *this, CallstackHash);
	}

	// Gets (and allocates, if necessary) state for the specified node given this calling context
	template<typename ValueType, EStatePersistence Persistence = EStatePersistence::Relevancy>
	TParam<ValueType> GetState(const IAnimNextInterface* InAnimNextInterface, uint32 InId) const
	{
		return State->GetState<ValueType, Persistence>(InAnimNextInterface, InId, *this, CallstackHash);
	}

public:
	// --- Mix Context Utils ---

	// Number of elements that this context represents
	int32 GetNum() const
	{
		return State->NumElements;
	}

	// Access delta time as a param
	TParam<const float> GetDeltaTimeParam() const;

	// Raw access to delta time
	float GetDeltaTime() const { return DeltaTime; }

	// --- Batching management ---

	// Get parameter flags for batching
	FParam::EFlags GetBatchedFlags() const
	{
		return State->NumElements > 1 ? FParam::EFlags::Batched : FParam::EFlags::None;
	}

	FORCEINLINE_DEBUGGABLE bool ShouldProcessThisElement(int32 ElementIndex) const
	{
		check(ElementIndex < MAX_BATCH_ELEMENTS);
		return (BranchingMask & (1ull << ElementIndex)) != 0;
	}

	FORCEINLINE_DEBUGGABLE const uint64 GetBranchingMask() const
	{
		return BranchingMask;
	}

	void SetBranchingMask(const TArrayView<int32> InEnabledElementIndexes)
	{
		check(InEnabledElementIndexes.Num() <= GetNum());

		BranchingMask = 0;
		for (int32 ElementIndex : InEnabledElementIndexes)
		{
			check(ElementIndex < MAX_BATCH_ELEMENTS);

			BranchingMask |= (1ull << ElementIndex);
		}
	}

	void SetBranchingMask(uint64 InBranchingMask)
	{
		BranchingMask = InBranchingMask;
	}

private:
	FContext(float InDeltaTime, FState& InState, FParamStorage& InParamStorage, FParam& InResult)
		: State(&InState)
		, ParamStorage(&InParamStorage)
		, Result(&InResult)
		, DeltaTime(InDeltaTime)
		, BranchingMask(MAX_uint64)
	{}

	FContext() = default;

	FContext WithCallRaw(const IAnimNextInterface* InAnimNextInterface) const;

	void FlushRelevancy() const;

	int32 GetParametersSize(TArrayView<const TPair<FName, FParam>> InParameters, TArray<int32>& ParamAllocSizes) const;
	void AddParameters(TArrayView<const TPair<FName, FParam>> InParameters);

private:
	TMap<FName, FParam> AdditionalParameters;
	TMap<FName, FHParam> AdditionalParameterHandles;  // Temp param storage prototype
	const FContext* Parent = nullptr;
	const FContext* Root = nullptr;
	FState* State = nullptr;
	FParamStorage* ParamStorage = nullptr;
	FParamStorageHandle BlockHandle = InvalidBlockHandle;
	FParam* Result = nullptr;
	IAnimNextInterfaceParameters* Parameters = nullptr;
	float DeltaTime = 0.0f;
	uint32 CallstackHash = 0;
	uint32 UpdateCounter = 0;
	uint64 BranchingMask = MAX_uint64;
};

// A typed param that owns it's own memory
template<typename ValueType, typename AllocatorType = TMemStackAllocator<>>
struct TAllocParam : public TParam<ValueType>
{
public:
	TAllocParam(const FContext& InContext)
		: TParam<ValueType>(InContext.GetBatchedFlags())
	{
		TParam<ValueType>::Data = GetDataForConstructor(InContext);
	}

	TAllocParam(const FContext& InContext, FParam::EFlags InFlags)
		: TParam<ValueType>(InFlags)
	{
		TParam<ValueType>::Data = GetDataForConstructor(InContext);
	}


private:
	void* GetDataForConstructor(const FContext& InContext)
	{
		ValueArray.SetNum(InContext.GetNum());
		return ValueArray.GetData();
	}

	TArray<ValueType, AllocatorType> ValueArray;
};


// --- Batching helper macros ---

#define SKIP_CONTEXT_ELEMENT_IF_MASKED(Context) \
		if (!Context.ShouldProcessThisElement(i)) \
			continue;

// Helper macros to allow processing context element batches
#define PROCESS_BATCH_ELEMENTS_START(Context)  \
	const int32 ContextNumElements = Context.GetNum(); \
	for (int32 i = 0; i < ContextNumElements; i++) \
	{ \
		SKIP_CONTEXT_ELEMENT_IF_MASKED(Context)

#define PROCESS_BATCH_ELEMENTS_END }

}
