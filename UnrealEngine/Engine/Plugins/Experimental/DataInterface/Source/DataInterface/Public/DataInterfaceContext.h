// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "DataInterfaceParam.h"
#include "DataInterfaceState.h"
#include "DataInterfaceKey.h"
#include "Misc/MemStack.h"

class IDataInterface;
class IDataInterfaceParameters;

namespace UE::DataInterface
{

struct FState;
enum class EStatePersistence : uint8;

// Helper to provide a non-callstack bridge for interfacing with data interface contexts
// Declaring one of these on the stack will enable all calls in its scope to access the passed-in context
// via FThreadContext::Get()
struct DATAINTERFACE_API FThreadContext
{
	FThreadContext(const FContext& InContext);
	~FThreadContext();

	static const FContext& Get();
};

// Context providing methods for mutating & interrogating the data interface runtime
struct DATAINTERFACE_API FContext
{
private:
	friend class ::IDataInterface;
	friend struct FState;

public:
	// Root public constructor. Constructs a context given a state.
	// Further usage must use WithResult etc. to allow writable results to be used or param
	FContext(float InDeltaTime, FState& InState, IDataInterfaceParameters* InParameters = nullptr);

	FContext WithResult(FParam& InResult) const;

	FContext WithParameter(FName ParameterId, const FParam& InParameter) const;

	FContext WithParameters(TArrayView<const TPair<FName, FParam>> InParameters) const;
	
	FContext WithParameters(IDataInterfaceParameters* InParameters) const;
	
	bool GetParameter(FName InKey, FParam& OutParam) const;

public:
	// Gets (and allocates, if necessary) state for the specified node given this calling context
	template<typename ValueType, EStatePersistence Persistence = EStatePersistence::Relevancy>
	TParam<ValueType> GetState(const FInterfaceKeyWithId& InKey) const
	{
		return State.GetState<ValueType, Persistence>(InKey, *this, CallstackHash);
	}

	// Gets (and allocates, if necessary) state for the specified node given this calling context
	template<typename ValueType, EStatePersistence Persistence = EStatePersistence::Relevancy>
	TParam<ValueType> GetState(const IDataInterface* InDataInterface, uint32 InId) const
	{
		return State.GetState<ValueType, Persistence>(InDataInterface, InId, *this, CallstackHash);
	}
	
public:
	// Set a result directly
	template<typename ValueType>
	void SetResult(const ValueType& InValue) const
	{
		if(Result != nullptr)
		{
			// Support containers with Num() and GetData()
			if constexpr (TModels<Private::CSizedContainerWithAccessibleDataAsRawPtr, ValueType>::Value)
			{
				checkSlow(Private::CheckParam<ValueType>(Result));
				checkSlow(InValue.Num() == State.ChunkSize);

				TArrayView<ValueType> Results(static_cast<ValueType*>(Result->Data), State.NumElements);
				const ValueType* Data = InValue.GetData();
				for(int32 ResultIndex = 0; ResultIndex < State.NumElements; ++ResultIndex)
				{
					Results[ResultIndex] = Data[ResultIndex];
				}
			}
			else
			{
				checkSlow(Private::CheckParam<ValueType>(Result));

				TArrayView<ValueType> Results(static_cast<ValueType*>(Result->Data), State.NumElements);
				for(ValueType& ResultElement : Results)
				{
					ResultElement = InValue;
				}
			}
		}
	}
	
	// Get the current result as a specified type
	template<typename ValueType>
	TParam<ValueType> GetResult() const
	{
		// Cannot call this without a result being present
		check(Result);
		
		// Types must match
		checkSlow(Private::CheckParam<ValueType>(*Result));
		return TParam<ValueType>(*Result, GetChunkFlags());
	}
	
	// Get the current result as a raw parameter
	const FParam& GetResultRaw() const
	{
		// Cannot call this without a result being present
		check(Result);
		return *Result;
	}

public:
	// Number of elements that this context represents
	uint32 GetNum() const
	{
		return State.NumElements;
	}

	// Max number of elements that this context represents
	uint32 GetChunkSize() const
	{
		return State.ChunkSize;
	}

	// Access delta time as a param
	TParam<const float> GetDeltaTimeParam() const;

	// Raw access to delta time
	float GetDeltaTime() const { return DeltaTime; }
	
	// Get parameter flags for chunking
	FParam::EFlags GetChunkFlags() const
	{
		return State.ChunkSize > 1 ? FParam::EFlags::Chunked : FParam::EFlags::None;
	}

private:
	FContext(float InDeltaTime, FState& InState, FParam* InResult)
		: State(InState)
		, Result(InResult)
		, DeltaTime(InDeltaTime)
	{}

	FContext WithCallRaw(const IDataInterface* InDataInterface) const;

	void FlushRelevancy() const;

private:
	TMap<FName, FParam> AdditionalParameters;
	const FContext* Parent = nullptr;
	const FContext* Root = nullptr;
	FState& State;
	FParam* Result = nullptr;
	IDataInterfaceParameters* Parameters = nullptr;
	float DeltaTime = 0.0f;
	uint32 CallstackHash = 0;
	uint32 UpdateCounter = 0;
};

// A typed param that wraps a user ptr
template<typename ValueType>
struct TWrapParam : TParam<ValueType>
{
public:
	TWrapParam(const FContext& InContext, ValueType* InValuePtrToWrap)
		: TParam<ValueType>(GetDataForConstructor(InContext, InValuePtrToWrap), GetFlagsForConstructor(InContext, InValuePtrToWrap))
	{
	}

private:
	void* GetDataForConstructor(const FContext& InContext, ValueType* InValuePtrToWrap) const
	{
		using MutableValueType = typename TRemoveConst<ValueType>::Type;
		
		// Support containers with Num() and GetData()
		if constexpr (TModels<Private::CSizedContainerWithAccessibleDataAsRawPtr, ValueType>::Value)
		{
			checkSlow(InContext.GetChunkSize() == InValuePtrToWrap->Num());
			return const_cast<void*>(InValuePtrToWrap->GetData());
		}
		else
		{
			checkSlow(InContext.GetChunkSize() == 1);
			return const_cast<MutableValueType*>(InValuePtrToWrap);
		}
	}

	FParam::EFlags GetFlagsForConstructor(const FContext& InContext, ValueType* InValuePtrToWrap) const
	{
		FParam::EFlags NewFlags = FParam::EFlags::None;
		
		// Add chunked flags or not
		if constexpr (TModels<Private::CSizedContainerWithAccessibleDataAsRawPtr, ValueType>::Value)
		{
			// Containers always assume chunked
			checkSlow(InContext.GetChunkSize() == InValuePtrToWrap->Num());
			NewFlags |= InContext.GetChunkFlags();
		}
		else
		{
			checkSlow(InContext.GetChunkSize() == 1);
		}

		// Add const flags or not
		if constexpr (!TIsConst<ValueType>::Value)
		{
			NewFlags |= FParam::EFlags::Mutable;
		}

		return NewFlags;
	}
};

// A typed param that owns it's own memory
template<typename ValueType, typename AllocatorType = TMemStackAllocator<>>
struct TAllocParam : TParam<ValueType>
{
public:
	TAllocParam(const FContext& InContext)
		: TParam<ValueType>(GetDataForConstructor(InContext), InContext.GetChunkFlags())
	{
	}

private:
	void* GetDataForConstructor(const FContext& InContext)
	{
		ValueArray.SetNum(InContext.GetChunkSize());
		return ValueArray.GetData();
	}

	TArray<ValueType, AllocatorType> ValueArray;
};

}