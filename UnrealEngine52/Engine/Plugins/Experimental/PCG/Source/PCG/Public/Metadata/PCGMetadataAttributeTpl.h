// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "Metadata/PCGMetadataCommon.h"
#include "PCGModule.h"

#include "Helpers/PCGMetadataHelpers.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTraits.h"
#include "Misc/ScopeRWLock.h"

class UPCGMetadata;

template<typename T>
class FPCGMetadataAttribute : public FPCGMetadataAttributeBase
{
public:
	FPCGMetadataAttribute(UPCGMetadata* InMetadata, FName InName, const FPCGMetadataAttributeBase* InParent, const T& InDefaultValue, bool bInAllowsInterpolation)
		: FPCGMetadataAttributeBase(InMetadata, InName, InParent, bInAllowsInterpolation)
		, DefaultValue(InDefaultValue)
	{
		TypeId = PCG::Private::MetadataTypes<T>::Id;

		// Make sure we don't parent with the wrong type id
		check(!Parent || Parent->GetTypeId() == TypeId)

		if (GetParent())
		{
			ValueKeyOffset = GetParent()->GetValueKeyOffsetForChild();
		}
	}

	// This constructor is used only during serialization
	FPCGMetadataAttribute()
	{
		TypeId = PCG::Private::MetadataTypes<T>::Id;
	}

	virtual void Serialize(UPCGMetadata* InMetadata, FArchive& InArchive) override
	{
		FPCGMetadataAttributeBase::Serialize(InMetadata, InArchive);

		InArchive << Values;
		InArchive << DefaultValue;
		
		// Initialize non-serialized members
		if (InArchive.IsLoading())
		{
			ValueKeyOffset = GetParent() ? GetParent()->GetValueKeyOffsetForChild() : 0;
		}
	}

	virtual void Flatten() override
	{
		// Implementation notes:
		// We don't need to flatten the EntryToValueKeyMap - this will have been taken care of in the metadata flatten

		// Flatten values, from root to current attribute
		if(Parent)
		{
			FWriteScopeLock ScopeLock(ValueLock);

			TArray<const TArray<T>*> OriginalValues;
			int32 ValueCount = 0;

			const FPCGMetadataAttribute<T>* Current = static_cast<const FPCGMetadataAttribute<T>*>(this);
			while (Current)
			{
				ValueCount += Current->Values.Num();
				OriginalValues.Add(&Current->Values);
				Current = static_cast<const FPCGMetadataAttribute<T>*>(Current->Parent);
			}

			TArray<T> FlattenedValues;
			FlattenedValues.Reserve(ValueCount);

			for (int32 ValuesIndex = OriginalValues.Num() - 1; ValuesIndex >= 0; --ValuesIndex)
			{
				FlattenedValues.Append(*OriginalValues[ValuesIndex]);
			}

			Values = MoveTemp(FlattenedValues);
		}
		
		// Reset value offset, and lose parent
		ValueKeyOffset = 0;
		Parent = nullptr;
	}

	const FPCGMetadataAttribute* GetParent() const { return static_cast<const FPCGMetadataAttribute*>(Parent); }

	FPCGMetadataAttribute* TypedCopy(FName NewName, UPCGMetadata* InMetadata, bool bKeepParent, bool bCopyEntries = true, bool bCopyValues = true)
	{
		return static_cast<FPCGMetadataAttribute*>(Copy(NewName, InMetadata, bKeepParent, bCopyEntries, bCopyValues));
	}

	virtual FPCGMetadataAttributeBase* Copy(FName NewName, UPCGMetadata* InMetadata, bool bKeepParent, bool bCopyEntries = true, bool bCopyValues = true) const override
	{
		// If we copy an attribute where we don't want to keep the parent, while copying entries and/or values, we'll lose data.
		// In that case, we will copy all the data from this attribute and all its ancestors.

		// We can't keep the parent if we don't have the same root.
		check(!bKeepParent || PCGMetadataHelpers::HasSameRoot(Metadata, InMetadata));

		// Validate that the new name is valid
		if (!IsValidName(NewName))
		{
			UE_LOG(LogPCG, Error, TEXT("Try to create a new attribute with an invalid name: %s"), *NewName.ToString());
			return nullptr;
		}
		
		// This copies to a new attribute.
		FPCGMetadataAttribute<T>* AttributeCopy = new FPCGMetadataAttribute<T>(InMetadata, NewName, bKeepParent ? this : nullptr, DefaultValue, bAllowsInterpolation);

		// Gather the chain of parents if we don't keep the parent and we want to copy entries/values.
		// We always have at least one item, "this".
		TArray<const FPCGMetadataAttribute<T>*, TInlineAllocator<2>> Parents = { this };
		if (!bKeepParent && (bCopyEntries || bCopyValues))
		{
			const UPCGMetadata* CurrentMetadata = Metadata.Get();
			const FPCGMetadataAttribute<T>* Current = this;

			const UPCGMetadata* ParentMetadata = PCGMetadataHelpers::GetParentMetadata(CurrentMetadata);
			while(ParentMetadata && Current->Parent)
			{
				CurrentMetadata = ParentMetadata;
				Current = static_cast<const FPCGMetadataAttribute<T>*>(Current->Parent);
				Parents.Add(Current);

				ParentMetadata = PCGMetadataHelpers::GetParentMetadata(CurrentMetadata);
			}
		}

		if (bCopyEntries)
		{
			// We go backwards, since we need to preserve order (root -> this)
			// Latest entry in our Parents array is the root.
			for (int32 i = Parents.Num() - 1; i >= 0; --i)
			{
				const FPCGMetadataAttribute<T>* Current = Parents[i];
				
				Current->EntryMapLock.ReadLock();
				AttributeCopy->EntryToValueKeyMap.Append(Current->EntryToValueKeyMap);
				Current->EntryMapLock.ReadUnlock();
			}
		}

		if (bCopyValues)
		{
			// We go backwards, since we need to preserve order (root -> this)
			// Latest entry in our Parents array is the root.
			for (int32 i = Parents.Num() - 1; i >= 0; --i)
			{
				const FPCGMetadataAttribute<T>* Current = Parents[i];
				
				Current->ValueLock.ReadLock();
				AttributeCopy->Values.Append(Current->Values);
				// The expected value key offset is the one for this attribute (i == 0), and only if we
				// keep the parent. Otherwise we don't have any parent, so offset should be kept at 0.
				if (i == 0 && bKeepParent)
				{
					AttributeCopy->ValueKeyOffset = Current->ValueKeyOffset;
				}
				Current->ValueLock.ReadUnlock();
			}
			
			
		}

		return AttributeCopy;
	}

	virtual void SetValue(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttribute, PCGMetadataEntryKey InEntryKey) override
	{
		check(ItemKey != PCGInvalidEntryKey);
		if (InAttribute == this)
		{
			SetValueFromValueKey(ItemKey, GetValueKey(InEntryKey));
		}
		else if (InAttribute)
		{
			SetValue(ItemKey, static_cast<const FPCGMetadataAttribute<T>*>(InAttribute)->GetValueFromItemKey(InEntryKey));
		}
	}

	virtual void SetZeroValue(PCGMetadataEntryKey ItemKey) override
	{
		check(ItemKey != PCGInvalidEntryKey);
		ZeroValue(ItemKey);
	}

	virtual void AccumulateValue(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttribute, PCGMetadataEntryKey InEntryKey, float Weight) override
	{
		check(ItemKey != PCGInvalidEntryKey);
		Accumulate(ItemKey, InAttribute, InEntryKey, Weight);
	}

	virtual void SetWeightedValue(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttribute, const TArrayView<TPair<PCGMetadataEntryKey, float>> InWeightedKeys) override
	{
		check(ItemKey != PCGInvalidEntryKey);
		Accumulate(ItemKey, InAttribute, InWeightedKeys);
	}

	virtual void SetValue(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB, EPCGMetadataOp Op) override
	{
		check(ItemKey != PCGInvalidEntryKey);
		bool bAppliedValue = false;

		if (Op == EPCGMetadataOp::TargetValue && InAttributeB)
		{
			// Take value of second attribute.
			if (InAttributeB == this)
			{
				SetValueFromValueKey(ItemKey, GetValueKey(InEntryKeyB));
			}
			else
			{
				SetValue(ItemKey, static_cast<const FPCGMetadataAttribute<T>*>(InAttributeB)->GetValueFromItemKey(InEntryKeyB));
			}

			bAppliedValue = true;
		}
		else if (Op == EPCGMetadataOp::SourceValue && InAttributeA)
		{
			// Take value of first attribute.
			if (InAttributeA == this)
			{
				SetValueFromValueKey(ItemKey, GetValueKey(InEntryKeyA));
			}
			else
			{
				SetValue(ItemKey, static_cast<const FPCGMetadataAttribute<T>*>(InAttributeA)->GetValueFromItemKey(InEntryKeyA));
			}

			bAppliedValue = true;
		}
		else if (InAttributeA && InAttributeB && bAllowsInterpolation)
		{
			// Combine attributes using specified operation.
			if (Op == EPCGMetadataOp::Min)
			{
				bAppliedValue = SetMin(ItemKey, InAttributeA, InEntryKeyA, InAttributeB, InEntryKeyB);
			}
			else if (Op == EPCGMetadataOp::Max)
			{
				bAppliedValue = SetMax(ItemKey, InAttributeA, InEntryKeyA, InAttributeB, InEntryKeyB);
			}
			else if (Op == EPCGMetadataOp::Sub)
			{
				bAppliedValue = SetSub(ItemKey, InAttributeA, InEntryKeyA, InAttributeB, InEntryKeyB);
			}
			else if (Op == EPCGMetadataOp::Add)
			{
				bAppliedValue = SetAdd(ItemKey, InAttributeA, InEntryKeyA, InAttributeB, InEntryKeyB);
			}
			else if (Op == EPCGMetadataOp::Mul)
			{
				bAppliedValue = SetMul(ItemKey, InAttributeA, InEntryKeyA, InAttributeB, InEntryKeyB);
			}
			else if (Op == EPCGMetadataOp::Div)
			{
				bAppliedValue = SetDiv(ItemKey, InAttributeA, InEntryKeyA, InAttributeB, InEntryKeyB);
			}
		}
		else if (InAttributeA && InAttributeB && HasNonDefaultValue(ItemKey))
		{
			// In this case, the current already has a value, in which case we should not update it
			bAppliedValue = true;
		}

		if (bAppliedValue)
		{
			// Nothing to do
		}
		else if (InAttributeA)
		{
			if (InAttributeA == this)
			{
				SetValueFromValueKey(ItemKey, GetValueKey(InEntryKeyA));
			}
			else
			{
				SetValue(ItemKey, static_cast<const FPCGMetadataAttribute<T>*>(InAttributeA)->GetValueFromItemKey(InEntryKeyA));
			}
		}
		else if (InAttributeB)
		{
			if (InAttributeB == this)
			{
				SetValueFromValueKey(ItemKey, GetValueKey(InEntryKeyB));
			}
			else
			{
				SetValue(ItemKey, static_cast<const FPCGMetadataAttribute<T>*>(InAttributeB)->GetValueFromItemKey(InEntryKeyB));
			}
		}
	}

	virtual bool AreValuesEqualForEntryKeys(PCGMetadataEntryKey EntryKey1, PCGMetadataEntryKey EntryKey2) const override
	{
		return AreValuesEqual(GetValueKey(EntryKey1), GetValueKey(EntryKey2));
	}

	virtual bool AreValuesEqual(PCGMetadataValueKey ValueKey1, PCGMetadataValueKey ValueKey2) const override
	{
		if constexpr (PCG::Private::MetadataTraits<T>::CompressData)
		{
			if (ValueKey1 == PCGInvalidEntryKey)
			{
				return PCG::Private::MetadataTraits<T>::Equal(DefaultValue, GetValue(ValueKey2));
			}
			else if (ValueKey2 == PCGInvalidEntryKey)
			{
				return PCG::Private::MetadataTraits<T>::Equal(GetValue(ValueKey1), DefaultValue);
			}
			else
			{
				return ValueKey1 == ValueKey2;
			}
		}
		else
		{
			return ValueKey1 == ValueKey2 || PCG::Private::MetadataTraits<T>::Equal(GetValue(ValueKey1), GetValue(ValueKey2));
		}
	}

	virtual bool IsEqualToDefaultValue(PCGMetadataValueKey ValueKey) const override
	{
		return PCG::Private::MetadataTraits<T>::Equal(GetValue(ValueKey), DefaultValue);
	}

	PCGMetadataValueKey GetValueKeyOffsetForChild() const
	{
		FReadScopeLock ScopeLock(ValueLock);
		return Values.Num() + ValueKeyOffset;
	}

	/** Adds the value, returns the value key for the given value */
	PCGMetadataValueKey AddValue(const T& InValue)
	{
		PCGMetadataValueKey FoundValue = FindValue(InValue);

		if (FoundValue == PCGDefaultValueKey)
		{
			FWriteScopeLock ScopeLock(ValueLock);
			return Values.Add(InValue) + ValueKeyOffset;
		}
		else
		{
			return FoundValue;
		}
	}

	void SetValue(PCGMetadataEntryKey ItemKey, const T& InValue)
	{
		check(ItemKey != PCGInvalidEntryKey);
		SetValueFromValueKey(ItemKey, AddValue(InValue));
	}

	template<typename U>
	void SetValue(PCGMetadataEntryKey ItemKey, const U& InValue)
	{
		check(ItemKey != PCGInvalidEntryKey);
		SetValueFromValueKey(ItemKey, AddValue(T(InValue)));
	}

	T GetValueFromItemKey(PCGMetadataEntryKey ItemKey) const
	{
		return GetValue(GetValueKey(ItemKey));
	}

	T GetValue(PCGMetadataValueKey ValueKey) const
	{
		if (ValueKey == PCGDefaultValueKey)
		{
			return DefaultValue;
		}
		else if (ValueKey >= ValueKeyOffset)
		{
			int32 Index = ValueKey - ValueKeyOffset;
			FReadScopeLock ScopeLock(ValueLock);
			return Index < Values.Num() ? Values[Index] : DefaultValue;
		}
		else if (GetParent())
		{
			return GetParent()->GetValue(ValueKey);
		}
		else
		{
			return DefaultValue;
		}
	}

	/** Code related to finding values / compressing data */
	virtual bool UsesValueKeys() const override
	{
		return PCG::Private::MetadataTraits<T>::CompressData;
	}

	template<typename IT = T, typename TEnableIf<PCG::Private::MetadataTraits<IT>::CompressData>::Type* = nullptr>
	PCGMetadataValueKey FindValue(const T& InValue) const
	{
		PCGMetadataValueKey ParentValueKey = (GetParent() ? GetParent()->FindValue(InValue) : PCGDefaultValueKey);
		if (ParentValueKey != PCGDefaultValueKey)
		{
			return ParentValueKey;
		}
		else
		{
			ValueLock.ReadLock();
			const int32 ValueIndex = Values.FindLast(InValue);
			ValueLock.ReadUnlock();

			if (ValueIndex != INDEX_NONE)
			{
				return ValueIndex + ValueKeyOffset;
			}
			else
			{
				return ParentValueKey;
			}
		}
	}

	template<typename IT = T, typename TEnableIf<!PCG::Private::MetadataTraits<IT>::CompressData>::Type* = nullptr>
	PCGMetadataValueKey FindValue(const T& InValue) const
	{
		return PCGDefaultValueKey;
	}

	void SetDefaultValue(const T& Value)
	{
		DefaultValue = Value;
	}

protected:
	/** Code related to computing compared values (min, max, sub, add) */
	template<typename IT = T, typename TEnableIf<PCG::Private::MetadataTraits<IT>::CanMinMax>::Type* = nullptr>
	bool SetMin(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB)
	{
		SetValue(ItemKey, 
			PCG::Private::MetadataTraits<IT>::Min(
				static_cast<const FPCGMetadataAttribute<T>*>(InAttributeA)->GetValueFromItemKey(InEntryKeyA),
				static_cast<const FPCGMetadataAttribute<T>*>(InAttributeB)->GetValueFromItemKey(InEntryKeyB)));

		return true;
	}

	template<typename IT = T, typename TEnableIf<!PCG::Private::MetadataTraits<IT>::CanMinMax>::Type* = nullptr>
	bool SetMin(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB)
	{
		return false;
	}

	template<typename IT = T, typename TEnableIf<PCG::Private::MetadataTraits<IT>::CanMinMax>::Type* = nullptr>
	bool SetMax(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB)
	{
		SetValue(ItemKey,
			PCG::Private::MetadataTraits<IT>::Max(
				static_cast<const FPCGMetadataAttribute<T>*>(InAttributeA)->GetValueFromItemKey(InEntryKeyA),
				static_cast<const FPCGMetadataAttribute<T>*>(InAttributeB)->GetValueFromItemKey(InEntryKeyB)));

		return true;
	}

	template<typename IT = T, typename TEnableIf<!PCG::Private::MetadataTraits<IT>::CanMinMax>::Type* = nullptr>
	bool SetMax(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB)
	{
		return false;
	}

	template<typename IT = T, typename TEnableIf<PCG::Private::MetadataTraits<IT>::CanSubAdd>::Type* = nullptr>
	bool SetAdd(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB)
	{
		SetValue(ItemKey,
			PCG::Private::MetadataTraits<IT>::Add(
				static_cast<const FPCGMetadataAttribute<T>*>(InAttributeA)->GetValueFromItemKey(InEntryKeyA),
				static_cast<const FPCGMetadataAttribute<T>*>(InAttributeB)->GetValueFromItemKey(InEntryKeyB)));

		return true;
	}

	template<typename IT = T, typename TEnableIf<!PCG::Private::MetadataTraits<IT>::CanSubAdd>::Type* = nullptr>
	bool SetAdd(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB)
	{
		return false;
	}

	template<typename IT = T, typename TEnableIf<PCG::Private::MetadataTraits<IT>::CanSubAdd>::Type* = nullptr>
	bool SetSub(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB)
	{
		SetValue(ItemKey,
			PCG::Private::MetadataTraits<IT>::Sub(
				static_cast<const FPCGMetadataAttribute<T>*>(InAttributeA)->GetValueFromItemKey(InEntryKeyA),
				static_cast<const FPCGMetadataAttribute<T>*>(InAttributeB)->GetValueFromItemKey(InEntryKeyB)));

		return true;
	}

	template<typename IT = T, typename TEnableIf<!PCG::Private::MetadataTraits<IT>::CanSubAdd>::Type* = nullptr>
	bool SetSub(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB)
	{
		return false;
	}

	template<typename IT = T, typename TEnableIf<PCG::Private::MetadataTraits<IT>::CanMulDiv>::Type* = nullptr>
	bool SetMul(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB)
	{
		SetValue(ItemKey,
			PCG::Private::MetadataTraits<IT>::Mul(
				static_cast<const FPCGMetadataAttribute<T>*>(InAttributeA)->GetValueFromItemKey(InEntryKeyA),
				static_cast<const FPCGMetadataAttribute<T>*>(InAttributeB)->GetValueFromItemKey(InEntryKeyB)));

		return true;
	}

	template<typename IT = T, typename TEnableIf<!PCG::Private::MetadataTraits<IT>::CanMulDiv>::Type* = nullptr>
	bool SetMul(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB)
	{
		return false;
	}

	template<typename IT = T, typename TEnableIf<PCG::Private::MetadataTraits<IT>::CanMulDiv>::Type* = nullptr>
	bool SetDiv(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB)
	{
		SetValue(ItemKey,
			PCG::Private::MetadataTraits<IT>::Div(
				static_cast<const FPCGMetadataAttribute<T>*>(InAttributeA)->GetValueFromItemKey(InEntryKeyA),
				static_cast<const FPCGMetadataAttribute<T>*>(InAttributeB)->GetValueFromItemKey(InEntryKeyB)));

		return true;
	}

	template<typename IT = T, typename TEnableIf<!PCG::Private::MetadataTraits<IT>::CanMulDiv>::Type* = nullptr>
	bool SetDiv(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB)
	{
		return false;
	}

	/** Weighted/interpolated values related code */
	template<typename IT = T, typename TEnableIf<PCG::Private::MetadataTraits<IT>::CanInterpolate>::Type* = nullptr>
	void ZeroValue(PCGMetadataEntryKey ItemKey)
	{
		SetValue(ItemKey, PCG::Private::MetadataTraits<IT>::ZeroValue());
	}

	template<typename IT = T, typename TEnableIf<!PCG::Private::MetadataTraits<IT>::CanInterpolate>::Type* = nullptr>
	void ZeroValue(PCGMetadataEntryKey ItemKey)
	{
		// Intentionally empty
	}

	template<typename IT = T, typename TEnableIf<PCG::Private::MetadataTraits<IT>::CanInterpolate>::Type* = nullptr>
	void Accumulate(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttribute, PCGMetadataEntryKey InEntryKey, float Weight)
	{
		SetValue(ItemKey,
			PCG::Private::MetadataTraits<IT>::WeightedSum(
				GetValueFromItemKey(ItemKey),
				static_cast<const FPCGMetadataAttribute<T>*>(InAttribute)->GetValueFromItemKey(InEntryKey),
				Weight));
	}

	template<typename IT = T, typename TEnableIf<!PCG::Private::MetadataTraits<IT>::CanInterpolate>::Type* = nullptr>
	void Accumulate(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttribute, PCGMetadataEntryKey InEntryKey, float Weight)
	{
		// Empty on purpose
	}

	template<typename IT = T, typename TEnableIf<PCG::Private::MetadataTraits<IT>::CanInterpolate>::Type* = nullptr>
	void Accumulate(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttribute, const TArrayView<TPair<PCGMetadataEntryKey, float>>& InWeightedKeys)
	{
		IT Value = PCG::Private::MetadataTraits<IT>::ZeroValue();
		for (const TPair<PCGMetadataEntryKey, float>& WeightedEntry : InWeightedKeys)
		{
			Value = PCG::Private::MetadataTraits<IT>::WeightedSum(
				Value,
				static_cast<const FPCGMetadataAttribute<T>*>(InAttribute)->GetValueFromItemKey(WeightedEntry.Key),
				WeightedEntry.Value);
		}

		SetValue(ItemKey, Value);
	}

	template<typename IT = T, typename TEnableIf<!PCG::Private::MetadataTraits<IT>::CanInterpolate>::Type* = nullptr>
	void Accumulate(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttribute, const TArrayView<TPair<PCGMetadataEntryKey, float>>& InWeightedKeys)
	{
		// Empty on purpose
	}

protected:
	mutable FRWLock ValueLock;
	TArray<T> Values;
	T DefaultValue = T{};
	PCGMetadataValueKey ValueKeyOffset = 0;
};

namespace PCGMetadataAttribute
{
	inline FPCGMetadataAttributeBase* AllocateEmptyAttributeFromType(int16 TypeId)
	{

		switch (TypeId)
		{

#define PCG_ALLOCATEEMPTY_DECL(T) case PCG::Private::MetadataTypes<T>::Id: return new FPCGMetadataAttribute<T>();
		PCG_FOREACH_SUPPORTEDTYPES(PCG_ALLOCATEEMPTY_DECL)
#undef PCG_ALLOCATEEMPTY_DECL

		default:
			return nullptr;
		}

#undef AllocatePCGMetadataAttributeOnType
	}

	template <typename Func, typename... Args>
	inline decltype(auto) CallbackWithRightType(uint16 TypeId, Func Callback, Args&& ...InArgs)
	{
		using ReturnType = decltype(Callback(double{}, std::forward<Args>(InArgs)...));

		switch (TypeId)
		{

#define PCG_CALLBACKWITHRIGHTTYPE_DECL(T) case (uint16)(PCG::Private::MetadataTypes<T>::Id): return Callback(T{}, std::forward<Args>(InArgs)...);
		PCG_FOREACH_SUPPORTEDTYPES(PCG_CALLBACKWITHRIGHTTYPE_DECL)
#undef PCG_CALLBACKWITHRIGHTTYPE_DECL

		default:
		{
			// ReturnType{} is invalid if ReturnType is void
			if constexpr (std::is_same_v<ReturnType, void>)
			{
				return;
			}
			else
			{
				return ReturnType{};
			}
		}
		}
	}
}

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
