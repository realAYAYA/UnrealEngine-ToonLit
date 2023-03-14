// Copyright Epic Games, Inc. All Rights Reserved.
#include "PerQualityLevelProperties.h"
#include "Serialization/Archive.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DataDrivenPlatformInfoRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PerQualityLevelProperties)

#if WITH_EDITOR
#include "Interfaces/ITargetPlatform.h"
#include "PlatformInfo.h"
#endif


namespace QualityLevelProperty
{
	static TArray<FName> QualityLevelNames = { FName("Low"), FName("Medium"), FName("High"), FName("Epic"), FName("Cinematic") };
	static FString QualityLevelMappingStr = TEXT("QualityLevelMapping");

	TMap<int32, int32> ConvertQualtiyLevelData(const TMap<EPerQualityLevels, int32>& Data)
	{
		TMap<int32, int32> ConvertedData;

		for (const TPair<EPerQualityLevels, int32>& Pair : Data)
		{
			ConvertedData.Add((int32)Pair.Key,Pair.Value);
		}

		return ConvertedData;
	}

	TMap<EPerQualityLevels, int32> ConvertQualtiyLevelData(const TMap<int32, int32>& Data)
	{
		TMap<EPerQualityLevels, int32> ConvertedData;

		for (const TPair<int32, int32>& Pair : Data)
		{
			ConvertedData.Add((EPerQualityLevels)Pair.Key, Pair.Value);
		}

		return ConvertedData;
	}

	FName QualityLevelToFName(int32 QL)
	{
		if (QL >= 0 && QL < static_cast<int32>(EPerQualityLevels::Num))
		{
			return QualityLevelNames[QL];
		}
		else
		{
			return NAME_None;
		}
		return QualityLevelNames[QL];
	}

	int32 FNameToQualityLevel(FName QL)
	{
		return QualityLevelNames.IndexOfByKey(QL);
	}

#if WITH_EDITOR
	static TMap<FString, FSupportedQualityLevelArray> CachedPerPlatformToQualityLevels;
	static FCriticalSection MappingCriticalSection;

	FSupportedQualityLevelArray PerPlatformOverrideMapping(FString& InPlatformName)
	{
		FSupportedQualityLevelArray* CachedMappingQualitLevelInfo = nullptr;
		{
			FScopeLock ScopeLock(&MappingCriticalSection);
			CachedMappingQualitLevelInfo = CachedPerPlatformToQualityLevels.Find(InPlatformName);
			if (CachedMappingQualitLevelInfo)
			{
				return *CachedMappingQualitLevelInfo;
			}
		}

		// Platform (group) names
		const TArray<FName>& PlatformGroupNameArray = PlatformInfo::GetAllPlatformGroupNames();
		TArray<FName> EnginePlatforms;

		bool bIsPlatformGroup = PlatformGroupNameArray.Contains(FName(*InPlatformName));
		if (bIsPlatformGroup)
		{
			// get all the platforms from that group
			for (const FDataDrivenPlatformInfo* DataDrivenPlatformInfo : FDataDrivenPlatformInfoRegistry::GetSortedPlatformInfos(EPlatformInfoType::TruePlatformsOnly))
			{
				// gather all platform related to the platform group
				if (DataDrivenPlatformInfo->PlatformGroupName == FName(*InPlatformName))
				{
					EnginePlatforms.AddUnique(DataDrivenPlatformInfo->IniPlatformName);
				}
			}
		}
		else
		{
			FName PlatformFName = FDataDrivenPlatformInfoRegistry::GetPlatformInfo(FName(InPlatformName)).IniPlatformName;
			if (!PlatformFName.IsNone())
			{
				InPlatformName = PlatformFName.ToString();
			}

			EnginePlatforms.AddUnique(FName(*InPlatformName));
		}

		FSupportedQualityLevelArray QualityLevels;

		for (const FName& EnginePlatformName : EnginePlatforms)
		{
			//load individual platform ini files
			FConfigFile EngineSettings;
			 FConfigCacheIni::LoadLocalIniFile(EngineSettings, TEXT("Engine"), true, *EnginePlatformName.ToString());

			FString MappingStr;
			if (EngineSettings.GetString(TEXT("SystemSettings"), *QualityLevelMappingStr, MappingStr))
			{
				int32 Value = FNameToQualityLevel(FName(*MappingStr));
				if (Value == INDEX_NONE)
				{
					UE_LOG(LogCore, Warning, TEXT("Bad QualityLevelMapping input value. Need to be either [low,medium,high,epic,cinematic]"));
					continue;
				}		
				QualityLevels.Add(Value);
			}
			else
			{
				UE_LOG(LogCore, Warning, TEXT("Didnt found QualityLevelMapping in the %sEngine.ini. Need to define QualityLevelMapping under the [SystemSettings] section. All perplatform MinLOD will not be converted to PerQuality"), *EnginePlatformName.ToString());
			}
		}

		// Cache the Scalability setting for this platform
		{
			FScopeLock ScopeLock(&MappingCriticalSection);
			CachedMappingQualitLevelInfo = &CachedPerPlatformToQualityLevels.Add(InPlatformName, QualityLevels);
			return *CachedMappingQualitLevelInfo;
		}
	}
#endif
}

#if WITH_EDITOR
static TMap<FString, FSupportedQualityLevelArray> GSupportedQualityLevels;
static FCriticalSection GCookCriticalSection;


template<typename _StructType, typename _ValueType, EName _BasePropertyName>
int32 FPerQualityLevelProperty<_StructType, _ValueType, _BasePropertyName>::GetValueForPlatform(const ITargetPlatform* TargetPlatform) const
{
	const _StructType* This = StaticCast<const _StructType*>(this);
	// get all supported quality level from scalability + engine ini files
	FSupportedQualityLevelArray SupportedQualityLevels = GetSupportedQualityLevels(*TargetPlatform->GetPlatformInfo().IniPlatformName.ToString());

	// loop through all the supported quality level to find the min lod index
	int32 MinLodIdx = MAX_int32;
	for (int32& QL : SupportedQualityLevels)
	{
		// check if have data for the supported quality level
		if (IsQualityLevelValid(QL))
		{
			MinLodIdx = FMath::Min(GetValueForQualityLevel(QL), MinLodIdx);
		}
	}

	if (MinLodIdx == MAX_int32)
	{
		MinLodIdx = This->Default;
	}

	return MinLodIdx;
}

template<typename _StructType, typename _ValueType, EName _BasePropertyName>
FSupportedQualityLevelArray FPerQualityLevelProperty<_StructType, _ValueType, _BasePropertyName>::GetSupportedQualityLevels(const TCHAR* InPlatformName) const
{
	const FString PlatformNameStr = FDataDrivenPlatformInfoRegistry::GetPlatformInfo(FName(InPlatformName)).IniPlatformName.ToString();
	InPlatformName = *PlatformNameStr;

	FSupportedQualityLevelArray* CachedCookingQualitLevelInfo = nullptr;
	
	{
		FScopeLock ScopeLock(&GCookCriticalSection);
		CachedCookingQualitLevelInfo = GSupportedQualityLevels.Find(InPlatformName);
		if (CachedCookingQualitLevelInfo)
		{
			return *CachedCookingQualitLevelInfo;
		}
	}

	FSupportedQualityLevelArray CookingQualitLevelInfo;

	// check the Engine file
	FConfigFile EngineSettings;
	FConfigCacheIni::LoadLocalIniFile(EngineSettings, TEXT("Engine"), true, InPlatformName);

	int32 PropertyQualityLevel = -1;
	if (EngineSettings.GetInt(TEXT("SystemSettings"), *CVarName, PropertyQualityLevel))
	{
		CookingQualitLevelInfo.Add(PropertyQualityLevel);
	}

	// Load the scalability platform file
	FConfigFile ScalabilitySettings;
	FConfigCacheIni::LoadLocalIniFile(ScalabilitySettings, TEXT("Scalability"), true, InPlatformName);

	//check all possible quality levels specify in the scalability ini 
	for (int32 QualityLevel = 0; QualityLevel < (int32)EPerQualityLevels::Num; ++QualityLevel)
	{
		FString QualitLevelSectionName = Scalability::GetScalabilitySectionString(*ScalabilitySection, QualityLevel, (int32)EPerQualityLevels::Num);
		PropertyQualityLevel = -1;
		ScalabilitySettings.GetInt(*QualitLevelSectionName, *CVarName, PropertyQualityLevel);

		// add supported quality level to the property map
		if (PropertyQualityLevel != -1)
		{
			CookingQualitLevelInfo.Add(PropertyQualityLevel);
		}
	}

	// Cache the Scalability setting for this platform
	{
		FScopeLock ScopeLock(&GCookCriticalSection);
		CachedCookingQualitLevelInfo = &GSupportedQualityLevels.Add(FString(InPlatformName), CookingQualitLevelInfo);
		return *CachedCookingQualitLevelInfo;
	}
}

template<typename _StructType, typename _ValueType, EName _BasePropertyName>
void FPerQualityLevelProperty<_StructType, _ValueType, _BasePropertyName>::StripQualtiyLevelForCooking(const TCHAR* InPlatformName)
{
	_StructType* This = StaticCast<_StructType*>(this);
	if (This->PerQuality.Num() > 0)
	{
		FSupportedQualityLevelArray CookQualityLevelInfo = This->GetSupportedQualityLevels(InPlatformName);
		CookQualityLevelInfo.Sort([&](const int32& A, const int32& B) { return (A > B); });

		// remove unsupported quality levels 
		for (TMap<int32, int32>::TIterator It(This->PerQuality); It; ++It)
		{
			if (!CookQualityLevelInfo.Contains(It.Key()))
			{
				It.RemoveCurrent();
			}
		}

		if (This->PerQuality.Num() > 0)
		{
			int32 PreviousQualityLevel = This->Default;

			for (TSet<int32>::TIterator It(CookQualityLevelInfo); It; ++It)
			{
				int32* QualityLevelMinLod = This->PerQuality.Find(*It);
			
				// add quality level supported by the platform
				if (!QualityLevelMinLod)
				{
					This->PerQuality.Add(*It, PreviousQualityLevel);
				}
				else
				{
					PreviousQualityLevel = *QualityLevelMinLod;
				}
			}

			This->Default = PreviousQualityLevel;
		}
	}
}

template<typename _StructType, typename _ValueType, EName _BasePropertyName>
bool FPerQualityLevelProperty<_StructType, _ValueType, _BasePropertyName>::IsQualityLevelValid(int32 QualityLevel) const
{
	const _StructType* This = StaticCast<const _StructType*>(this);
	int32* Value = (int32*)This->PerQuality.Find(QualityLevel);

	if (Value != nullptr)
	{
		return true;
	}
	else
	{
		return false;
	}
}
#endif

template<typename _StructType, typename _ValueType, EName _BasePropertyName>
_ValueType FPerQualityLevelProperty<_StructType, _ValueType, _BasePropertyName>::GetLowestValue() const
{
	const _StructType* This = StaticCast<const _StructType*>(this);
	_ValueType Value = This->Default;

	for (const TPair<int32, int32>& Pair : This->PerQuality)
	{
		if (Pair.Value < Value)
		{
			Value = Pair.Value;
		}
	}
	return Value;
}

/** Serializer to cook out the most appropriate platform override */
template<typename _StructType, typename _ValueType, EName _BasePropertyName>
ENGINE_API FArchive& operator<<(FArchive& Ar, FPerQualityLevelProperty<_StructType, _ValueType, _BasePropertyName>& Property)
{
	bool bCooked = false;
	_StructType* This = StaticCast<_StructType*>(&Property);

#if WITH_EDITOR
	if (Ar.IsCooking())
	{
		bCooked = true;
		const FDataDrivenPlatformInfo& PlatformInfo = Ar.CookingTarget()->GetPlatformInfo();
		This->StripQualtiyLevelForCooking(*(PlatformInfo.IniPlatformName.ToString()));
	}
#endif
	{
		Ar << bCooked;
		Ar << This->Default;
		Ar << This->PerQuality;
	}
	return Ar;
}

/** Serializer to cook out the most appropriate platform override */
template<typename _StructType, typename _ValueType, EName _BasePropertyName>
ENGINE_API void operator<<(FStructuredArchive::FSlot Slot, FPerQualityLevelProperty<_StructType, _ValueType, _BasePropertyName>& Property)
{
	FArchive& UnderlyingArchive = Slot.GetUnderlyingArchive();
	FStructuredArchive::FRecord Record = Slot.EnterRecord();

	bool bCooked = false;
	_StructType* This = StaticCast<_StructType*>(&Property);

#if WITH_EDITOR
	if (UnderlyingArchive.IsCooking())
	{
		bCooked = true;
		This->StripQualtiyLevelForCooking(*(UnderlyingArchive.CookingTarget()->GetPlatformInfo().IniPlatformName.ToString()));
	}
#endif
	{
		Record << SA_VALUE(TEXT("bCooked"), bCooked);
		Record << SA_VALUE(TEXT("Value"), This->Default);
		Record << SA_VALUE(TEXT("PerQuality"), This->PerQuality);
	}
}
// 
template ENGINE_API FArchive& operator<<(FArchive&, FPerQualityLevelProperty<FPerQualityLevelInt, int32, NAME_IntProperty>&);
template ENGINE_API void operator<<(FStructuredArchive::FSlot Slot, FPerQualityLevelProperty<FPerQualityLevelInt, int32, NAME_IntProperty>&);

#if WITH_EDITOR
template int32 FPerQualityLevelProperty<FPerQualityLevelInt, int32, NAME_IntProperty>::GetValueForPlatform(const ITargetPlatform* TargetPlatform) const;
template FSupportedQualityLevelArray FPerQualityLevelProperty<FPerQualityLevelInt, int32, NAME_IntProperty>::GetSupportedQualityLevels(const TCHAR* InPlatformName) const;
template void FPerQualityLevelProperty<FPerQualityLevelInt, int32, NAME_IntProperty>::StripQualtiyLevelForCooking(const TCHAR* InPlatformName);
template bool FPerQualityLevelProperty<FPerQualityLevelInt, int32, NAME_IntProperty>::IsQualityLevelValid(int32 QualityLevel) const;
#endif
template int32 FPerQualityLevelProperty<FPerQualityLevelInt, int32, NAME_IntProperty>::GetLowestValue() const;

FString FPerQualityLevelInt::ToString() const
{
	FString Result = FString::FromInt(Default);

#if WITH_EDITORONLY_DATA
	TArray<int32> QualityLevels;
	PerQuality.GetKeys(QualityLevels);
	QualityLevels.Sort();

	for (int32 QL : QualityLevels)
	{
		Result = FString::Printf(TEXT("%s, %s=%d"), *Result, *QualityLevelProperty::QualityLevelToFName(QL).ToString(), PerQuality.FindChecked(QL));
	}
#endif

	return Result;
}

