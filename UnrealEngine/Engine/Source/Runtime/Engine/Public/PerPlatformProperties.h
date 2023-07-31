// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
PerPlatformProperties.h: Property types that can be overridden on a per-platform basis at cook time
=============================================================================*/

#pragma once

#include "Engine/Engine.h"
#include "Serialization/Archive.h"
#include "RHIDefinitions.h"
#include "Containers/Map.h"
#include "Algo/Find.h"
#include "Serialization/MemoryLayout.h"
#include "Misc/DataDrivenPlatformInfoRegistry.h"

#if WITH_EDITORONLY_DATA && WITH_EDITOR
#include "Engine/Engine.h"
#endif

#include "PerPlatformProperties.generated.h"

namespace PerPlatformProperty::Private
{
	template<typename MapType>
	struct TGetKeyType
	{
		using Type = typename TTupleElement<0, typename MapType::ElementType>::Type;
	};

	template<typename NameType>
	struct FNameFuncs;

	template<>
	struct FNameFuncs<FName>
	{
		static FName NameToKey(FName Name) { return Name; }

		template<typename ValueType>
		static void SerializePerPlatformMap(FArchive& Ar, TMap<FName, ValueType>& Map)
		{
			Ar << Map;
		}

		template<typename ValueType>
		static void SerializePerPlatformMap(FArchive& UnderlyingArchive, FStructuredArchive::FRecord& Record, TMap<FName, ValueType>& Map)
		{
			Record << SA_VALUE(TEXT("PerPlatform"), Map);
		}
	};

	template<>
	struct FNameFuncs<FMemoryImageName>
	{
		static FMemoryImageName NameToKey(FName Name) { return FMemoryImageName(Name); }

		template<typename ValueType>
		static void SerializePerPlatformMap(FArchive& Ar, TMemoryImageMap<FMemoryImageName, ValueType>& Map)
		{
			if( Ar.IsLoading())
			{
				TMemoryImageMap<FName, ValueType> TempMap;
				Ar << TempMap;
				Map.Reset();
				for( TPair<FName, ValueType>& Pair : TempMap)
				{
					Map.Add(FMemoryImageName(Pair.Key), Pair.Value);
				}
			}
			else
			{
				TMemoryImageMap<FName, ValueType> TempMap;
				for (TPair<FMemoryImageName, ValueType>& Pair : Map)
				{
					TempMap.Add(FName(Pair.Key), Pair.Value);
				}
				Ar << TempMap;
			}
		}

		template<typename ValueType>
		static void SerializePerPlatformMap(FArchive& UnderlyingArchive, FStructuredArchive::FRecord& Record, TMemoryImageMap<FMemoryImageName, ValueType>& Map)
		{
			if (UnderlyingArchive.IsLoading())
			{
				TMemoryImageMap<FName, ValueType> TempMap;

				Record << SA_VALUE(TEXT("PerPlatform"), TempMap);
				Map.Reset();
				for (TPair<FName, ValueType>& Pair : TempMap)
				{
					Map.Add(FMemoryImageName(Pair.Key), Pair.Value);
				}
			}
			else
			{
				TMemoryImageMap<FName, ValueType> TempMap;
				for (TPair<FMemoryImageName, ValueType>& Pair : Map)
				{
					TempMap.Add(FName(Pair.Key), Pair.Value);
				}
				Record << SA_VALUE(TEXT("PerPlatform"), TempMap);
			}
		}
	};

	template<typename MapType>
	using KeyFuncs = FNameFuncs<typename TGetKeyType<MapType>::Type>;
}

/** TPerPlatformProperty - template parent class for per-platform properties 
 *  Implements Serialize function to replace value at cook time, and 
 *  backwards-compatible loading code for properties converted from simple types.
 */
template<typename _StructType, typename _ValueType, EName _BasePropertyName>
struct ENGINE_API TPerPlatformProperty
{
	typedef _ValueType ValueType;
	typedef _StructType StructType;

#if WITH_EDITOR
	/** Get the value for the given platform (using standard "ini" name, so Windows, not Win64 or WindowsClient), which can be used to lookup the group */
	_ValueType GetValueForPlatform(FName PlatformName) const
	{
		const _StructType* This = StaticCast<const _StructType*>(this);

		using MapType = decltype(This->PerPlatform);
		using KeyFuncs = typename PerPlatformProperty::Private::KeyFuncs<MapType>;

		const _ValueType* Ptr = This->PerPlatform.Find(KeyFuncs::NameToKey(PlatformName));

		if (Ptr == nullptr)
		{
			const FDataDrivenPlatformInfo& Info = FDataDrivenPlatformInfoRegistry::GetPlatformInfo(PlatformName);
			if (Info.PlatformGroupName != NAME_None)
			{
				Ptr = This->PerPlatform.Find(KeyFuncs::NameToKey(Info.PlatformGroupName));
			}
		}

		return Ptr ? *Ptr : This->Default;
	}

	/* Return the value */
	UE_DEPRECATED(5.0, "GetValueForPlatform should now be used, with a single platform name")
		_ValueType GetValueForPlatformIdentifiers(FName PlatformGroupName, FName VanillaPlatformName = NAME_None) const
	{
		const _StructType* This = StaticCast<const _StructType*>(this);

		using MapType = decltype(This->PerPlatform);
		using KeyFuncs = typename PerPlatformProperty::Private::KeyFuncs<MapType>;
		
		const _ValueType* ValuePtr = [This, VanillaPlatformName, PlatformGroupName]() -> const _ValueType*
		{
			const _ValueType* Ptr = nullptr;
			if (VanillaPlatformName != NAME_None)
			{
				TArray<FName> Keys;
				This->PerPlatform.GetKeys(Keys);
				const FName* MatchedName = Keys.FindByPredicate([VanillaPlatformName](FName& Name)
				{
					return VanillaPlatformName.ToString().Contains(Name.ToString());
				});
				Ptr = MatchedName ? This->PerPlatform.Find(KeyFuncs::NameToKey(*MatchedName)) : nullptr;
			}			
			if (Ptr == nullptr && PlatformGroupName != NAME_None)
			{				
				Ptr = This->PerPlatform.Find(KeyFuncs::NameToKey(PlatformGroupName));
			}
			return Ptr;			
		}();

		return ValuePtr != nullptr ? *ValuePtr : This->Default;
	}

	UE_DEPRECATED(4.22, "GetValueForPlatformGroup renamed GetValueForPlatformIdentifiers")
	_ValueType GetValueForPlatformGroup(FName PlatformGroupName) const
	{
		return GetValueForPlatformIdentifiers(PlatformGroupName);
	}

#endif

	_ValueType GetDefault() const
	{
		const _StructType* This = StaticCast<const _StructType*>(this);
		return This->Default;
	}
	
	_ValueType GetValue() const
	{
#if WITH_EDITORONLY_DATA && WITH_EDITOR
		FName PlatformName;
		// Lookup the override preview platform info, if any
		// @todo this doesn't set PlatformName, just a group, but GetValueForPlatform() will technically work being given a Group name instead of a platform name, so we just use it
		if (GEngine && GEngine->GetPreviewPlatformName(PlatformName))
		{
			return GetValueForPlatform(PlatformName);
		}
		else		
#endif
		{
			const _StructType* This = StaticCast<const _StructType*>(this);
			return This->Default;
		}
	}

	UE_DEPRECATED(4.26, "GetValueForFeatureLevel is not needed for platform previewing and GetValue() should be used instead.")
	_ValueType GetValueForFeatureLevel(ERHIFeatureLevel::Type FeatureLevel) const
	{
#if WITH_EDITORONLY_DATA
		FName PlatformGroupName;
		switch (FeatureLevel)
		{
		    case ERHIFeatureLevel::ES3_1:
		    {
			    PlatformGroupName = NAME_Mobile;
			    break;
		    }
		    default:
			    PlatformGroupName = NAME_None;
			    break;
		}
		return GetValueForPlatformIdentifiers(PlatformGroupName);
#else
		const _StructType* This = StaticCast<const _StructType*>(this);
		return This->Default;
#endif
	}

	/* Load old properties that have been converted to FPerPlatformX */
	bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FArchive& Ar)
	{
		if (Tag.Type == _BasePropertyName)
		{
			_StructType* This = StaticCast<_StructType*>(this);
			_ValueType OldValue;
			Ar << OldValue;
			*This = _StructType(OldValue);
			return true;
		}
		return false;
	}

	/* Serialization */
	bool Serialize(FArchive& Ar)
	{
		Ar << *this;
		return true;
	}

	/* Serialization */
	bool Serialize(FStructuredArchive::FSlot Slot)
	{
		Slot << *this;
		return true;
	}
};

template<typename _StructType, typename _ValueType, EName _BasePropertyName>
ENGINE_API FArchive& operator<<(FArchive& Ar, TPerPlatformProperty<_StructType, _ValueType, _BasePropertyName>& P);
template<typename _StructType, typename _ValueType, EName _BasePropertyName>
ENGINE_API void operator<<(FStructuredArchive::FSlot Slot, TPerPlatformProperty<_StructType, _ValueType, _BasePropertyName>& P);

struct FFreezablePerPlatformInt;

/** FPerPlatformInt - int32 property with per-platform overrides */
USTRUCT()
struct ENGINE_API FPerPlatformInt
#if CPP
:	public TPerPlatformProperty<FPerPlatformInt, int32, NAME_IntProperty>
#endif
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = PerPlatform)
	int32 Default;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = PerPlatform)
	TMap<FName, int32> PerPlatform;
#endif

	FPerPlatformInt()
	:	Default(0)
	{
	}

	FPerPlatformInt(int32 InDefaultValue)
	:	Default(InDefaultValue)
	{
	}

	FString ToString() const;

	FPerPlatformInt(const FFreezablePerPlatformInt& Other);

};

USTRUCT()
struct ENGINE_API FFreezablePerPlatformInt
#if CPP
	: public TPerPlatformProperty<FFreezablePerPlatformInt, int32, NAME_IntProperty>
#endif
{
	DECLARE_TYPE_LAYOUT(FFreezablePerPlatformInt, NonVirtual);

	GENERATED_USTRUCT_BODY()

public:
	using KeyType = FMemoryImageName;
	using FPerPlatformMap = TMemoryImageMap<FMemoryImageName, int32>;

	LAYOUT_FIELD(int32, Default);
	LAYOUT_FIELD_EDITORONLY(FPerPlatformMap, PerPlatform);

	FFreezablePerPlatformInt() : Default(0) {}
	FFreezablePerPlatformInt(int32 InDefaultValue) : Default(InDefaultValue) {}
	FFreezablePerPlatformInt(const FPerPlatformInt& Other)
		: Default(Other.Default)
	{
#if WITH_EDITORONLY_DATA
		for (const TPair<FName, int32>& Pair : Other.PerPlatform)
		{
			PerPlatform.Add(FMemoryImageName(Pair.Key), Pair.Value);
		}
#endif
	}

	FString ToString() const;
};

inline FPerPlatformInt::FPerPlatformInt(const FFreezablePerPlatformInt& Other)
	: Default(Other.Default)
{
#if WITH_EDITORONLY_DATA
	for (const TPair<FMemoryImageName, int32>& Pair : Other.PerPlatform)
	{
		PerPlatform.Add(FName(Pair.Key), Pair.Value);
	}
#endif
}

extern template ENGINE_API FArchive& operator<<(FArchive&, TPerPlatformProperty<FPerPlatformInt, int32, NAME_IntProperty>&);
extern template ENGINE_API void operator<<(FStructuredArchive::FSlot Slot, TPerPlatformProperty<FPerPlatformInt, int32, NAME_IntProperty>&);

template<>
struct TStructOpsTypeTraits<FPerPlatformInt>
	: public TStructOpsTypeTraitsBase2<FPerPlatformInt>
{
	enum
	{
		WithSerializeFromMismatchedTag = true,
		WithSerializer = true
	};
};

struct FFreezablePerPlatformFloat;

/** FPerPlatformFloat - float property with per-platform overrides */
USTRUCT(meta = (CanFlattenStruct))
struct ENGINE_API FPerPlatformFloat
#if CPP
:	public TPerPlatformProperty<FPerPlatformFloat, float, NAME_FloatProperty>
#endif
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = PerPlatform)
	float Default;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = PerPlatform)
	TMap<FName, float> PerPlatform;
#endif

	FPerPlatformFloat()
	:	Default(0.f)
	{
	}

	FPerPlatformFloat(float InDefaultValue)
	:	Default(InDefaultValue)
	{
	}

	FPerPlatformFloat(const FFreezablePerPlatformFloat& Other);
};
extern template ENGINE_API FArchive& operator<<(FArchive&, TPerPlatformProperty<FPerPlatformFloat, float, NAME_FloatProperty>&);

struct ENGINE_API FFreezablePerPlatformFloat
#if CPP
	: public TPerPlatformProperty<FFreezablePerPlatformFloat, float, NAME_FloatProperty>
#endif
{
	DECLARE_TYPE_LAYOUT(FFreezablePerPlatformFloat, NonVirtual);
public:
	using FPerPlatformMap = TMemoryImageMap<FMemoryImageName, float>;
	
	LAYOUT_FIELD(float, Default);
	LAYOUT_FIELD_EDITORONLY(FPerPlatformMap, PerPlatform);

	FFreezablePerPlatformFloat() : Default(0.0f) {}
	FFreezablePerPlatformFloat(float InDefaultValue) : Default(InDefaultValue) {}
	FFreezablePerPlatformFloat(const FPerPlatformFloat& Other)
		: Default(Other.Default)
	{
#if WITH_EDITORONLY_DATA
		for (const TPair<FName, float>& Pair : Other.PerPlatform)
		{
			PerPlatform.Add(FMemoryImageName(Pair.Key), Pair.Value);
		}
#endif
}
};

inline FPerPlatformFloat::FPerPlatformFloat(const FFreezablePerPlatformFloat& Other)
	: Default(Other.Default)
{
#if WITH_EDITORONLY_DATA
	for (const TPair<FMemoryImageName, float>& Pair : Other.PerPlatform)
	{
		PerPlatform.Add(FName(Pair.Key), Pair.Value);
	}
#endif
}

template<>
struct TStructOpsTypeTraits<FPerPlatformFloat>
:	public TStructOpsTypeTraitsBase2<FPerPlatformFloat>
{
	enum
	{
		WithSerializeFromMismatchedTag = true,
		WithSerializer = true
	};
};

/** FPerPlatformBool - bool property with per-platform overrides */
USTRUCT()
struct ENGINE_API FPerPlatformBool
#if CPP
:	public TPerPlatformProperty<FPerPlatformBool, bool, NAME_BoolProperty>
#endif
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = PerPlatform)
	bool Default;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = PerPlatform)
	TMap<FName, bool> PerPlatform;
#endif

	FPerPlatformBool()
	:	Default(false)
	{
	}

	FPerPlatformBool(bool InDefaultValue)
	:	Default(InDefaultValue)
	{
	}
};
extern template ENGINE_API FArchive& operator<<(FArchive&, TPerPlatformProperty<FPerPlatformBool, bool, NAME_BoolProperty>&);

template<>
struct TStructOpsTypeTraits<FPerPlatformBool>
	: public TStructOpsTypeTraitsBase2<FPerPlatformBool>
{
	enum
	{
		WithSerializeFromMismatchedTag = true,
		WithSerializer = true
	};
};