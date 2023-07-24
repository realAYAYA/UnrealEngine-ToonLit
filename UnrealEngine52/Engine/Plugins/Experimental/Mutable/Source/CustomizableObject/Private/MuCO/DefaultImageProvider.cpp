// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/DefaultImageProvider.h" 

#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DefaultImageProvider)


namespace UDefaultImageProviderCVars
{
	void CheckImageMode(IConsoleVariable* Var);
	
	int32 ImageMode = 3;
	FAutoConsoleVariableRef CVarImageMode(
		TEXT("Mutable.DefaultImageProvider.ImageMode"),
		ImageMode,
		TEXT("0 = None: Texture is not provided by this provider.\n"
		"2 = Unreal: Data will be provided from an unreal texture, loaded in the game thread and kept in memory.\n"
		"3 = Unreal_Deferred (default): Data will be provided from an unreal texture. Will only be loaded when actually needed in the Mutable thread."),
		FConsoleVariableDelegate::CreateStatic(CheckImageMode));

	void CheckImageMode(IConsoleVariable* Var)
	{
		if ((ImageMode < 0 || ImageMode >= static_cast<int32>(UCustomizableSystemImageProvider::ValueType::Count)) &&
			ImageMode == static_cast<int32>(UCustomizableSystemImageProvider::ValueType::Raw)) // Raw not supported.
		{
			UE_LOG(LogMutable, Error, TEXT("DefaultImageProvider: Incorrect Image Mode. Setting Texture Mode to \"None\"."));
			ImageMode = static_cast<int32>(UCustomizableSystemImageProvider::ValueType::None);
		}
	}
}


UCustomizableSystemImageProvider::ValueType UDefaultImageProvider::HasTextureParameterValue(int64 ID)
{
	const int32 TextureIndex = ToIndex(ID);
	
	return Textures.IsValidIndex(TextureIndex) && Textures[TextureIndex] ?
		static_cast<ValueType>(UDefaultImageProviderCVars::ImageMode) :
		ValueType::None;
}


UTexture2D* UDefaultImageProvider::GetTextureParameterValue(int64 ID)
{
	return Textures[ToIndex(ID)];
}


void UDefaultImageProvider::GetTextureParameterValues(TArray<FCustomizableObjectExternalTexture>& OutValues)
{
	for (int32 TextureIndex = 0; TextureIndex < Textures.Num(); ++TextureIndex)
	{
		if (const UTexture2D* Texture = Textures[TextureIndex])
		{
			FCustomizableObjectExternalTexture Data;
			Data.Name = Texture->GetName();
			Data.Value = ToTextureId(TextureIndex);
			OutValues.Add(Data);
		}
	}
}


UTexture2D* UDefaultImageProvider::Get(const uint64 TextureId) const
{
	return Textures[ToIndex(TextureId)];
}


int64 UDefaultImageProvider::Get(UTexture2D* Texture) const
{
	for (int32 TextureIndex = 0; TextureIndex < Textures.Num(); ++TextureIndex)
	{
		if (Textures[TextureIndex] == Texture)
		{
			return ToTextureId(TextureIndex);
		}
	}

	return INDEX_NONE;
}


uint64 UDefaultImageProvider::GetOrAdd(UTexture2D* Texture)
{
	int32 Hole = INDEX_NONE;

	for (int32 TextureIndex = 0; TextureIndex < Textures.Num(); ++TextureIndex)
	{
		if (Textures[TextureIndex] == Texture)
		{
			return ToTextureId(TextureIndex);
		}
		else if (!Texture && Hole == INDEX_NONE)
		{
			Hole = TextureIndex;
		}
	}
	
	if (Hole == INDEX_NONE)
	{
		Hole = Textures.Num();

		const int32 NumElements = Hole + 1;
		check(NumElements < MAX_IDS); // Max number of TextureIds reached.

		Textures.SetNumUninitialized(NumElements);
		KeepTextures.SetNumUninitialized(NumElements);
	} 
	
	Textures[Hole] = Texture;
	KeepTextures[Hole] = false;
	
	return ToTextureId(Hole);
}


void UDefaultImageProvider::CacheTextures(UCustomizableObjectInstance& Instance)
{
	GarbageCollectTextureIds();

	UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();

	for (const FCustomizableObjectTextureParameterValue& TextureParameter : Instance.GetDescriptor().GetTextureParameters())
	{
		if (const int32 TextureIndex = ToIndex(TextureParameter.ParameterValue);
			Textures.IsValidIndex(TextureIndex) && Textures[TextureIndex])
		{
			System->CacheImage(TextureParameter.ParameterValue);
		}

		for (const uint64 RangeValue : TextureParameter.ParameterRangeValues)
		{
			if (const int32 TextureIndex = ToIndex(RangeValue);
				Textures.IsValidIndex(TextureIndex) && Textures[TextureIndex])
			{
				System->CacheImage(RangeValue);
			}
		}
	}
}


void UDefaultImageProvider::Keep(const uint64 TextureId, const bool bKeep)
{
	KeepTextures[ToIndex(TextureId)] = bKeep;
}


void UDefaultImageProvider::GarbageCollectTextureIds()
{
	TBitArray IdUsed;
	IdUsed.Init(false, Textures.Num());

	for (TObjectIterator<UCustomizableObjectInstance> It; It; ++It)
	{
		for (const FCustomizableObjectTextureParameterValue& TextureParameter :  It->GetDescriptor().GetTextureParameters())
		{			
			if (const int32 TextureIndex = ToIndex(TextureParameter.ParameterValue);
				IdUsed.IsValidIndex(TextureIndex))
			{
				IdUsed[TextureIndex] = true;
			}

			for (const uint64 RangeValue : TextureParameter.ParameterRangeValues)
			{
				if (const int32 TextureIndex = ToIndex(RangeValue);
					IdUsed.IsValidIndex(TextureIndex))
				{
					IdUsed[TextureIndex] = true;
				}
			}
		}
	}

	UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();

	int32 LastToKeep = -1;
	
	for (int32 TextureIndex = 0; TextureIndex < Textures.Num(); ++TextureIndex)
	{
		if (!IdUsed[TextureIndex] && !KeepTextures[TextureIndex])
		{
			Textures[TextureIndex] = nullptr;
			System->UnCacheImage(ToTextureId(TextureIndex));
		}
		else
		{
			LastToKeep = TextureIndex;
		}
	}

	if (const int32 NumToKeep = LastToKeep + 1;
		NumToKeep < Textures.Num())
	{
		Textures.SetNumUninitialized(NumToKeep);
		KeepTextures.SetNumUninitialized(NumToKeep);
	}
}


int32 UDefaultImageProvider::ToIndex(const uint64 TextureId) const
{
	return TextureId - BASE_ID;
}


uint64 UDefaultImageProvider::ToTextureId(const int32 TextureIndex) const
{
	return TextureIndex + BASE_ID;
}

