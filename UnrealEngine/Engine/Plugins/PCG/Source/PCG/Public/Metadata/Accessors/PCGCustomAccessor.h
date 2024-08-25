// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPCGAttributeAccessorTpl.h"

#include "PCGPoint.h"

/**
* Templated accessor class for custom point properties. Need a getter and a setter, defined in the
* FPCGPoint class.
* Key supported: Points
*/
template <typename T>
class FPCGCustomPointAccessor : public IPCGAttributeAccessorT<FPCGCustomPointAccessor<T>>
{
public:
	using Type = T;
	using Super = IPCGAttributeAccessorT<FPCGCustomPointAccessor<T>>;

	FPCGCustomPointAccessor(const FPCGPoint::PointCustomPropertyGetter& InGetter, const FPCGPoint::PointCustomPropertySetter& InSetter)
		: Super(/*bInReadOnly=*/ false)
		, Getter(InGetter)
		, Setter(InSetter)
	{}

	bool GetRangeImpl(TArrayView<T> OutValues, int32 Index, const IPCGAttributeAccessorKeys& Keys) const
	{
		TArray<const FPCGPoint*> PointKeys;
		PointKeys.SetNum(OutValues.Num());
		TArrayView<const FPCGPoint*> PointKeysView(PointKeys);
		if (!Keys.GetKeys(Index, PointKeysView))
		{
			return false;
		}

		for (int32 i = 0; i < OutValues.Num(); ++i)
		{
			Getter(*PointKeys[i], &OutValues[i]);
		}

		return true;
	}

	bool SetRangeImpl(TArrayView<const T> InValues, int32 Index, IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags Flags)
	{
		TArray<FPCGPoint*> PointKeys;
		PointKeys.SetNum(InValues.Num());
		TArrayView<FPCGPoint*> PointKeysView(PointKeys);
		if (!Keys.GetKeys(Index, PointKeysView))
		{
			return false;
		}

		for (int32 i = 0; i < InValues.Num(); ++i)
		{
			Setter(*PointKeys[i], &InValues[i]);
		}

		return true;
	}

private:
	FPCGPoint::PointCustomPropertyGetter Getter;
	FPCGPoint::PointCustomPropertySetter Setter;
};

/**
* Very simple accessor that returns a constant value. Read only
* Key supported: All
*/
template <typename T>
class FPCGConstantValueAccessor : public IPCGAttributeAccessorT<FPCGConstantValueAccessor<T>>
{
public:
	using Type = T;
	using Super = IPCGAttributeAccessorT<FPCGConstantValueAccessor<T>>;

	FPCGConstantValueAccessor(const T& InValue)
		: Super(/*bInReadOnly=*/ true)
		, Value(InValue)
	{}

	bool GetRangeImpl(TArrayView<T> OutValues, int32, const IPCGAttributeAccessorKeys&) const
	{
		for (int32 i = 0; i < OutValues.Num(); ++i)
		{
			OutValues[i] = Value;
		}

		return true;
	}

	bool SetRangeImpl(TArrayView<const T>, int32, IPCGAttributeAccessorKeys&, EPCGAttributeAccessorFlags)
	{
		return false;
	}

private:
	T Value;
};

/**
* To chain accessors. T is the type of this accessor. U is the type of the underlying accessor.
* Key supported: Same as the underlying accessor
*/
template <typename T, typename U>
class FPCGChainAccessor : public IPCGAttributeAccessorT<FPCGChainAccessor<T,U>>
{
public:
	using ChainGetter = TFunction<T(const U&)>;
	using ChainSetter = TFunction<void(U&, const T&)>;

	using Type = T;
	using OtherType = U;
	using Super = IPCGAttributeAccessorT<FPCGChainAccessor<T, U>>;

	FPCGChainAccessor(TUniquePtr<IPCGAttributeAccessor> InAccessor, const ChainGetter& InGetter)
		: Super(/*bInReadOnly=*/ true)
		, Accessor(std::move(InAccessor))
		, Getter(InGetter)
		, Setter()
	{
		check(Accessor);
	}

	FPCGChainAccessor(TUniquePtr<IPCGAttributeAccessor> InAccessor, const ChainGetter& InGetter, const ChainSetter& InSetter)
		: Super(/*bInReadOnly=*/ !InAccessor || InAccessor->IsReadOnly())
		, Accessor(std::move(InAccessor))
		, Getter(InGetter)
		, Setter(InSetter)
	{
		check(Accessor);
	}

	bool GetRangeImpl(TArrayView<T> OutValues, int32 Index, const IPCGAttributeAccessorKeys& Keys) const
	{
		TArray<U> TempValues;
		TempValues.SetNumUninitialized(OutValues.Num());
		if (!Accessor->GetRange<U>(TempValues, Index, Keys))
		{
			return false;
		}

		for (int32 i = 0; i < OutValues.Num(); ++i)
		{
			OutValues[i] = Getter(TempValues[i]);
		}

		return true;
	}

	bool SetRangeImpl(TArrayView<const T> InValues, int32 Index, IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags Flags)
	{
		TArray<U> TempValues;
		TempValues.SetNumUninitialized(InValues.Num());
		if (!Accessor->GetRange<U>(TempValues, Index, Keys))
		{
			return false;
		}

		for (int32 i = 0; i < InValues.Num(); ++i)
		{
			Setter(TempValues[i], InValues[i]);
		}

		return Accessor->SetRange<U>(TempValues, Index, Keys, Flags);
	}

private:
	TUniquePtr<IPCGAttributeAccessor> Accessor;
	ChainGetter Getter;
	ChainSetter Setter;
};

/**
* Very simple accessor that returns the index. Read only
* Key supported: All
*/
class FPCGIndexAccessor : public IPCGAttributeAccessorT<FPCGIndexAccessor>
{
public:
	using Type = int32;
	using Super = IPCGAttributeAccessorT<FPCGIndexAccessor>;

	FPCGIndexAccessor()
		: Super(/*bInReadOnly=*/ true)
	{}

	bool GetRangeImpl(TArrayView<int32> OutValues, int32 Index, const IPCGAttributeAccessorKeys& InKeys) const
	{
		const int32 NumKeys = InKeys.GetNum();
		int32 Counter = Index;

		for (int32 i = 0; i < OutValues.Num(); ++i)
		{
			OutValues[i] = Counter++;
			if (Counter >= NumKeys)
			{
				Counter = 0;
			}
		}

		return true;
	}

	bool SetRangeImpl(TArrayView<const int32>, int32, IPCGAttributeAccessorKeys&, EPCGAttributeAccessorFlags)
	{
		return false;
	}
};