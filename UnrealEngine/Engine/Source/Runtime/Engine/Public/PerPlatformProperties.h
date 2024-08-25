// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
PerPlatformProperties.h: Property types that can be overridden on a per-platform basis at cook time
=============================================================================*/

#pragma once

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Engine/Engine.h"
#endif
#include "Serialization/Archive.h"
#include "RHIDefinitions.h"
#include "Containers/Map.h"
#include "Algo/Find.h"
#include "Serialization/MemoryLayout.h"
#include "Misc/DataDrivenPlatformInfoRegistry.h"
#include "Misc/FrameRate.h"

#include "PerPlatformProperties.generated.h"

/** Helper function to avoid including Engine.h in this header */
#if WITH_EDITORONLY_DATA && WITH_EDITOR
ENGINE_API bool GEngine_GetPreviewPlatformName(FName& PlatformName);
#endif

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
struct TPerPlatformProperty
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
		if (GEngine_GetPreviewPlatformName(PlatformName))
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
USTRUCT(BlueprintType)
struct FPerPlatformInt
#if CPP
:	public TPerPlatformProperty<FPerPlatformInt, int32, NAME_IntProperty>
#endif
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = PerPlatform)
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

	ENGINE_API FString ToString() const;

	ENGINE_API FPerPlatformInt(const FFreezablePerPlatformInt& Other);

};

USTRUCT()
struct FFreezablePerPlatformInt
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

	ENGINE_API FString ToString() const;
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
	static constexpr EPropertyObjectReferenceType WithSerializerObjectReferences = EPropertyObjectReferenceType::None;
};

struct FFreezablePerPlatformFloat;

/** FPerPlatformFloat - float property with per-platform overrides */
USTRUCT(BlueprintType, meta = (CanFlattenStruct))
struct FPerPlatformFloat
#if CPP
:	public TPerPlatformProperty<FPerPlatformFloat, float, NAME_FloatProperty>
#endif
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = PerPlatform)
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

	ENGINE_API FPerPlatformFloat(const FFreezablePerPlatformFloat& Other);
};
extern template ENGINE_API FArchive& operator<<(FArchive&, TPerPlatformProperty<FPerPlatformFloat, float, NAME_FloatProperty>&);

struct FFreezablePerPlatformFloat
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
	static constexpr EPropertyObjectReferenceType WithSerializerObjectReferences = EPropertyObjectReferenceType::None;
};

/** FPerPlatformBool - bool property with per-platform overrides */
USTRUCT()
struct FPerPlatformBool
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
	static constexpr EPropertyObjectReferenceType WithSerializerObjectReferences = EPropertyObjectReferenceType::None;
};

/** FPerPlatformFrameRate - FFrameRate property with per-platform overrides */
USTRUCT(BlueprintType)
struct FPerPlatformFrameRate
#if CPP
:	public TPerPlatformProperty<FPerPlatformFrameRate, FFrameRate, NAME_FrameRate>
#endif
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = PerPlatform)
	FFrameRate Default;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = PerPlatform)
	TMap<FName, FFrameRate> PerPlatform;
#endif

	FPerPlatformFrameRate()
	:	Default(30, 1)
	{
	}

	FPerPlatformFrameRate(FFrameRate InDefaultValue)
	:	Default(InDefaultValue)
	{
	}
	
	bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FArchive& Ar)
	{
		if(const UStruct* FrameRateStruct = FindObject<UStruct>(FTopLevelAssetPath("/Script/CoreUObject.FrameRate")))
		{
			FFrameRate Value;
			Ar << Value.Denominator;
			Ar << Value.Numerator;
			Default = Value;

			return true;
		}
		
		return false;
	}
};

extern template ENGINE_API FArchive& operator<<(FArchive&, TPerPlatformProperty<FPerPlatformFrameRate, FFrameRate, NAME_FrameRate>&);

template<>
struct TStructOpsTypeTraits<FPerPlatformFrameRate>
	: public TStructOpsTypeTraitsBase2<FPerPlatformFrameRate>
{
	enum
	{
		WithSerializeFromMismatchedTag = false,
		WithSerializer = true
	};
	static constexpr EPropertyObjectReferenceType WithSerializerObjectReferences = EPropertyObjectReferenceType::None;
};
