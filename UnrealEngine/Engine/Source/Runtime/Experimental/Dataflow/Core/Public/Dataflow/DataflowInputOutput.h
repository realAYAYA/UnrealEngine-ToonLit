// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "Dataflow/DataflowConnection.h"
#include "Dataflow/DataflowNode.h"
#include "Templates/Function.h"
#include "Async/Async.h"
#include "GenericPlatform/GenericPlatformCriticalSection.h"

#include "DataflowInputOutput.generated.h"


struct FDataflowOutput;

//
//  Input
//
namespace Dataflow
{
	struct FInputParameters {
		FInputParameters(FName InType = FName(""), FName InName = FName(""), FDataflowNode* InOwner = nullptr, const FProperty* InProperty = nullptr)
			: Type(InType)
			, Name(InName)
			, Owner(InOwner)
			, Property(InProperty){}
		FName Type;
		FName Name;
		FDataflowNode* Owner = nullptr;
		const FProperty* Property = nullptr;
	};
}

USTRUCT()
struct FDataflowInput : public FDataflowConnection
{
	GENERATED_USTRUCT_BODY()

	static FDataflowInput NoOpInput;

	friend struct FDataflowConnection;

	FDataflowOutput* Connection;
public:
	FDataflowInput(const Dataflow::FInputParameters& Param = {}, FGuid InGuid = FGuid::NewGuid());

	virtual bool AddConnection(FDataflowConnection* InOutput) override;
	virtual bool RemoveConnection(FDataflowConnection* InOutput) override;

	FDataflowOutput* GetConnection() { return Connection; }
	const FDataflowOutput* GetConnection() const { return Connection; }

	virtual TArray< FDataflowOutput* > GetConnectedOutputs();
	virtual const TArray< const FDataflowOutput* > GetConnectedOutputs() const;

	template<class T>
	const T& GetValue(Dataflow::FContext& Context, const T& Default) const;

	template<class T>
	TFuture<const T&> GetValueParallel(Dataflow::FContext& Context, const T& Default) const;

	virtual void Invalidate(const Dataflow::FTimestamp& ModifiedTimestamp = Dataflow::FTimestamp::Current()) override;
};

//
// Output
//
namespace Dataflow
{
	struct FOutputParameters
	{
		FOutputParameters(FName InType = FName(""), FName InName = FName(""), FDataflowNode* InOwner = nullptr, const FProperty* InProperty = nullptr)
			: Type(InType)
			, Name(InName)
			, Owner(InOwner)
			, Property(InProperty) {}

		FName Type;
		FName Name;
		FDataflowNode* Owner = nullptr;
		const FProperty* Property = nullptr;
	};
}
USTRUCT()
struct FDataflowOutput : public FDataflowConnection
{
	GENERATED_USTRUCT_BODY()

	friend struct FDataflowConnection;
	
	TArray< FDataflowInput* > Connections;

	uint32 PassthroughOffset = INDEX_NONE;

public:
	static DATAFLOWCORE_API FDataflowOutput NoOpOutput;
	
	mutable TSharedPtr<FCriticalSection> OutputLock;
	
	DATAFLOWCORE_API FDataflowOutput(const Dataflow::FOutputParameters& Param = {}, FGuid InGuid = FGuid::NewGuid());

	DATAFLOWCORE_API TArray<FDataflowInput*>& GetConnections();
	DATAFLOWCORE_API const TArray<FDataflowInput*>& GetConnections() const;

	DATAFLOWCORE_API virtual TArray<FDataflowInput*> GetConnectedInputs();
	DATAFLOWCORE_API virtual const TArray<const FDataflowInput*> GetConnectedInputs() const;

	DATAFLOWCORE_API virtual bool AddConnection(FDataflowConnection* InOutput) override;

	DATAFLOWCORE_API virtual bool RemoveConnection(FDataflowConnection* InInput) override;

	virtual FORCEINLINE void SetPassthroughOffset(const uint32 InPassthroughOffset)
	{
		PassthroughOffset = InPassthroughOffset;
	}

	virtual FORCEINLINE void* GetPassthroughRealAddress() const
	{
		if(PassthroughOffset != INDEX_NONE)
		{
			return (void*)((size_t)OwningNode + (size_t)PassthroughOffset);
		}
		return nullptr;
	}
 
	template<class T>
	void SetValue(T&& InVal, Dataflow::FContext& Context) const
	{
		if (Property)
		{
			Context.SetData(CacheKey(), Property, Forward<T>(InVal));
		}
	}

	template<class T> const T& GetValue(Dataflow::FContext& Context, const T& Default) const
	{
		if (!this->Evaluate<T>(Context))
		{
			Context.SetData(CacheKey(), Property, Default);
		}

		if (Context.HasData(CacheKey()))
		{
			return Context.GetData(CacheKey(), Property, Default);
		}

		return Default;
	}

	DATAFLOWCORE_API bool EvaluateImpl(Dataflow::FContext& Context) const;
	
	template<class T>
	bool Evaluate(Dataflow::FContext& Context) const;

	template<class T>
	TFuture<bool> EvaluateParallel(Dataflow::FContext& Context) const;

	DATAFLOWCORE_API virtual void Invalidate(const Dataflow::FTimestamp& ModifiedTimestamp = Dataflow::FTimestamp::Current()) override;

};
 
template<class T>
const T& FDataflowInput::GetValue(Dataflow::FContext& Context, const T& Default) const
{
	if (GetConnectedOutputs().Num())
	{
		ensure(GetConnectedOutputs().Num() == 1);
		if (const FDataflowOutput* ConnectionOut = GetConnection())
		{
			if (!ConnectionOut->Evaluate<T>(Context))
			{
				Context.SetData(ConnectionOut->CacheKey(), Property, Default);
			}
			if (Context.HasData(ConnectionOut->CacheKey()))
			{
				const T& data = Context.GetData(ConnectionOut->CacheKey(), Property, Default);
				return data;
			}
		}
	}
	return Default;
}

template<class T>
TFuture<const T&> FDataflowInput::GetValueParallel(Dataflow::FContext& Context, const T& Default) const
{
	return Async(EAsyncExecution::TaskGraph, [&]() -> const T& { return this->GetValue<T>(Context, Default); });
}

template<class T>
bool FDataflowOutput::Evaluate(Dataflow::FContext& Context) const
{
	check(OwningNode);
 
	if (OwningNode->bActive)
	{
		return Context.Evaluate(*this);
	}
	else if(const FDataflowInput* PassthroughInput = OwningNode->FindInput(GetPassthroughRealAddress()))
	{
		// @todo(dataflow) would be nice if the passthrough does not overwrite the existing cache value.
		const T& PassthroughData = PassthroughInput->GetValue<T>(Context, *reinterpret_cast<const T*>(PassthroughInput->RealAddress()));
		SetValue(PassthroughData, Context);
		return true;
	}
 
	return false;
}

template<class T>
TFuture<bool> FDataflowOutput::EvaluateParallel(Dataflow::FContext& Context) const
{
	return Async(EAsyncExecution::TaskGraph, [&]() -> bool { return this->Evaluate<T>(Context); });
}
