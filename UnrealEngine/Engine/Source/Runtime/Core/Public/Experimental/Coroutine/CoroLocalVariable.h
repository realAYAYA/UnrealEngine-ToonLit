// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"
#include "Experimental/Containers/RobinHoodHashTable.h"
#include "CoroutineHandle.h"

namespace CoroTask_Detail
{
	class FPromise;

	struct FCoroLocal
	{};

	class FCoroLocalVariable
	{
	public:
		virtual ~FCoroLocalVariable() {};
	};

	class FCoroLocalState
	{
	public:
		FCoroLocalState(const CoroTask_Detail::FPromise& InTaskOwner) 
			: TaskOwner(InTaskOwner)
		{};

		static CORE_API FCoroLocalState& GetCoroLocalState();
		static CORE_API FCoroLocalState* SetCoroLocalState(FCoroLocalState*);
		static CORE_API uint64 GenerateCoroId();
		static CORE_API bool IsCoroLaunchedTask();

		const CoroTask_Detail::FPromise& TaskOwner;

		using TlsDictionaryType = Experimental::TRobinHoodHashMap<const FCoroLocal*, TUniquePtr<FCoroLocalVariable>>;
		TlsDictionaryType CoroLocalStorage;
	};

	template<typename ValueType>
	class TCoroLocalVariable final : public FCoroLocalVariable
	{
		ValueType Value;

	public:
		TCoroLocalVariable(const ValueType& DefaultValue) : Value(DefaultValue)
		{
		}

		virtual ~TCoroLocalVariable() override
		{}

		ValueType& GetValue()
		{
			return Value;
		}
	};
}

/*
* TCoroLocal is an implementation of Coroutine Local State similar to TLS
*/
template<typename ValueType>
class TCoroLocal : private CoroTask_Detail::FCoroLocal
{
	using VariableType = CoroTask_Detail::TCoroLocalVariable<ValueType>;
	ValueType DefaultValue;

	inline ValueType& GetValue() const
	{
		using namespace CoroTask_Detail;
		FCoroLocalState::TlsDictionaryType& ClsDict = FCoroLocalState::GetCoroLocalState().CoroLocalStorage;
		TUniquePtr<CoroTask_Detail::FCoroLocalVariable>& Variable = *ClsDict.FindOrAdd(this, TUniquePtr<VariableType>());

		if(!Variable.IsValid())
		{
			Variable = MakeUnique<VariableType>(DefaultValue);
		}

		return static_cast<VariableType*>(Variable.Get())->GetValue();
	}

public:
	TCoroLocal() = default;
	TCoroLocal(ValueType&& InDefaultValue) : DefaultValue(MoveTemp(InDefaultValue))
	{
	}

	inline operator bool() const
	{
		return CoroTask_Detail::FCoroLocalState::IsCoroLaunchedTask();
	}

	inline ValueType* operator->() const
	{
		return &GetValue();
	}

	inline ValueType& operator*()
	{
		return GetValue();
	}

	inline TCoroLocal& operator= (const ValueType& Other)
	{
		GetValue() = Other;
		return *this;
	}
};