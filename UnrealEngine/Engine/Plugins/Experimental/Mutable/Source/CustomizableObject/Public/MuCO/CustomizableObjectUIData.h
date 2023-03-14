// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCO/UnrealPortabilityHelpers.h"
#include "UObject/ObjectMacros.h"
#include "MuCO/CustomizableObjectParameterTypeDefinitions.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "CustomizableObjectUIData.generated.h"

class UTexture2D;


USTRUCT(BlueprintType)
struct FMutableParamUIMetadata
{
	GENERATED_BODY()

	FMutableParamUIMetadata()
		: UIOrder(0)
	{}

	/** This is the name to be shown in UI */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = UI)
	FString ObjectFriendlyName;

	/** This is the name of the section where the parameter will be placed in UI */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = UI)
	FString UISectionName;

	/** This is the order of the parameter inside its section */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = UI)
	int32 UIOrder;

	/** Thumnbail for UI */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = UI)
	TSoftObjectPtr<UTexture2D> UIThumbnail;

	/** Extra information to be used in UI building, with semantics completely defined by the game/UI programmer, with a key to identify the semantic of its related value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = UI)
	TMap<FString, FString> ExtraInformation;

	/** Extra assets to be used in UI building */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = UI)
	TMap<FString, TSoftObjectPtr<UObject>> ExtraAssets;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = UI)
	float MinimumValue = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = UI)
	float MaximumValue = 1.0f;

	bool operator ==(const FMutableParamUIMetadata& Other) const
	{
		if (ObjectFriendlyName != Other.ObjectFriendlyName || UISectionName != Other.UISectionName || UIOrder != Other.UIOrder || UIThumbnail != Other.UIThumbnail)
		{
			return false;
		}

		if (!ExtraInformation.OrderIndependentCompareEqual(Other.ExtraInformation))
		{
			return false;
		}

		if (!ExtraAssets.OrderIndependentCompareEqual(Other.ExtraAssets))
		{
			return false;
		}

		return true;
	}

	bool operator !=(const FMutableParamUIMetadata& Other) const
	{
		return !(*this == Other);
	}

	friend FArchive& operator <<(FArchive& Ar, FMutableParamUIMetadata& Metadata)
	{
		Ar.UsingCustomVersion(FCustomizableObjectCustomVersion::GUID);

		Ar << Metadata.ObjectFriendlyName;
		Ar << Metadata.UISectionName;
		Ar << Metadata.UIOrder;

		//Ar << Metadata.UIThumbnail;
		if (Ar.IsLoading())
		{
			FString StringRef;
			Ar << StringRef;
			Metadata.UIThumbnail = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(StringRef));
		}
		else
		{
			FString StringRef = Metadata.UIThumbnail.ToSoftObjectPath().ToString();
			Ar << StringRef;
		}

		Ar << Metadata.ExtraInformation;

		//Ar << Metadata.ExtraAssets;
		if (Ar.IsLoading())
		{
			int32 NumReferencedAssets = 0;
			Ar << NumReferencedAssets;
			Metadata.ExtraAssets.Empty(NumReferencedAssets);

			for (int32 i = 0; i < NumReferencedAssets; ++i)
			{
				FString Key, StringRef;
				Ar << Key;
				Ar << StringRef;

				Metadata.ExtraAssets.Add(Key, TSoftObjectPtr<UObject>(FSoftObjectPath(StringRef)));
			}
		}
		else
		{
			int32 NumReferencedAssets = Metadata.ExtraAssets.Num();
			Ar << NumReferencedAssets;

			for (TPair<FString, TSoftObjectPtr<UObject>>& AssetPair : Metadata.ExtraAssets)
			{
				FString StringRef = AssetPair.Value.ToSoftObjectPath().ToString();
				Ar << AssetPair.Key;
				Ar << StringRef;
			}
		}

		// Update structure
		if (Ar.CustomVer(FCustomizableObjectCustomVersion::GUID) >= FCustomizableObjectCustomVersion::BeforeCustomVersionWasAdded)
		{
			Ar << Metadata.MinimumValue;
			Ar << Metadata.MaximumValue;
		}

		return Ar;
	}

#if WITH_EDITOR

	/** Only called in the BeginCacheForCookPlatform to include new references in the final package */
	void LoadResources()
	{
		UIThumbnail.LoadSynchronous();

		for (const TPair<FString, TSoftObjectPtr<UObject>>& a : ExtraAssets)
		{
			a.Value.LoadSynchronous();
		}
	}

#endif
};

USTRUCT(BlueprintType)
struct FIntegerParameterUIData
{
	GENERATED_BODY()

	FIntegerParameterUIData()
	{

	}

	FIntegerParameterUIData(
		const FString& ParamName,
		const FMutableParamUIMetadata& InParamFriendlyName) :
		  Name(ParamName)
		, ParamUIMetadata(InParamFriendlyName)
	{

	}

	/** Integer parameter option name */
	UPROPERTY(BlueprintReadWrite, Category = UI)
	FString Name;

	UPROPERTY(BlueprintReadWrite, Category = UI, meta = (DisplayName = "Parameter UI Metadata"))
	FMutableParamUIMetadata ParamUIMetadata;

	bool operator ==(const FIntegerParameterUIData& Other) const
	{
		if (Name != Other.Name || ParamUIMetadata != Other.ParamUIMetadata)
		{
			return false;
		}

		return true;
	}

	bool operator !=(const FIntegerParameterUIData& Other) const
	{
		return !(*this == Other);
	}

	friend FArchive& operator <<(FArchive& Ar, FIntegerParameterUIData& UIData)
	{
		Ar << UIData.Name;
		Ar << UIData.ParamUIMetadata;

		return Ar;
	}

#if WITH_EDITOR

	/** Only called in the BeginCacheForCookPlatform to include new references in the final package */
	void LoadResources()
	{
		ParamUIMetadata.LoadResources();
	}

#endif
};


USTRUCT(BlueprintType)
struct FParameterUIData
{
	GENERATED_BODY()

	FParameterUIData() 
		: Type(EMutableParameterType::None)
		, IntegerParameterGroupType(ECustomizableObjectGroupType::COGT_ONE_OR_NONE)
	{

	}

	FParameterUIData(
		const FString& ParamName,
		const FMutableParamUIMetadata& InParamUIMetadata,
		EMutableParameterType ParamType) :
		Name(ParamName),
		ParamUIMetadata(InParamUIMetadata),
		Type(ParamType), 
		IntegerParameterGroupType(ECustomizableObjectGroupType::COGT_ONE_OR_NONE)
	{

	}

	/** Parameter name */
	UPROPERTY(BlueprintReadWrite, Category = UI)
	FString Name;

	UPROPERTY(BlueprintReadWrite, Category = UI, meta = (DisplayName = "Parameter UI Metadata"))
	FMutableParamUIMetadata ParamUIMetadata;

	/** Parameter type, using uint8 since the enum in declared in the class it is used */
	UPROPERTY(BlueprintReadWrite, Category = UI)
	EMutableParameterType Type;

	/** In the case of an integer parameter, store here all options */
	UPROPERTY(BlueprintReadWrite, Category = UI)
	TArray<FIntegerParameterUIData> ArrayIntegerParameterOption;

	/** In the case of an integer parameter, how are the different options selected (one, one or none, etc...) */
	UPROPERTY(BlueprintReadWrite, Category = UI)
	ECustomizableObjectGroupType IntegerParameterGroupType;

	UPROPERTY(BlueprintReadWrite, Category = CustomizableObject)
	bool bDontCompressRuntimeTextures = false; // Only useful for state metadata

	UPROPERTY(BlueprintReadWrite, Category = CustomizableObject)
	TMap<FString, FString> ForcedParameterValues;

	bool operator ==(const FParameterUIData& Other) const
	{
		if (Name != Other.Name || ParamUIMetadata != Other.ParamUIMetadata || Type != Other.Type || ArrayIntegerParameterOption != Other.ArrayIntegerParameterOption || IntegerParameterGroupType != Other.IntegerParameterGroupType || !ForcedParameterValues.OrderIndependentCompareEqual(Other.ForcedParameterValues))
		{
			return false;
		}

		return true;
	}

	friend FArchive& operator <<(FArchive& Ar, FParameterUIData& UIData)
	{
		Ar << UIData.Name;
		Ar << UIData.ParamUIMetadata;
		Ar << UIData.Type;
		Ar << UIData.ArrayIntegerParameterOption;
		Ar << UIData.IntegerParameterGroupType;
		Ar << UIData.bDontCompressRuntimeTextures;
		Ar << UIData.ForcedParameterValues;

		return Ar;
	}

#if WITH_EDITOR

	/** Only called in the BeginCacheForCookPlatform to include new references in the final package */
	void LoadResources()
	{
		ParamUIMetadata.LoadResources();

		for (FIntegerParameterUIData& intParam : ArrayIntegerParameterOption)
		{
			intParam.LoadResources();
		}
	}

#endif
};
