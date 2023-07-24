// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"
#include "Containers/Map.h"
#include "Math/Vector.h"

struct FManagedArrayCollection;

namespace Chaos::Softs
{
	/** Property flags, whether properties are enabled, animatable, ...etc. */
	enum class ECollectionPropertyFlag : uint8
	{
		None,
		Enabled = 1 << 0,  /** Whether this property is enabled(so that it doesn't have to be removed from the collection when not needed). */
		Animatable = 1 << 1,  /** Whether this property needs to be set at every frame. */
		//~ Add new flags above this line
		Dirty = 1 << 7  /** Whether this property has changed and needs to be updated at the next frame. */
	};
	ENUM_CLASS_FLAGS(ECollectionPropertyFlag)

	/** Weighted types are all property types that can have a pair of low and high values to be associated with a weight map. */
	template<typename T> struct TIsWeightedType { static constexpr bool Value = false; };
	template<> struct TIsWeightedType<bool> { static constexpr bool Value = true; };
	template<> struct TIsWeightedType<int32> { static constexpr bool Value = true; };
	template<> struct TIsWeightedType<float> { static constexpr bool Value = true; };
	template<> struct TIsWeightedType<FVector3f> { static constexpr bool Value = true; };

	/**
	 * Defines common API for reading simulation properties data and metadata.
	 * This is mainly used for the cloth simulation properties to provide
	 * weighted values that works in conjunction with weight maps.
	 */
	class CHAOS_API FCollectionPropertyConstFacade
	{
	public:
		explicit FCollectionPropertyConstFacade(const TSharedPtr<const FManagedArrayCollection>& InManagedArrayCollection);
		virtual ~FCollectionPropertyConstFacade() = default;

		FCollectionPropertyConstFacade() = delete;
		FCollectionPropertyConstFacade(const FCollectionPropertyConstFacade&) = delete;
		FCollectionPropertyConstFacade& operator=(const FCollectionPropertyConstFacade&) = delete;

		FCollectionPropertyConstFacade(FCollectionPropertyConstFacade&&) = default;
		FCollectionPropertyConstFacade& operator=(FCollectionPropertyConstFacade&&) = default;

		/** Return whether the facade is defined on the collection. */
		bool IsValid() const;

		/** Return the number of properties in this collection. */
		int32 Num() const { return KeyIndices.Num(); }

		/** Return the property index for the specified key if it exists, or INDEX_NONE otherwise. */
		int32 GetKeyIndex(const FString& Key) const { const int32* const Index = KeyIndices.Find(Key); return Index ? *Index : INDEX_NONE; }

		//~ Values access per index, fast, no check, index must be valid (0 <= KeyIndex < Num())
		template<typename T, TEMPLATE_REQUIRES(TIsWeightedType<T>::Value)>
		T GetLowValue(int32 KeyIndex) const { return GetValue<T>(KeyIndex, LowValueArray); }

		template<typename T, TEMPLATE_REQUIRES(TIsWeightedType<T>::Value)>
		T GetHighValue(int32 KeyIndex) const { return GetValue<T>(KeyIndex, HighValueArray); }

		template<typename T, TEMPLATE_REQUIRES(TIsWeightedType<T>::Value)>
		TPair<T, T> GetWeightedValue(int32 KeyIndex) const { return MakeTuple(GetLowValue<T>(KeyIndex), GetHighValue<T>(KeyIndex)); }

		FVector2f GetWeightedFloatValue(int32 KeyIndex) const { return FVector2f(GetLowValue<float>(KeyIndex), GetHighValue<float>(KeyIndex)); }

		template<typename T, TEMPLATE_REQUIRES(TIsWeightedType<T>::Value)>
		T GetValue(int32 KeyIndex) const { return GetLowValue<T>(KeyIndex); }

		const FString& GetStringValue(int32 KeyIndex, const FString& Default = "") const { return GetValue<const FString&>(KeyIndex, StringValueArray); }

		uint8 GetFlags(int32 KeyIndex) const { return FlagsArray[KeyIndex]; }
		bool IsEnabled(int32 KeyIndex) const { return EnumHasAnyFlags((ECollectionPropertyFlag)FlagsArray[KeyIndex], ECollectionPropertyFlag::Enabled); }
		bool IsAnimatable(int32 KeyIndex) const { return EnumHasAnyFlags((ECollectionPropertyFlag)FlagsArray[KeyIndex], ECollectionPropertyFlag::Animatable); }
		bool IsDirty(int32 KeyIndex) const { return EnumHasAnyFlags((ECollectionPropertyFlag)FlagsArray[KeyIndex], ECollectionPropertyFlag::Dirty); }

		//~ Values access per key
		template<typename T, TEMPLATE_REQUIRES(TIsWeightedType<T>::Value)>
		T GetLowValue(const FString& Key, const T& Default = T(0), int32* OutKeyIndex = nullptr) const { return GetValue(Key, LowValueArray, Default, OutKeyIndex); }

		template<typename T, TEMPLATE_REQUIRES(TIsWeightedType<T>::Value)>
		T GetHighValue(const FString& Key, const T& Default = T(0), int32* OutKeyIndex = nullptr) const { return GetValue(Key, HighValueArray, Default, OutKeyIndex); }

		template<typename T, TEMPLATE_REQUIRES(TIsWeightedType<T>::Value)>
		inline TPair<T, T> GetWeightedValue(const FString& Key, const T& Default = T(0), int32* OutKeyIndex = nullptr) const;

		FVector2f GetWeightedFloatValue(const FString& Key, const float& Default = 0.f, int32* OutKeyIndex = nullptr) const;

		template<typename T, TEMPLATE_REQUIRES(TIsWeightedType<T>::Value)>
		T GetValue(const FString& Key, const T& Default = T(0), int32* OutKeyIndex = nullptr) const { return GetValue(Key, LowValueArray, Default, OutKeyIndex); }

		FString GetStringValue(const FString& Key, const FString& Default = "", int32* OutKeyIndex = nullptr) const { return GetValue(Key, StringValueArray, Default, OutKeyIndex); }

		uint8 GetFlags(const FString& Key, uint8 Default = 0, int32* OutKeyIndex = nullptr) const { return GetValue(Key, FlagsArray, (uint8)Default, OutKeyIndex); }
		bool IsEnabled(const FString& Key, bool bDefault = false, int32* OutKeyIndex = nullptr) const { return EnumHasAnyFlags((ECollectionPropertyFlag)GetValue(Key, FlagsArray, (uint8)bDefault, OutKeyIndex), ECollectionPropertyFlag::Enabled); }
		bool IsAnimatable(const FString& Key, bool bDefault = false, int32* OutKeyIndex = nullptr) const { return EnumHasAnyFlags((ECollectionPropertyFlag)GetValue(Key, FlagsArray, (uint8)bDefault, OutKeyIndex), ECollectionPropertyFlag::Animatable); }
		bool IsDirty(const FString& Key, bool bDefault = false, int32* OutKeyIndex = nullptr) const { return EnumHasAnyFlags((ECollectionPropertyFlag)GetValue(Key, FlagsArray, (uint8)bDefault, OutKeyIndex), ECollectionPropertyFlag::Dirty); }

	protected:
		// No init constructor for FCollectionPropertyFacade
		FCollectionPropertyConstFacade(const TSharedPtr<const FManagedArrayCollection>& InManagedArrayCollection, ENoInit);

		// Update the array views
		void UpdateArrays();

		// Rebuild the search map
		void RebuildKeyIndices();

		template<typename T, typename ElementType>
		T GetValue(int32 KeyIndex, const TConstArrayView<ElementType>& ValueArray) const;

		template<typename T, typename ElementType>
		inline T GetValue(const FString& Key, const TConstArrayView<ElementType>& ValueArray, const T& Default, int32* OutKeyIndex) const;

		template <typename T>
		TConstArrayView<T> GetArray(const FName& Name) const;

		// Attribute groups, predefined data member of the collection
		static const FName PropertyGroup;
		static const FName KeyName;  // Property key, name to look for
		static const FName LowValueName;  // Boolean, 24 bit integer (max 16777215), float, or vector value, or value of the lowest weight on the weight map if any
		static const FName HighValueName;  // Boolean, 24 bit integer (max 16777215), float, or vector value of the highest weight on the weight map if any
		static const FName StringValueName;  // String value, or weight map name, ...etc.
		static const FName FlagsName;  // Whether this property is enabled, animatable, ...etc.

		// Property Group array views
		TConstArrayView<FString> KeyArray;
		TConstArrayView<FVector3f> LowValueArray;
		TConstArrayView<FVector3f> HighValueArray;
		TConstArrayView<FString> StringValueArray;
		TConstArrayView<uint8> FlagsArray;

		// Key to index search map
		TMap<FString, int32> KeyIndices;

		// Property collection
		TSharedPtr<const FManagedArrayCollection> ManagedArrayCollection;
	};

	/**
	 * Defines common API for reading and writing simulation properties data and metadata.
	 * This is mainly used for the cloth simulation properties to provide
	 * weighted values that works in conjunction with weight maps.
	 * 
	 * Note: Int property values are stored as float and therefore limited to 24 bits (-16777215 to 16777215).
	 */
	class CHAOS_API FCollectionPropertyFacade : public FCollectionPropertyConstFacade
	{
	public:
		explicit FCollectionPropertyFacade(const TSharedPtr<FManagedArrayCollection>& InManagedArrayCollection);
		virtual ~FCollectionPropertyFacade() = default;

		FCollectionPropertyFacade() = delete;
		FCollectionPropertyFacade(const FCollectionPropertyFacade&) = delete;
		FCollectionPropertyFacade& operator=(const FCollectionPropertyFacade&) = delete;

		FCollectionPropertyFacade(FCollectionPropertyFacade&&) = default;
		FCollectionPropertyFacade& operator=(FCollectionPropertyFacade&&) = default;

		//~ Values set per index
		template<typename T, TEMPLATE_REQUIRES(TIsWeightedType<T>::Value)>
		void SetLowValue(int32 KeyIndex, const T& Value) { GetLowValueArray()[KeyIndex] = FVector3f(Value); }

		template<typename T, TEMPLATE_REQUIRES(TIsWeightedType<T>::Value)>
		void SetHighValue(int32 KeyIndex, const T& Value) { GetHighValueArray()[KeyIndex] = FVector3f(Value); }

		template<typename T, TEMPLATE_REQUIRES(TIsWeightedType<T>::Value)>
		void SetWeightedValue(int32 KeyIndex, const T& LowValue, const T& HighValue) { SetLowValue(KeyIndex, LowValue); SetHighValue(KeyIndex, HighValue); }

		void SetWeightedFloatValue(int32 KeyIndex, const FVector2f& Value) { SetLowValue<float>(KeyIndex, Value.X); SetHighValue<float>(KeyIndex, Value.Y); }

		template<typename T, TEMPLATE_REQUIRES(TIsWeightedType<T>::Value)>
		void SetValue(int32 KeyIndex, const T& Value) { SetWeightedValue(KeyIndex, Value, Value); }

		void SetStringValue(int32 KeyIndex, const FString& Value) { GetStringValueArray()[KeyIndex] = Value; }

		void SetFlags(int32 KeyIndex, uint8 Flags) { GetFlagsArray()[KeyIndex] = Flags; }
		void SetEnabled(int32 KeyIndex, bool bEnabled) { EnableFlag(KeyIndex, ECollectionPropertyFlag::Enabled, bEnabled); }
		void SetAnimatable(int32 KeyIndex, bool bAnimatable) { EnableFlag(KeyIndex, ECollectionPropertyFlag::Animatable, bAnimatable); }
		void SetDirty(int32 KeyIndex, bool bDirty) { EnableFlag(KeyIndex, ECollectionPropertyFlag::Animatable, bDirty); }

		//~ Values set per key
		template<typename T, TEMPLATE_REQUIRES(TIsWeightedType<T>::Value)>
		int32 SetLowValue(const FString& Key, const T& Value) { return SetValue(Key, GetLowValueArray(), FVector3f(Value)); }

		template<typename T, TEMPLATE_REQUIRES(TIsWeightedType<T>::Value)>
		int32 SetHighValue(const FString& Key, const T& Value) { return SetValue(Key, GetHighValueArray(), FVector3f(Value)); }

		template<typename T, TEMPLATE_REQUIRES(TIsWeightedType<T>::Value)>
		inline int32 SetWeightedValue(const FString& Key, const T& LowValue, const T& HighValue);

		int32 SetWeightedFloatValue(const FString& Key, const FVector2f& Value);

		template<typename T, TEMPLATE_REQUIRES(TIsWeightedType<T>::Value)>
		int32 SetValue(const FString& Key, const T& Value) { return SetWeightedValue(Key, Value, Value); }

		int32 SetStringValue(const FString& Key, const FString& Value) { return SetValue(Key, GetStringValueArray(), Value); }

		int32 SetFlags(const FString& Key, uint8 Flags) { return SetValue(Key, GetFlagsArray(), Flags); }
		int32 SetEnabled(const FString& Key, bool bEnabled) { return EnableFlag(Key, ECollectionPropertyFlag::Enabled, bEnabled); }
		int32 SetAnimatable(const FString& Key, bool bAnimatable) { return EnableFlag(Key, ECollectionPropertyFlag::Animatable, bAnimatable); }
		int32 SetDirty(const FString& Key, bool bDirty) { return EnableFlag(Key, ECollectionPropertyFlag::Animatable, bDirty); }

	protected:
		// No init constructor for FCollectionPropertyMutableFacade
		FCollectionPropertyFacade(const TSharedPtr<FManagedArrayCollection>& InManagedArrayCollection, ENoInit);

		// Access to a writeable ManagedArrayCollection is protected, use an FPropertyCollectionMutableAdapter if needed to get a non const pointer
		TSharedPtr<FManagedArrayCollection> GetManagedArrayCollection() { return ConstCastSharedPtr<FManagedArrayCollection>(ManagedArrayCollection); }

		const TArrayView<FString>& GetKeyArray() { return reinterpret_cast<TArrayView<FString>&>(KeyArray); }
		const TArrayView<FVector3f>& GetLowValueArray() { return reinterpret_cast<TArrayView<FVector3f>&>(LowValueArray); }
		const TArrayView<FVector3f>& GetHighValueArray() { return reinterpret_cast<TArrayView<FVector3f>&>(HighValueArray); }
		const TArrayView<FString>& GetStringValueArray() { return reinterpret_cast<TArrayView<FString>&>(StringValueArray); }
		const TArrayView<uint8>& GetFlagsArray() { return reinterpret_cast<const TArrayView<uint8>&>(FlagsArray); }

	private:
		template<typename T>
		inline int32 SetValue(const FString& Key, const TArrayView<T>& ValueArray, const T& Value);

		void EnableFlag(int32 KeyIndex, ECollectionPropertyFlag Flag, bool bEnable);
		int32 EnableFlag(const FString& Key, ECollectionPropertyFlag Flag, bool bEnable);
	};

	/**
	 * Defines common API for reading and writing, and adding/removing simulation properties data and metadata.
	 * This is mainly used for the cloth simulation properties to provide
	 * weighted values that works in conjunction with weight maps.
	 */
	class CHAOS_API FCollectionPropertyMutableFacade final : public FCollectionPropertyFacade
	{
	public:
		explicit FCollectionPropertyMutableFacade(const TSharedPtr<FManagedArrayCollection>& InManagedArrayCollection);
		virtual ~FCollectionPropertyMutableFacade() = default;

		FCollectionPropertyMutableFacade() = delete;
		FCollectionPropertyMutableFacade(const FCollectionPropertyMutableFacade&) = delete;
		FCollectionPropertyMutableFacade& operator=(const FCollectionPropertyMutableFacade&) = delete;

		FCollectionPropertyMutableFacade(FCollectionPropertyMutableFacade&&) = default;
		FCollectionPropertyMutableFacade& operator=(FCollectionPropertyMutableFacade&&) = default;

		/** Create this facade's groups and attributes. */
		void DefineSchema();

		/** Add a single property, and return its index. */
		int32 AddProperty(const FString& Key, bool bEnabled = true, bool bAnimatable = false);

		/** Add new properties, and return the index of the first added property. */
		int32 AddProperties(const TArray<FString>& Keys, bool bEnabled = true, bool bAnimatable = false);

		/**
		 * Append all properties and values from an existing collection to this property collection.
		 * This won't copy any other groups, only data from PropertyGroup.
		 * Any pre-existing data will be preserved.
		 */
		void Append(const FManagedArrayCollection& InManagedArrayCollection);

		/**
		 * Copy all properties and values from an existing collection to this property collection.
		 * This won't copy any other groups, only data from PropertyGroup.
		 * Any pre-xisting data will be removed/replaced.
		 */
		void Copy(const FManagedArrayCollection& InManagedArrayCollection);

		//~ Add values
		template<typename T, TEMPLATE_REQUIRES(TIsWeightedType<T>::Value)>
		inline int32 AddWeightedValue(const FString& Key, const T& LowValue, const T& HighValue, bool bEnabled = true, bool bAnimatable = false);

		int32 AddWeightedFloatValue(const FString& Key, const FVector2f& Value, bool bEnabled, bool bAnimatable);

		template<typename T, TEMPLATE_REQUIRES(TIsWeightedType<T>::Value)>
		int32 AddValue(const FString& Key, const T& Value, bool bEnabled = true, bool bAnimatable = false) { return AddWeightedValue(Key, Value, Value, bEnabled, bAnimatable); }

		int32 AddStringValue(const FString& Key, const FString& Value, bool bEnabled = true, bool bAnimatable = false);
	};

	template<typename T, typename TEnableIf<TIsWeightedType<T>::Value, int>::type>
	inline TPair<T, T> FCollectionPropertyConstFacade::GetWeightedValue(const FString& Key, const T& Default, int32* OutKeyIndex) const
	{
		const int32 KeyIndex = GetKeyIndex(Key);
		if (OutKeyIndex)
		{
			*OutKeyIndex = KeyIndex;
		}
		return KeyIndex != INDEX_NONE ? GetWeightedValue<T>(KeyIndex) : MakeTuple(Default, Default);
	}

	template<typename T, typename ElementType>
	inline T FCollectionPropertyConstFacade::GetValue(const FString& Key, const TConstArrayView<ElementType>& ValueArray, const T& Default, int32* OutKeyIndex) const
	{
		const int32 KeyIndex = GetKeyIndex(Key);
		if (OutKeyIndex)
		{
			*OutKeyIndex = KeyIndex;
		}
		return KeyIndex != INDEX_NONE ? GetValue<T>(KeyIndex, ValueArray) : Default;
	}

	template<typename T, typename TEnableIf<TIsWeightedType<T>::Value, int>::type>
	inline int32 FCollectionPropertyFacade::SetWeightedValue(const FString& Key, const T& LowValue, const T& HighValue)
	{
		const int32 KeyIndex = GetKeyIndex(Key);
		if (KeyIndex != INDEX_NONE)
		{
			SetLowValue(KeyIndex, LowValue);
			SetHighValue(KeyIndex, HighValue);
		}
		return KeyIndex;
	}

	template<typename T>
	inline int32 FCollectionPropertyFacade::SetValue(const FString& Key, const TArrayView<T>& ValueArray, const T& Value)
	{
		const int32 KeyIndex = GetKeyIndex(Key);
		if (KeyIndex != INDEX_NONE)
		{
			ValueArray[KeyIndex] = Value;
		}
		return KeyIndex;
	}

	template<typename T, typename TEnableIf<TIsWeightedType<T>::Value, int>::type>
	inline int32 FCollectionPropertyMutableFacade::AddWeightedValue(const FString& Key, const T& LowValue, const T& HighValue, bool bEnabled, bool bAnimatable)
	{
		const int32 KeyIndex = AddProperty(Key, bEnabled, bAnimatable);
		SetWeightedValue(KeyIndex, LowValue, HighValue);
		return KeyIndex;
	}
}  // End namespace Chaos

// Use this macro to add shorthands for property getters and direct access through the declared key index
#define UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(PropertyName, Type) \
	inline static const FName PropertyName##Name = TEXT(#PropertyName); \
	static FString PropertyName##String() { return PropertyName##Name.ToString(); } \
	static bool Is##PropertyName##Enabled(const FCollectionPropertyConstFacade& PropertyCollection, bool bDefault) \
	{ \
		return PropertyCollection.IsEnabled(PropertyName##String(), bDefault); \
	} \
	static bool Is##PropertyName##Animatable(const FCollectionPropertyConstFacade& PropertyCollection, bool bDefault) \
	{ \
		return PropertyCollection.IsAnimatable(PropertyName##String(), bDefault); \
	} \
	Type GetLow##PropertyName(const FCollectionPropertyConstFacade& PropertyCollection, const Type& Default) \
	{ \
		return PropertyCollection.GetLowValue<Type>(PropertyName##String(), Default, &PropertyName##Index); \
	} \
	Type GetHigh##PropertyName(const FCollectionPropertyConstFacade& PropertyCollection, const Type& Default) \
	{ \
		return PropertyCollection.GetHighValue<Type>(PropertyName##String(), Default, &PropertyName##Index); \
	} \
	TPair<Type, Type> GetWeighted##PropertyName(const FCollectionPropertyConstFacade& PropertyCollection, const Type& Default) \
	{ \
		return PropertyCollection.GetWeightedValue<Type>(PropertyName##String(), Default, &PropertyName##Index); \
	} \
	FVector2f GetWeightedFloat##PropertyName(const FCollectionPropertyConstFacade& PropertyCollection, const float& Default) \
	{ \
		return PropertyCollection.GetWeightedFloatValue(PropertyName##String(), Default, &PropertyName##Index); \
	} \
	Type Get##PropertyName(const FCollectionPropertyConstFacade& PropertyCollection, const Type& Default) \
	{ \
		return PropertyCollection.GetValue<Type>(PropertyName##String(), Default, &PropertyName##Index); \
	} \
	uint8 Get##PropertyName##Flags(const FCollectionPropertyConstFacade& PropertyCollection, uint8 Default) \
	{ \
		return PropertyCollection.GetFlags(PropertyName##String(), Default, &PropertyName##Index); \
	} \
	Type GetLow##PropertyName(const FCollectionPropertyConstFacade& PropertyCollection) const \
	{ \
		checkSlow(PropertyName##Index == PropertyCollection.GetKeyIndex(PropertyName##String())); \
		checkf(PropertyName##Index != INDEX_NONE, TEXT("The default value getter that sets the property index must be called once prior to calling this function.")); \
		return PropertyCollection.GetLowValue<Type>(PropertyName##Index); \
	} \
	Type GetHigh##PropertyName(const FCollectionPropertyConstFacade& PropertyCollection) const \
	{ \
		checkSlow(PropertyName##Index == PropertyCollection.GetKeyIndex(PropertyName##String())); \
		checkf(PropertyName##Index != INDEX_NONE, TEXT("The default value getter that sets the property index must be called once prior to calling this function.")); \
		return PropertyCollection.GetHighValue<Type>(PropertyName##Index); \
	} \
	TPair<Type, Type> GetWeighted##PropertyName(const FCollectionPropertyConstFacade& PropertyCollection) const \
	{ \
		checkSlow(PropertyName##Index == PropertyCollection.GetKeyIndex(PropertyName##String())); \
		checkf(PropertyName##Index != INDEX_NONE, TEXT("The default value getter that sets the property index must be called once prior to calling this function.")); \
		return PropertyCollection.GetWeightedValue<Type>(PropertyName##Index); \
	} \
	FVector2f GetWeightedFloat##PropertyName(const FCollectionPropertyConstFacade& PropertyCollection) const \
	{ \
		checkSlow(PropertyName##Index == PropertyCollection.GetKeyIndex(PropertyName##String())); \
		checkf(PropertyName##Index != INDEX_NONE, TEXT("The default value getter that sets the property index must be called once prior to calling this function.")); \
		return PropertyCollection.GetWeightedFloatValue(PropertyName##Index); \
	} \
	Type Get##PropertyName(const FCollectionPropertyConstFacade& PropertyCollection) const \
	{ \
		checkSlow(PropertyName##Index == PropertyCollection.GetKeyIndex(PropertyName##String())); \
		checkf(PropertyName##Index != INDEX_NONE, TEXT("The default value getter that sets the property index must be called once prior to calling this function.")); \
		return PropertyCollection.GetValue<Type>(PropertyName##Index); \
	} \
	uint8 Get##PropertyName##Flags(const FCollectionPropertyConstFacade& PropertyCollection) const \
	{ \
		checkSlow(PropertyName##Index == PropertyCollection.GetKeyIndex(PropertyName##String())); \
		checkf(PropertyName##Index != INDEX_NONE, TEXT("The default value getter that sets the property index must be called once prior to calling this function.")); \
		return PropertyCollection.GetFlags(PropertyName##Index); \
	} \
	bool Is##PropertyName##Enabled(const FCollectionPropertyConstFacade& PropertyCollection) const \
	{ \
		checkSlow(PropertyName##Index == PropertyCollection.GetKeyIndex(PropertyName##String())); \
		checkf(PropertyName##Index != INDEX_NONE, TEXT("The default value getter that sets the property index must be called once prior to calling this function.")); \
		return PropertyCollection.IsEnabled(PropertyName##Index); \
	} \
	bool Is##PropertyName##Animatable(const FCollectionPropertyConstFacade& PropertyCollection) const\
	{ \
		checkSlow(PropertyName##Index == PropertyCollection.GetKeyIndex(PropertyName##String())); \
		checkf(PropertyName##Index != INDEX_NONE, TEXT("The default value getter that sets the property index must be called once prior to calling this function.")); \
		return PropertyCollection.IsAnimatable(PropertyName##Index); \
	} \
	bool Is##PropertyName##Dirty(const FCollectionPropertyConstFacade& PropertyCollection) const \
	{ \
		checkSlow(PropertyName##Index == PropertyCollection.GetKeyIndex(PropertyName##String())); \
		checkf(PropertyName##Index != INDEX_NONE, TEXT("The default value getter that sets the property index must be called once prior to calling this function.")); \
		return PropertyCollection.IsDirty(PropertyName##Index); \
	} \
	bool Is##PropertyName##Mutable(const FCollectionPropertyConstFacade& PropertyCollection) const \
	{ \
		return PropertyName##Index != INDEX_NONE && \
			EnumHasAllFlags((ECollectionPropertyFlag)PropertyCollection.GetFlags(PropertyName##Index), ECollectionPropertyFlag::Enabled | ECollectionPropertyFlag::Animatable | ECollectionPropertyFlag::Dirty); \
	} \
	int32 PropertyName##Index = INDEX_NONE;
