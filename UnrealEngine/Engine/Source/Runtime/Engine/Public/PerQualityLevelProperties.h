// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
FPerQualityLevelProperties.h: Property types that can be overridden on a quality level basis at cook time
=============================================================================*/

#pragma once

#include "Serialization/Archive.h"
#include "Containers/Map.h"
#include "Algo/Find.h"
#include "Serialization/MemoryLayout.h"
#include "Scalability.h"
#include "CoreMinimal.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Engine/Engine.h"
#include "HAL/ConsoleManager.h"
#endif

#include "PerQualityLevelProperties.generated.h"

#if WITH_EDITOR
	typedef TSet<int32> FSupportedQualityLevelArray;
#endif


UENUM(BlueprintType)
enum class EPerQualityLevels : uint8
{
	Low,
	Medium,
	High,
	Epic,
	Cinematic,
	Num
};

namespace QualityLevelProperty
{
	UENUM()
	enum class UE_DEPRECATED(5.1, "Use EPerQualityLevels instead since we need to expose as an ENUM in blueprint.") EQualityLevels : uint8
	{
		Low,
		Medium,
		High,
		Epic,
		Cinematic,
		Num
	};

	ENGINE_API FName QualityLevelToFName(int32 QL);
	ENGINE_API int32 FNameToQualityLevel(FName QL);
	template<typename _ValueType>
	ENGINE_API TMap<int32, _ValueType> ConvertQualtiyLevelData(const TMap<EPerQualityLevels, _ValueType>& Data);
	template<typename _ValueType>
	ENGINE_API TMap<EPerQualityLevels, _ValueType> ConvertQualtiyLevelData(const TMap<int32, _ValueType>& Data);
	
#if WITH_EDITOR
	ENGINE_API FSupportedQualityLevelArray PerPlatformOverrideMapping(FString& InPlatformName);
#endif
};

template<typename _StructType, typename _ValueType, EName _BasePropertyName>
struct FPerQualityLevelProperty
{
	typedef _ValueType ValueType;
	typedef _StructType StructType;

	FPerQualityLevelProperty() 
	{
	}
	~FPerQualityLevelProperty() {}

	_ValueType GetValueForQualityLevel(int32 QualityLevel) const
	{
		const _StructType* This = StaticCast<const _StructType*>(this);
		if (This->PerQuality.Num() == 0 || QualityLevel < 0)
		{
			return This->Default;
		}

		_ValueType* Value = (_ValueType*)This->PerQuality.Find(QualityLevel);
		
		if (Value)
		{
			return *Value;
		}
		else
		{
			return This->Default;
		}
	}

#if WITH_EDITOR
	int32 GetValueForPlatform(const ITargetPlatform* TargetPlatform) const;
	FSupportedQualityLevelArray GetSupportedQualityLevels(const TCHAR* InPlatformName = nullptr) const;
	void StripQualtiyLevelForCooking(const TCHAR* InPlatformName = nullptr);
	bool IsQualityLevelValid(int32 QualityLevel) const;
	void ConvertQualtiyLevelData(TMap<FName, _ValueType>& PlaformData, TMultiMap<FName, FName>& PerPlatformToQualityLevel, _ValueType Default);
#endif

	// Set Cvar to be able to scan ini files at cook-time and only have the supported ranges of quality levels relevant to the platform.
	// Unsupported quality levels will be stripped.
	void SetQualityLevelCVarForCooking(const TCHAR* InCVarName, const TCHAR* InSection)
	{
#if WITH_EDITOR
		ScalabilitySection = FString(InSection);
#endif
		CVarName = FString(InCVarName);
	}

	UE_DEPRECATED(5.4, "If no cvar is associated with the property, all quality levels will be keept when cooking. Call SetQualtiyLevelCVarForCooking to strip unsupported quality levels when cooking")
	void Init(const TCHAR* InCVarName, const TCHAR* InSection)
	{
		SetQualityLevelCVarForCooking(InCVarName, InSection);
	}

	_ValueType GetDefault() const
	{
		const _StructType* This = StaticCast<const _StructType*>(this);
		return This->Default;
	}

	_ValueType GetValue(int32 QualityLevel) const
	{
		return GetValueForQualityLevel(QualityLevel);
	}

	_ValueType GetLowestValue() const
	{
		const _StructType* This = StaticCast<const _StructType*>(this);
		_ValueType Value = This->Default;

		for (const TPair<int32, _ValueType>& Pair : This->PerQuality)
		{
			if (Pair.Value < Value)
			{
				Value = Pair.Value;
			}
		}
		return Value;
	}

	/* Load old properties that have been converted to FPerQualityLevel */
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

#if WITH_EDITOR
	FString ScalabilitySection;
#endif
	FString CVarName;
};

template< typename _StructType, typename _ValueType, EName _BasePropertyName>
ENGINE_API FArchive& operator<<(FArchive& Ar, FPerQualityLevelProperty<_StructType, _ValueType, _BasePropertyName>& Property);

template<typename _StructType, typename _ValueType, EName _BasePropertyName>
ENGINE_API void operator<<(FStructuredArchive::FSlot Slot, FPerQualityLevelProperty<_StructType, _ValueType, _BasePropertyName>& Property);

USTRUCT(BlueprintType)
struct FPerQualityLevelInt 
#if CPP
	:	public FPerQualityLevelProperty<FPerQualityLevelInt, int32, NAME_IntProperty>
#endif
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = PerQualityLevel)
	int32 Default;

	UPROPERTY(EditAnywhere, Category = PerQualityLevel)
	TMap<int32, int32> PerQuality;

	FPerQualityLevelInt()
	{
		Default = 0;
	}

	FPerQualityLevelInt(int32 InDefaultValue)
	{
		Default = InDefaultValue;
	}

	ENGINE_API FString ToString() const;
	int32 MaxType() const { return MAX_int32; }
};

extern template ENGINE_API FArchive& operator<<(FArchive&, FPerQualityLevelProperty<FPerQualityLevelInt, int32, NAME_IntProperty>&);
extern template ENGINE_API void operator<<(FStructuredArchive::FSlot Slot, FPerQualityLevelProperty<FPerQualityLevelInt, int32, NAME_IntProperty>&);

template<>
struct TStructOpsTypeTraits<FPerQualityLevelInt>
	: public TStructOpsTypeTraitsBase2<FPerQualityLevelInt>
{
	enum
	{
		WithSerializeFromMismatchedTag = true,
		WithSerializer = true
	};
	static constexpr EPropertyObjectReferenceType WithSerializerObjectReferences = EPropertyObjectReferenceType::None;
};

USTRUCT(BlueprintType)
struct FPerQualityLevelFloat
#if CPP
	:	public FPerQualityLevelProperty<FPerQualityLevelFloat, float, NAME_FloatProperty>
#endif
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = PerQualityLevel)
	float Default;

	UPROPERTY(EditAnywhere, Category = PerQualityLevel)
	TMap<int32, float> PerQuality;

	FPerQualityLevelFloat()
	{
		Default = 0.0f;
	}

	FPerQualityLevelFloat(float InDefaultValue)
	{
		Default = InDefaultValue;
	}

	ENGINE_API FString ToString() const;
	float MaxType() const { return UE_MAX_FLT; }
};

extern template ENGINE_API FArchive& operator<<(FArchive&, FPerQualityLevelProperty<FPerQualityLevelFloat, float, NAME_FloatProperty>&);
extern template ENGINE_API void operator<<(FStructuredArchive::FSlot Slot, FPerQualityLevelProperty<FPerQualityLevelFloat, float, NAME_FloatProperty>&);

template<>
struct TStructOpsTypeTraits<FPerQualityLevelFloat>
	: public TStructOpsTypeTraitsBase2<FPerQualityLevelFloat>
{
	enum
	{
		WithSerializeFromMismatchedTag = true,
		WithSerializer = true
	};
	static constexpr EPropertyObjectReferenceType WithSerializerObjectReferences = EPropertyObjectReferenceType::None;
};

