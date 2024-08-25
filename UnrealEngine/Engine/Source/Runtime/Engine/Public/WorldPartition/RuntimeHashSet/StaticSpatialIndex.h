// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Algo/Transform.h"
#include "Misc/TVariant.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "OverrideVoidReturnInvoker.h"

struct IStaticSpatialIndexDataInterface
{
	virtual ~IStaticSpatialIndexDataInterface() {}
	virtual const FBox& GetBox(uint32 InIndex) const =0;
};

template <typename ValueType, class SpatialIndexType>
class TStaticSpatialIndex : public IStaticSpatialIndexDataInterface
{
public:
	TStaticSpatialIndex()
		: SpatialIndex(*this)
	{}

	// Creation
	void Init(const TArray<TPair<FBox, ValueType>>& InElements)
	{
		Elements = InElements;

		InitSpatialIndex();
	}

	template <class Func>
	void ForEachElement(Func InFunc) const
	{
		TOverrideVoidReturnInvoker Invoker(true, InFunc);

		SpatialIndex.ForEachElement([this, &Invoker](uint32 ValueIndex)
		{
			return Invoker(Elements[ValueIndex].Value);
		});
	}

	template <class Func>
	void ForEachIntersectingElement(const FBox& InBox, Func InFunc) const
	{
		TOverrideVoidReturnInvoker Invoker(true, InFunc);

		SpatialIndex.ForEachIntersectingElement(InBox, [this, &Invoker](uint32 ValueIndex)
		{
			return Invoker(Elements[ValueIndex].Value);
		});
	}

	template <class Func>
	void ForEachIntersectingElement(const FSphere& InSphere, Func InFunc) const
	{
		TOverrideVoidReturnInvoker Invoker(true, InFunc);

		SpatialIndex.ForEachIntersectingElement(InSphere, [this, &Invoker](uint32 ValueIndex)
		{
			return Invoker(Elements[ValueIndex].Value);
		});
	}

	void AddReferencedObjects(FReferenceCollector& Collector)
	{
		if constexpr (TIsPointerOrObjectPtrToBaseOf<ValueType, UObject>::Value)
		{
			for (TPair<FBox, ValueType>& Element : Elements)
			{
				Collector.AddReferencedObject(Element.Value);
			}
		}
	}

	// IStaticSpatialIndexDataInterface interface
	virtual const FBox& GetBox(uint32 InIndex) const override
	{
		return Elements[InIndex].Key;
	}

private:
	void InitSpatialIndex()
	{		
		TArray<TPair<FBox, uint32>> IndexData;
		
		int32 ElemIndex = 0;
		IndexData.Reserve(Elements.Num());
		Algo::Transform(Elements, IndexData, [&ElemIndex](const TPair<FBox, ValueType>& Element) { return TPair<FBox, uint32>(Element.Key, ElemIndex++); });

		SpatialIndex.Init(IndexData);
	}

	TArray<TPair<FBox, ValueType>> Elements;
	SpatialIndexType SpatialIndex;
};

namespace FStaticSpatialIndex
{
	class FImpl
	{
	public:
		FImpl(const IStaticSpatialIndexDataInterface& InDataInterface)
			: DataInterface(InDataInterface)
		{}

	protected:
		const IStaticSpatialIndexDataInterface& DataInterface;
	};

	class FListImpl : public FImpl
	{
	public:
		FListImpl(const IStaticSpatialIndexDataInterface& InDataInterface)
			: FImpl(InDataInterface)
		{}

		void Init(const TArray<TPair<FBox, uint32>>& InElements);

		bool ForEachElement(TFunctionRef<bool(uint32 InValueIndex)> Func) const;
		bool ForEachIntersectingElement(const FBox& InBox, TFunctionRef<bool(uint32 InValueIndex)> Func) const;
		bool ForEachIntersectingElement(const FSphere& InSphere, TFunctionRef<bool(uint32 InValueIndex)> Func) const;

	private:
		TArray<uint32> Elements;
	};

	class FRTreeImpl : public FImpl
	{
	public:
		FRTreeImpl(const IStaticSpatialIndexDataInterface& InDataInterface)
			: FImpl(InDataInterface)
		{}

		void Init(const TArray<TPair<FBox, uint32>>& InElements);

		bool ForEachElement(TFunctionRef<bool(uint32 InValueIndex)> Func) const;
		bool ForEachIntersectingElement(const FBox& InBox, TFunctionRef<bool(uint32 InValueIndex)> Func) const;
		bool ForEachIntersectingElement(const FSphere& InSphere, TFunctionRef<bool(uint32 InValueIndex)> Func) const;

	private:
		struct FNode
		{
			using FNodeType = TArray<FNode>;
			using FLeafType = TArray<uint32>;

			FBox Box = FBox(ForceInit);
			TVariant<FNodeType, FLeafType> Content;
		};

		bool ForEachElementRecursive(const FNode* Node, TFunctionRef<bool(uint32 InValueIndex)> Func) const;
		bool ForEachIntersectingElementRecursive(const FNode* Node, const FBox& InBox, TFunctionRef<bool(uint32 InValueIndex)> Func) const;
		bool ForEachIntersectingElementRecursive(const FNode* Node, const FSphere& InSphere, TFunctionRef<bool(uint32 InValueIndex)> Func) const;

		FNode RootNode;
	};
}

template <class ValueType> class TStaticSpatialIndexList : public TStaticSpatialIndex<ValueType, FStaticSpatialIndex::FListImpl> {};
template <class ValueType> class TStaticSpatialIndexRTree : public TStaticSpatialIndex<ValueType, FStaticSpatialIndex::FRTreeImpl> {};