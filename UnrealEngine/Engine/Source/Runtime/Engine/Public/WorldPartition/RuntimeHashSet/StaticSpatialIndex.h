// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Algo/Transform.h"
#include "Misc/TVariant.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

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

	void ForEachElement(TFunctionRef<void(const ValueType& InValue)> Func) const
	{
		SpatialIndex.ForEachElement([this, &Func](uint32 ValueIndex)
		{
			Func(Elements[ValueIndex].Value);
		});
	}

	void ForEachIntersectingElement(const FBox& InBox, TFunctionRef<void(const ValueType& InValue)> Func) const
	{
		SpatialIndex.ForEachIntersectingElement(InBox, [this, &Func](uint32 ValueIndex)
		{
			Func(Elements[ValueIndex].Value);
		});
	}

	void ForEachIntersectingElement(const FSphere& InSphere, TFunctionRef<void(const ValueType& InValue)> Func) const
	{
		SpatialIndex.ForEachIntersectingElement(InSphere, [this, &Func](uint32 ValueIndex)
		{
			Func(Elements[ValueIndex].Value);
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

	void Serialize(FArchive& Ar)
	{
		Ar << Elements;

		if (Ar.IsLoading())
		{
			InitSpatialIndex();
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

		void ForEachElement(TFunctionRef<void(uint32 InValueIndex)> Func) const;
		void ForEachIntersectingElement(const FBox& InBox, TFunctionRef<void(uint32 InValueIndex)> Func) const;
		void ForEachIntersectingElement(const FSphere& InSphere, TFunctionRef<void(uint32 InValueIndex)> Func) const;

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

		void ForEachElement(TFunctionRef<void(uint32 InValueIndex)> Func) const;
		void ForEachIntersectingElement(const FBox& InBox, TFunctionRef<void(uint32 InValueIndex)> Func) const;
		void ForEachIntersectingElement(const FSphere& InSphere, TFunctionRef<void(uint32 InValueIndex)> Func) const;

	private:
		struct FNode
		{
			using FNodeType = TArray<FNode>;
			using FLeafType = TArray<uint32>;

			FBox Box = FBox(ForceInit);
			TVariant<FNodeType, FLeafType> Content;
		};

		void ForEachElementRecursive(const FNode* Node, TFunctionRef<void(uint32 InValueIndex)> Func) const;
		void ForEachIntersectingElementRecursive(const FNode* Node, const FBox& InBox, TFunctionRef<void(uint32 InValueIndex)> Func) const;
		void ForEachIntersectingElementRecursive(const FNode* Node, const FSphere& InSphere, TFunctionRef<void(uint32 InValueIndex)> Func) const;

		FNode RootNode;
	};
}

template <class ValueType> class TStaticSpatialIndexList : public TStaticSpatialIndex<ValueType, FStaticSpatialIndex::FListImpl> {};
template <class ValueType> class TStaticSpatialIndexRTree : public TStaticSpatialIndex<ValueType, FStaticSpatialIndex::FRTreeImpl> {};