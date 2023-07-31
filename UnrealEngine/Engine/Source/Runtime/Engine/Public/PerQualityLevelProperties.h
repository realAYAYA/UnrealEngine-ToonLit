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
//#if WITH_EDITORONLY_DATA && WITH_EDITOR
#include "Engine/Engine.h"
#include "HAL/ConsoleManager.h"
//#endif

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
	ENGINE_API TMap<int32, int32> ConvertQualtiyLevelData(const TMap<EPerQualityLevels, int32>& Data);
	ENGINE_API TMap<EPerQualityLevels, int32> ConvertQualtiyLevelData(const TMap<int32, int32>& Data);
#if WITH_EDITOR
	ENGINE_API FSupportedQualityLevelArray PerPlatformOverrideMapping(FString& InPlatformName);
#endif
};

template<typename _StructType, typename _ValueType, EName _BasePropertyName>
struct ENGINE_API FPerQualityLevelProperty
{
	typedef _ValueType ValueType;

	FPerQualityLevelProperty() 
	{
	}
	~FPerQualityLevelProperty() {}

	bool operator==(const FPerQualityLevelProperty& Other) const;

	_ValueType GetValueForQualityLevel(int32 QualityLevel) const
	{
		const _StructType* This = StaticCast<const _StructType*>(this);
		if (This->PerQuality.Num() == 0 || QualityLevel < 0)
		{
			return This->Default;
		}

		int32* Value = (int32*)This->PerQuality.Find(QualityLevel);
		
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
#endif

	void Init(const TCHAR* InCVarName, const TCHAR* InSection)
	{
#if WITH_EDITOR
		ScalabilitySection = FString(InSection);
#endif
		CVarName = FString(InCVarName);
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

	_ValueType GetLowestValue() const;

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

USTRUCT()
struct ENGINE_API FPerQualityLevelInt 
#if CPP
	:	public FPerQualityLevelProperty<FPerQualityLevelInt, int32, NAME_IntProperty>
#endif
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = PerQualityLevel)
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

	FString ToString() const;
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
};