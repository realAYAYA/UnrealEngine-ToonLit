// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/CollectionPropertyFacade.h"
#include "GeometryCollection/ManagedArrayCollection.h"

namespace Chaos::Softs
{
	namespace PropertyFacadeNames
	{
		// Attribute groups, predefined data member of the collection
		static const FName PropertyGroup("Property");
		static const FName KeyName("Key");  // Property key, name to look for
		static const FName LowValueName("LowValue");  // Boolean, 24 bit integer (max 16777215), float, or vector value, or value of the lowest weight on the weight map if any
		static const FName HighValueName("HighValue");  // Boolean, 24 bit integer (max 16777215), float, or vector value of the highest weight on the weight map if any
		static const FName StringValueName("StringValue");  // String value, or weight map name, ...etc.
		static const FName FlagsName("Flags");  // Whether this property is enabled, animatable, ...etc.
	}

	FCollectionPropertyConstFacade::FCollectionPropertyConstFacade(const TSharedPtr<const FManagedArrayCollection>& InManagedArrayCollection)
		: ManagedArrayCollection(InManagedArrayCollection)
	{
		UpdateArrays();
		RebuildKeyIndices();
	}

	FCollectionPropertyConstFacade::FCollectionPropertyConstFacade(const TSharedPtr<const FManagedArrayCollection>& InManagedArrayCollection, ENoInit)
		: ManagedArrayCollection(InManagedArrayCollection)
	{
	}

	bool FCollectionPropertyConstFacade::IsValid() const
	{
		return
			ManagedArrayCollection->HasAttribute(PropertyFacadeNames::KeyName, PropertyFacadeNames::PropertyGroup) &&
			ManagedArrayCollection->HasAttribute(PropertyFacadeNames::LowValueName, PropertyFacadeNames::PropertyGroup) &&
			ManagedArrayCollection->HasAttribute(PropertyFacadeNames::HighValueName, PropertyFacadeNames::PropertyGroup) &&
			ManagedArrayCollection->HasAttribute(PropertyFacadeNames::StringValueName, PropertyFacadeNames::PropertyGroup) &&
			ManagedArrayCollection->HasAttribute(PropertyFacadeNames::FlagsName, PropertyFacadeNames::PropertyGroup);
	}

	void FCollectionPropertyConstFacade::UpdateArrays()
	{
		KeyArray = GetArray<FString>(PropertyFacadeNames::KeyName);
		LowValueArray = GetArray<FVector3f>(PropertyFacadeNames::LowValueName);
		HighValueArray = GetArray<FVector3f>(PropertyFacadeNames::HighValueName);
		StringValueArray = GetArray<FString>(PropertyFacadeNames::StringValueName);
		FlagsArray = GetArray<uint8>(PropertyFacadeNames::FlagsName);
	}
	
	void FCollectionPropertyConstFacade::RebuildKeyIndices()
	{
		// Create a fast access search map (although it might only be faster for a large enough number of properties)
		const int32 NumKeys = KeyArray.Num();
		KeyIndices.Empty(NumKeys);
		for (int32 Index = 0; Index < NumKeys; ++Index)
		{
			KeyIndices.Emplace(KeyArray[Index], Index);
		}
	}

	template<typename T, typename ElementType>
	T FCollectionPropertyConstFacade::GetValue(int32 KeyIndex, const TConstArrayView<ElementType>& ValueArray) const
	{
		return (T)ValueArray[KeyIndex];
	}
	template CHAOS_API FVector3f FCollectionPropertyConstFacade::GetValue<FVector3f, FVector3f>(int32 KeyIndex, const TConstArrayView<FVector3f>& ValueArray) const;
	template CHAOS_API const FVector3f& FCollectionPropertyConstFacade::GetValue<const FVector3f&, FVector3f>(int32 KeyIndex, const TConstArrayView<FVector3f>& ValueArray) const;
	template CHAOS_API FString FCollectionPropertyConstFacade::GetValue<FString, FString>(int32 KeyIndex, const TConstArrayView<FString>& ValueArray) const;
	template CHAOS_API const FString& FCollectionPropertyConstFacade::GetValue<const FString&, FString>(int32 KeyIndex, const TConstArrayView<FString>& ValueArray) const;
	template CHAOS_API uint8 FCollectionPropertyConstFacade::GetValue<uint8, uint8>(int32 KeyIndex, const TConstArrayView<uint8>& ValueArray) const;
	template CHAOS_API ECollectionPropertyFlags FCollectionPropertyConstFacade::GetValue<ECollectionPropertyFlags, uint8>(int32 KeyIndex, const TConstArrayView<uint8>& ValueArray) const;

	template<> CHAOS_API
	bool FCollectionPropertyConstFacade::GetValue<bool, FVector3f>(int32 KeyIndex, const TConstArrayView<FVector3f>& ValueArray) const
	{
		return (bool)ValueArray[KeyIndex].X;
	}

	template<> CHAOS_API
	int32 FCollectionPropertyConstFacade::GetValue<int32, FVector3f>(int32 KeyIndex, const TConstArrayView<FVector3f>& ValueArray) const
	{
		return (int32)ValueArray[KeyIndex].X;
	}

	template<> CHAOS_API
	float FCollectionPropertyConstFacade::GetValue<float, FVector3f>(int32 KeyIndex, const TConstArrayView<FVector3f>& ValueArray) const
	{
		return ValueArray[KeyIndex].X;
	}

	template <typename T>
	TConstArrayView<T> FCollectionPropertyConstFacade::GetArray(const FName& Name) const
	{
		const TManagedArray<T>* ManagedArray = ManagedArrayCollection->FindAttributeTyped<T>(Name, PropertyFacadeNames::PropertyGroup);
		return ManagedArray ? TConstArrayView<T>(ManagedArray->GetConstArray()) : TConstArrayView<T>();
	}

	FCollectionPropertyFacade::FCollectionPropertyFacade(const TSharedPtr<FManagedArrayCollection>& InManagedArrayCollection)
		: FCollectionPropertyConstFacade(InManagedArrayCollection, NoInit)
	{
		UpdateArrays();
		RebuildKeyIndices();
	}

	FCollectionPropertyFacade::FCollectionPropertyFacade(const TSharedPtr<FManagedArrayCollection>& InManagedArrayCollection, ENoInit)
		: FCollectionPropertyConstFacade(InManagedArrayCollection, NoInit)
	{
	}

	void FCollectionPropertyFacade::SetFlags(int32 KeyIndex, ECollectionPropertyFlags Flags)
	{
		// Cannot set string dirty without also dirtying the property
		Flags |= EnumHasAnyFlags(Flags, ECollectionPropertyFlags::StringDirty) ? ECollectionPropertyFlags::Dirty : ECollectionPropertyFlags::None;
		// Cannot remove the Dirty, StringDirty, and Intrinsic flags
		Flags |= GetFlagsArray()[KeyIndex] & (ECollectionPropertyFlags::Dirty | ECollectionPropertyFlags::StringDirty | ECollectionPropertyFlags::Intrinsic | ECollectionPropertyFlags::Interpolable);

		SetValue(KeyIndex, GetFlagsArray(), Flags);
	}

	void FCollectionPropertyFacade::ClearDirtyFlags()
	{
		for (ECollectionPropertyFlags& Flags : GetFlagsArray())
		{
			EnumRemoveFlags(Flags, ECollectionPropertyFlags::StringDirty | ECollectionPropertyFlags::Dirty);
		}
	}

	void FCollectionPropertyFacade::EnableFlags(int32 KeyIndex, ECollectionPropertyFlags Flags, bool bEnable)
	{
		const ECollectionPropertyFlags CurrentFlags = GetFlagsArray()[KeyIndex];

		if (bEnable)
		{
			if (!EnumHasAllFlags(CurrentFlags, Flags))
			{
				EnumAddFlags(GetFlagsArray()[KeyIndex], Flags | ECollectionPropertyFlags::Dirty);  // Changing any flags adds the dirty flag
			}
		}
		else
		{
			if (EnumHasAnyFlags(CurrentFlags, Flags))
			{
				EnumRemoveFlags(GetFlagsArray()[KeyIndex], Flags);
				EnumAddFlags(GetFlagsArray()[KeyIndex], ECollectionPropertyFlags::Dirty);  // Changing any flags adds the dirty flag, dirty flags can only be removed from the ClearDirtyFlags function
			}
		}
	}

	void FCollectionPropertyFacade::UpdateProperties(const TSharedPtr<const FManagedArrayCollection>& InManagedArrayCollection)
	{
		FCollectionPropertyConstFacade InPropertyFacade(InManagedArrayCollection);
		if (InPropertyFacade.IsValid())
		{
			const int32 NumInKeys = InPropertyFacade.Num();
			for (int32 InKeyIndex = 0; InKeyIndex < NumInKeys; ++InKeyIndex)
			{
				const FString& PropertyName = InPropertyFacade.GetKey(InKeyIndex);
				const int32 PropertyIndex = GetKeyIndex(PropertyName);
				if (PropertyIndex == INDEX_NONE)
				{
					continue;
				}
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
				// TODO: GetFlags needs to return an ECollectionPropertyFlags, not an uint8, but the uint8 getter needs to be deprecated first
				SetFlags(PropertyIndex, (ECollectionPropertyFlags)InPropertyFacade.GetFlags(InKeyIndex));
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
				// Setting as FVector3f since that is the underlying type
				SetLowValue(PropertyIndex, InPropertyFacade.GetLowValue<FVector3f>(InKeyIndex));
				SetHighValue(PropertyIndex, InPropertyFacade.GetHighValue<FVector3f>(InKeyIndex));
				SetStringValue(PropertyIndex, InPropertyFacade.GetStringValue(InKeyIndex));
			}
		}
	}

	FCollectionPropertyMutableFacade::FCollectionPropertyMutableFacade(const TSharedPtr<FManagedArrayCollection>& InManagedArrayCollection)
		: FCollectionPropertyFacade(InManagedArrayCollection, NoInit)
	{
		UpdateArrays();
		RebuildKeyIndices();
	}

	void FCollectionPropertyMutableFacade::DefineSchema()
	{
		GetManagedArrayCollection()->AddAttribute<FString>(PropertyFacadeNames::KeyName, PropertyFacadeNames::PropertyGroup);
		GetManagedArrayCollection()->AddAttribute<FVector3f>(PropertyFacadeNames::LowValueName, PropertyFacadeNames::PropertyGroup);
		GetManagedArrayCollection()->AddAttribute<FVector3f>(PropertyFacadeNames::HighValueName, PropertyFacadeNames::PropertyGroup);
		GetManagedArrayCollection()->AddAttribute<FString>(PropertyFacadeNames::StringValueName, PropertyFacadeNames::PropertyGroup);
		GetManagedArrayCollection()->AddAttribute<uint8>(PropertyFacadeNames::FlagsName, PropertyFacadeNames::PropertyGroup);

		UpdateArrays();
		RebuildKeyIndices();
	}

	int32 FCollectionPropertyMutableFacade::AddProperty(const FString& Key, bool bEnabled, bool bAnimatable, bool bIntrinsic)
	{
		const ECollectionPropertyFlags Flags =
			(bEnabled ? ECollectionPropertyFlags::Enabled : ECollectionPropertyFlags::None) |
			(bAnimatable ? ECollectionPropertyFlags::Animatable : ECollectionPropertyFlags::None) |
			(bIntrinsic ? ECollectionPropertyFlags::Intrinsic : ECollectionPropertyFlags::None);
		return AddProperty(Key, Flags);
	}

	int32 FCollectionPropertyMutableFacade::AddProperty(const FString& Key, ECollectionPropertyFlags Flags)
	{
		const int32 Index = GetManagedArrayCollection()->AddElements(1, PropertyFacadeNames::PropertyGroup);

		// Update the arrayviews in case the new element triggered a reallocation 
		UpdateArrays();

		// Setup the new element's default value and enable the property by default
		GetKeyArray()[Index] = Key;
		GetLowValueArray()[Index] = GetHighValueArray()[Index] = FVector3f::ZeroVector;
		GetFlagsArray()[Index] = Flags;

		// Update search map
		KeyIndices.Emplace(KeyArray[Index], Index);

		return Index;
	}

	int32 FCollectionPropertyMutableFacade::AddProperties(const TArray<FString>& Keys, bool bEnabled, bool bAnimatable, bool bIntrinsic)
	{
		const ECollectionPropertyFlags Flags =
			(bEnabled ? ECollectionPropertyFlags::Enabled : ECollectionPropertyFlags::None) |
			(bAnimatable ? ECollectionPropertyFlags::Animatable : ECollectionPropertyFlags::None) |
			(bIntrinsic ? ECollectionPropertyFlags::Intrinsic : ECollectionPropertyFlags::None);

		return AddProperties(Keys, Flags);
	}

	int32 FCollectionPropertyMutableFacade::AddProperties(const TArray<FString>& Keys, ECollectionPropertyFlags Flags)
	{
		if (const int32 NumProperties = Keys.Num())
		{
			const int32 StartIndex = GetManagedArrayCollection()->AddElements(NumProperties, PropertyFacadeNames::PropertyGroup);

			// Update the arrayviews in case the new elements triggered a reallocation 
			UpdateArrays();

			for (int32 Index = StartIndex; Index < NumProperties + StartIndex; ++Index)
			{
				// Setup the new elements' default value and enable the property by default
				GetKeyArray()[Index] = Keys[Index - StartIndex];
				GetLowValueArray()[Index] = GetHighValueArray()[Index] = FVector3f::ZeroVector;
				GetFlagsArray()[Index] = Flags;

				// Update search map
				KeyIndices.Emplace(KeyArray[Index], Index);
			}

			return StartIndex;
		}
		return INDEX_NONE;
	}

	void FCollectionPropertyMutableFacade::Append(const TSharedPtr<const FManagedArrayCollection>& InManagedArrayCollection, bool bUpdateExistingProperties)
	{
		
		Update(InManagedArrayCollection, ECollectionPropertyUpdateFlags::AppendNewProperties | (bUpdateExistingProperties ? ECollectionPropertyUpdateFlags::UpdateExistingProperties : ECollectionPropertyUpdateFlags::None));
	}

	void FCollectionPropertyMutableFacade::Append(const FManagedArrayCollection& InManagedArrayCollection)
	{
		constexpr ECollectionPropertyUpdateFlags UpdateFlagsAppendNewOnly = ECollectionPropertyUpdateFlags::AppendNewProperties;
		Update(MakeShared<const FManagedArrayCollection>(InManagedArrayCollection), UpdateFlagsAppendNewOnly);
	}

	void FCollectionPropertyMutableFacade::Copy(const FManagedArrayCollection& InManagedArrayCollection)
	{
		GetManagedArrayCollection()->Reset();
		DefineSchema();

		TArray<FName> GroupsToSkip = InManagedArrayCollection.GroupNames();
		GroupsToSkip.RemoveSingleSwap(PropertyFacadeNames::PropertyGroup);

		InManagedArrayCollection.CopyTo(GetManagedArrayCollection().Get(), GroupsToSkip);
		UpdateArrays();
		RebuildKeyIndices();
	}

	void FCollectionPropertyMutableFacade::Update(const TSharedPtr<const FManagedArrayCollection>& InManagedArrayCollection, ECollectionPropertyUpdateFlags UpdateFlags)
	{
		if (UpdateFlags == ECollectionPropertyUpdateFlags::None)
		{
			// Nothing to do
			return;
		}
		if (UpdateFlags == ECollectionPropertyUpdateFlags::UpdateExistingProperties)
		{
			return UpdateProperties(InManagedArrayCollection);
		}

		const bool bAppendNewProperties = (UpdateFlags & ECollectionPropertyUpdateFlags::AppendNewProperties) != ECollectionPropertyUpdateFlags::None;
		const bool bUpdateExistingProperties = (UpdateFlags & ECollectionPropertyUpdateFlags::UpdateExistingProperties) != ECollectionPropertyUpdateFlags::None;
		const bool bRemoveMissingProperties = (UpdateFlags & ECollectionPropertyUpdateFlags::RemoveMissingProperties) != ECollectionPropertyUpdateFlags::None;
		const bool bDisableMissingProperties = (UpdateFlags & ECollectionPropertyUpdateFlags::DisableMissingProperties) != ECollectionPropertyUpdateFlags::None;

		FCollectionPropertyConstFacade InPropertyFacade(InManagedArrayCollection);
		if (InPropertyFacade.IsValid())
		{
			DefineSchema();

			const int32 PrevNumKeys = Num();

			if (bAppendNewProperties || bUpdateExistingProperties)
			{
				const int32 NumInKeys = InPropertyFacade.Num();
				for (int32 InKeyIndex = 0; InKeyIndex < NumInKeys; ++InKeyIndex)
				{
					const FString& PropertyName = InPropertyFacade.GetKey(InKeyIndex);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
					// TODO: GetFlags needs to return an ECollectionPropertyFlags, not an uint8, but the uint8 getter needs to be deprecated first
					const ECollectionPropertyFlags PropertyFlags = (ECollectionPropertyFlags)InPropertyFacade.GetFlags(InKeyIndex);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
					int32 NewPropertyIndex = GetKeyIndex(PropertyName);
					if (NewPropertyIndex == INDEX_NONE)
					{
						if (bAppendNewProperties)
						{
							NewPropertyIndex = AddProperty(PropertyName, PropertyFlags);
						}
						else
						{
							continue;
						}
					}
					else if (!bUpdateExistingProperties)
					{
						continue;
					}
					else
					{
						SetFlags(NewPropertyIndex, PropertyFlags);
					}

					// Setting as FVector3f since that is the underlying type
					SetLowValue(NewPropertyIndex, InPropertyFacade.GetLowValue<FVector3f>(InKeyIndex));
					SetHighValue(NewPropertyIndex, InPropertyFacade.GetHighValue<FVector3f>(InKeyIndex));
					SetStringValue(NewPropertyIndex, InPropertyFacade.GetStringValue(InKeyIndex));
				}
			}
			if (bRemoveMissingProperties || bDisableMissingProperties)
			{
				TArray<int32> KeyIndicesToRemove;
				for (int32 ExistingKeyIndex = 0; ExistingKeyIndex < PrevNumKeys; ++ExistingKeyIndex)
				{
					if (InPropertyFacade.GetKeyIndex(GetKey(ExistingKeyIndex)) == INDEX_NONE)
					{
						if (bRemoveMissingProperties)
						{
							KeyIndicesToRemove.Add(ExistingKeyIndex);
						}
						else if(bDisableMissingProperties)
						{
							SetEnabled(ExistingKeyIndex, false);
						}
					}
				}

				if (!KeyIndicesToRemove.IsEmpty())
				{
					GetManagedArrayCollection()->RemoveElements(PropertyFacadeNames::PropertyGroup, KeyIndicesToRemove);
					UpdateArrays();
					RebuildKeyIndices();
				}
			}
		}
	}

	int32 FCollectionPropertyMutableFacade::AddWeightedFloatValue(const FString& Key, const FVector2f& Value, bool bEnabled, bool bAnimatable, bool bIntrinsic)
	{
		const int32 KeyIndex = AddProperty(Key, bEnabled, bAnimatable, bIntrinsic);
		SetWeightedFloatValue(KeyIndex, Value);
		return KeyIndex;
	}

	int32 FCollectionPropertyMutableFacade::AddWeightedFloatValue(const FString& Key, const FVector2f& Value, ECollectionPropertyFlags Flags)
	{
		const int32 KeyIndex = AddProperty(Key, Flags);
		SetWeightedFloatValue(KeyIndex, Value);
		return KeyIndex;
	}

	int32 FCollectionPropertyMutableFacade::AddStringValue(const FString& Key, const FString& Value, bool bEnabled, bool bAnimatable, bool bIntrinsic)
	{
		const int32 KeyIndex = AddProperty(Key, bEnabled, bAnimatable, bIntrinsic);
		SetStringValue(Key, Value);
		return KeyIndex;
	}

	int32 FCollectionPropertyMutableFacade::AddStringValue(const FString& Key, const FString& Value, ECollectionPropertyFlags Flags)
	{
		const int32 KeyIndex = AddProperty(Key, Flags);
		SetStringValue(Key, Value);
		return KeyIndex;
	}
}  // End namespace Chaos::Softs
