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
		return ValueArray[KeyIndex];
	}
	template CHAOS_API FVector3f FCollectionPropertyConstFacade::GetValue<FVector3f, FVector3f>(int32 KeyIndex, const TConstArrayView<FVector3f>& ValueArray) const;
	template CHAOS_API const FVector3f& FCollectionPropertyConstFacade::GetValue<const FVector3f&, FVector3f>(int32 KeyIndex, const TConstArrayView<FVector3f>& ValueArray) const;
	template CHAOS_API FString FCollectionPropertyConstFacade::GetValue<FString, FString>(int32 KeyIndex, const TConstArrayView<FString>& ValueArray) const;
	template CHAOS_API const FString& FCollectionPropertyConstFacade::GetValue<const FString&, FString>(int32 KeyIndex, const TConstArrayView<FString>& ValueArray) const;
	template CHAOS_API uint8 FCollectionPropertyConstFacade::GetValue<uint8, uint8>(int32 KeyIndex, const TConstArrayView<uint8>& ValueArray) const;

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

	FVector2f FCollectionPropertyConstFacade::GetWeightedFloatValue(const FString& Key, const float& Default, int32* OutKeyIndex) const
	{
		const int32 KeyIndex = GetKeyIndex(Key);
		if (OutKeyIndex)
		{
			*OutKeyIndex = KeyIndex;
		}
		return KeyIndex != INDEX_NONE ? GetWeightedFloatValue(KeyIndex) : FVector2f(Default, Default);
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

	int32 FCollectionPropertyFacade::SetWeightedFloatValue(const FString& Key, const FVector2f& Value)
	{
		const int32 KeyIndex = GetKeyIndex(Key);
		if (KeyIndex != INDEX_NONE)
		{
			SetWeightedFloatValue(KeyIndex, Value);
		}
		return KeyIndex;
	}

	void FCollectionPropertyFacade::EnableFlag(int32 KeyIndex, ECollectionPropertyFlag Flag, bool bEnable)
	{
		if (bEnable)
		{
			EnumAddFlags(GetFlagsArray()[KeyIndex], (uint8)Flag);
		}
		else
		{
			EnumRemoveFlags(GetFlagsArray()[KeyIndex], (uint8)Flag);
		}
	}

	int32 FCollectionPropertyFacade::EnableFlag(const FString& Key, ECollectionPropertyFlag Flag, bool bEnable)
	{
		const int32 KeyIndex = GetKeyIndex(Key);
		if (KeyIndex != INDEX_NONE)
		{
			EnableFlag(KeyIndex, Flag, bEnable);
		}
		return KeyIndex;
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

	int32 FCollectionPropertyMutableFacade::AddProperty(const FString& Key, bool bEnabled, bool bAnimatable)
	{
		const int32 Index = GetManagedArrayCollection()->AddElements(1, PropertyFacadeNames::PropertyGroup);
		const uint8 Flags = (bEnabled ? (uint8)ECollectionPropertyFlag::Enabled : 0) | (bAnimatable ? (uint8)ECollectionPropertyFlag::Animatable : 0);

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

	int32 FCollectionPropertyMutableFacade::AddProperties(const TArray<FString>& Keys, bool bEnabled, bool bAnimatable)
	{
		if (const int32 NumProperties = Keys.Num())
		{
			const int32 StartIndex = GetManagedArrayCollection()->AddElements(NumProperties, PropertyFacadeNames::PropertyGroup);
			const uint8 Flags = (bEnabled ? (uint8)ECollectionPropertyFlag::Enabled : 0) | (bAnimatable ? (uint8)ECollectionPropertyFlag::Animatable : 0);

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

	void FCollectionPropertyMutableFacade::Append(const FManagedArrayCollection& InManagedArrayCollection)
	{
		TArray<FName> GroupsToSkip = InManagedArrayCollection.GroupNames();
		GroupsToSkip.RemoveSingleSwap(PropertyFacadeNames::PropertyGroup);

		InManagedArrayCollection.CopyTo(GetManagedArrayCollection().Get(), GroupsToSkip);
		UpdateArrays();
		RebuildKeyIndices();
	}

	void FCollectionPropertyMutableFacade::Copy(const FManagedArrayCollection& InManagedArrayCollection)
	{
		GetManagedArrayCollection()->Reset();
		DefineSchema();
		Append(InManagedArrayCollection);
	}

	int32 FCollectionPropertyMutableFacade::AddWeightedFloatValue(const FString& Key, const FVector2f& Value, bool bEnabled, bool bAnimatable)
	{
		const int32 KeyIndex = AddProperty(Key, bEnabled, bAnimatable);
		SetWeightedFloatValue(KeyIndex, Value);
		return KeyIndex;
	}

	int32 FCollectionPropertyMutableFacade::AddStringValue(const FString& Key, const FString& Value, bool bEnabled, bool bAnimatable)
	{
		const int32 KeyIndex = AddProperty(Key, bEnabled, bAnimatable);
		SetStringValue(Key, Value);
		return KeyIndex;
	}
}  // End namespace Chaos
