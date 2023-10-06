// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UCurveLinearColorAtlas.cpp
=============================================================================*/

#include "Curves/CurveLinearColorAtlas.h"
#include "Async/TaskGraphInterfaces.h"
#include "Curves/CurveLinearColor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CurveLinearColorAtlas)


UCurveLinearColorAtlas::UCurveLinearColorAtlas(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TextureSize = 256;
	bSquareResolution = true;
	TextureHeight = 256;
#if WITH_EDITORONLY_DATA
	bHasAnyDirtyTextures = false;
	bShowDebugColorsForNullGradients = false;
	SizeXY = { (float)TextureSize,  1.0f };
	MipGenSettings = TMGS_NoMipmaps;
#endif
	Filter = TextureFilter::TF_Bilinear;
	SRGB = false;
	AddressX = TA_Clamp;
	AddressY = TA_Clamp;
	CompressionSettings = TC_HDR;
#if WITH_EDITORONLY_DATA
	bDisableAllAdjustments = false;
	bHasCachedColorAdjustments = false;
#endif
}
#if WITH_EDITOR
bool UCurveLinearColorAtlas::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	if (bDisableAllAdjustments &&
		(InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UTexture, AdjustBrightness) ||
		 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UTexture, AdjustBrightnessCurve) ||
		 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UTexture, AdjustBrightness) ||
		 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UTexture, AdjustSaturation) ||
		 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UTexture, AdjustVibrance) ||
		 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UTexture, AdjustRGBCurve) ||
		 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UTexture, AdjustHue) ||
		 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UTexture, AdjustMinAlpha) ||
		 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UTexture, AdjustMaxAlpha) ||
		 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UTexture, bChromaKeyTexture) ||
		 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UTexture, ChromaKeyThreshold) ||
		 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UTexture, ChromaKeyColor)))
	{
		return false;
	}

	return true;
}

void UCurveLinearColorAtlas::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	// Determine whether any property that requires recompression of the texture, or notification to Materials has changed.
	bool bRequiresNotifyMaterials = false;

	if (PropertyChangedEvent.Property != nullptr)
	{
		const FName PropertyName(PropertyChangedEvent.Property->GetFName());
		// if Resizing
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UCurveLinearColorAtlas, TextureSize) || 
			PropertyName == GET_MEMBER_NAME_CHECKED(UCurveLinearColorAtlas, bSquareResolution) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UCurveLinearColorAtlas, TextureHeight))
		{
			if (bSquareResolution)
			{
				TextureHeight = TextureSize;
			}

			if ((uint32)GradientCurves.Num() > TextureHeight)
			{
				int32 OldCurveCount = GradientCurves.Num();
				GradientCurves.RemoveAt(TextureHeight, OldCurveCount - TextureHeight);
			}

			Source.Init(TextureSize, TextureHeight, 1, 1, TSF_RGBA16F);

			SizeXY = { (float)TextureSize, 1.0f };
			UpdateTextures();
			bRequiresNotifyMaterials = true;
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(UCurveLinearColorAtlas, GradientCurves))
		{
			if ((uint32)GradientCurves.Num() > TextureHeight)
			{
				int32 OldCurveCount = GradientCurves.Num();
				GradientCurves.RemoveAt(TextureHeight, OldCurveCount - TextureHeight);
			}
			else
			{
				for (int32 i = 0; i < GradientCurves.Num(); ++i)
				{
					if (GradientCurves[i] != nullptr)
					{
						GradientCurves[i]->OnUpdateCurve.AddUObject(this, &UCurveLinearColorAtlas::OnCurveUpdated);
					}
				}
				UpdateTextures();
				bRequiresNotifyMaterials = true;
			}
		}	
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(UCurveLinearColorAtlas, bDisableAllAdjustments))
		{
			if (bDisableAllAdjustments)
			{
				CacheAndResetColorAdjustments();
			}
			else
			{
				RestoreCachedColorAdjustments();
			}

			UpdateTextures();
			bRequiresNotifyMaterials = true;
		}
		else if (bDisableAllAdjustments)
		{
			if (PropertyName == GET_MEMBER_NAME_CHECKED(UTexture, AdjustBrightness))
			{
				AdjustBrightness = 0.0f;
			}
			else if (PropertyName == GET_MEMBER_NAME_CHECKED(UTexture, AdjustBrightnessCurve))
			{
				AdjustBrightnessCurve = 0.0f;
			}
			else if (PropertyName == GET_MEMBER_NAME_CHECKED(UTexture, AdjustBrightness))
			{
				AdjustBrightness = 0.0f;
			}
			else if (PropertyName == GET_MEMBER_NAME_CHECKED(UTexture, AdjustSaturation))
			{
				AdjustSaturation = 0.0f;
			}
			else if (PropertyName == GET_MEMBER_NAME_CHECKED(UTexture, AdjustVibrance))
			{
				AdjustVibrance = 0.0f;
			}
			else if (PropertyName == GET_MEMBER_NAME_CHECKED(UTexture, AdjustRGBCurve))
			{
				AdjustRGBCurve = 0.0f;
			}
			else if (PropertyName == GET_MEMBER_NAME_CHECKED(UTexture, AdjustHue))
			{
				AdjustHue = 0.0f;
			}
			else if (PropertyName == GET_MEMBER_NAME_CHECKED(UTexture, AdjustMinAlpha))
			{
				AdjustMinAlpha = 0.0f;
			}
			else if (PropertyName == GET_MEMBER_NAME_CHECKED(UTexture, AdjustMaxAlpha))
			{
				AdjustMaxAlpha = 0.0f;
			}
			else if (PropertyName == GET_MEMBER_NAME_CHECKED(UTexture, bChromaKeyTexture))
			{
				bChromaKeyTexture = false;
			}
		}
	}

	// Notify any loaded material instances if changed our compression format
	if (bRequiresNotifyMaterials)
	{
		NotifyMaterials();
	}
}

void UCurveLinearColorAtlas::CacheAndResetColorAdjustments()
{
	Modify();

	bHasCachedColorAdjustments = true;

	CachedColorAdjustments.bChromaKeyTexture = bChromaKeyTexture;
	CachedColorAdjustments.AdjustBrightness = AdjustBrightness;
	CachedColorAdjustments.AdjustBrightnessCurve = AdjustBrightnessCurve;
	CachedColorAdjustments.AdjustVibrance = AdjustVibrance;
	CachedColorAdjustments.AdjustSaturation = AdjustSaturation;
	CachedColorAdjustments.AdjustRGBCurve = AdjustRGBCurve;
	CachedColorAdjustments.AdjustHue = AdjustHue;
	CachedColorAdjustments.AdjustMinAlpha = AdjustMinAlpha;
	CachedColorAdjustments.AdjustMaxAlpha = AdjustMaxAlpha;

	AdjustBrightness = 1.0f;
	AdjustBrightnessCurve = 1.0f;
	AdjustVibrance = 0.0f;
	AdjustSaturation = 1.0f;
	AdjustRGBCurve = 1.0f;
	AdjustHue = 0.0f;
	AdjustMinAlpha = 0.0f;
	AdjustMaxAlpha = 1.0f;
	bChromaKeyTexture = false;
}

void UCurveLinearColorAtlas::RestoreCachedColorAdjustments()
{
	if (bHasCachedColorAdjustments)
	{
		Modify();

		AdjustBrightness = CachedColorAdjustments.AdjustBrightness;
		AdjustBrightnessCurve = CachedColorAdjustments.AdjustBrightnessCurve;
		AdjustVibrance = CachedColorAdjustments.AdjustVibrance;
		AdjustSaturation = CachedColorAdjustments.AdjustSaturation;
		AdjustRGBCurve = CachedColorAdjustments.AdjustRGBCurve;
		AdjustHue = CachedColorAdjustments.AdjustHue;
		AdjustMinAlpha = CachedColorAdjustments.AdjustMinAlpha;
		AdjustMaxAlpha = CachedColorAdjustments.AdjustMaxAlpha;
		bChromaKeyTexture = CachedColorAdjustments.bChromaKeyTexture;
	}
}
#endif

void UCurveLinearColorAtlas::PostLoad()
{
#if WITH_EDITOR
	if (FApp::CanEverRender())
	{
		FinishCachePlatformData();
	}

	for (int32 i = 0; i < GradientCurves.Num(); ++i)
	{
		if (GradientCurves[i] != nullptr)
		{
			GradientCurves[i]->OnUpdateCurve.AddUObject(this, &UCurveLinearColorAtlas::OnCurveUpdated);
		}
	}
	
	if (bSquareResolution)
	{
		TextureHeight = TextureSize;
	}
	Source.Init(TextureSize, TextureHeight, 1, 1, TSF_RGBA16F);
	SizeXY = { (float)TextureSize, 1.0f };
	UpdateTextures();
#endif

	Super::PostLoad();
}

#if WITH_EDITOR
static void RenderGradient(TArray<FFloat16Color>& InSrcData, UObject* Gradient, int32 StartXY, FVector2D SizeXY, bool bUseUnadjustedColor)
{
	if (Gradient == nullptr)
	{
		int32 Start = StartXY;
		for (uint32 y = 0; y < SizeXY.Y; y++)
		{
			// Create base mip for the texture we created.
			for (uint32 x = 0; x < SizeXY.X; x++)
			{
				InSrcData[Start + x + y * SizeXY.X] = FLinearColor::White;
			}
		}
	}
	else if (Gradient->IsA(UCurveLinearColor::StaticClass()))
	{
		// Render a gradient
		UCurveLinearColor* GradientCurve = CastChecked<UCurveLinearColor>(Gradient);
		if (bUseUnadjustedColor)
		{
			GradientCurve->PushUnadjustedToSourceData(InSrcData, StartXY, SizeXY);
		}
		else
		{
			GradientCurve->PushToSourceData(InSrcData, StartXY, SizeXY);
		}
	}
}

static void UpdateTexture(UCurveLinearColorAtlas& Atlas)
{
	const int32 TextureDataSize = Atlas.Source.CalcMipSize(0);

	FGuid MD5Guid;
	FMD5 MD5;
	MD5.Update(reinterpret_cast<const uint8*>(Atlas.SrcData.GetData()), TextureDataSize);
	MD5.Final(reinterpret_cast<uint8*>(&MD5Guid));

	uint32* TextureData = reinterpret_cast<uint32*>(Atlas.Source.LockMip(0));
	FMemory::Memcpy(TextureData, Atlas.SrcData.GetData(), TextureDataSize);
	Atlas.Source.UnlockMip(0);

	Atlas.Source.SetId(MD5Guid, /*bInGuidIsHash*/ true);
	Atlas.UpdateResource();
}

// Immediately render a new material to the specified slot index (SlotIndex must be within this section's range)
void UCurveLinearColorAtlas::OnCurveUpdated(UCurveBase* Curve, EPropertyChangeType::Type ChangeType)
{
	if (ChangeType != EPropertyChangeType::Interactive)
	{
		UCurveLinearColor* Gradient = CastChecked<UCurveLinearColor>(Curve);

		int32 SlotIndex = GradientCurves.Find(Gradient);

		if (SlotIndex != INDEX_NONE && (uint32)SlotIndex < MaxSlotsPerTexture())
		{
			// Determine the position of the gradient
			int32 StartXY = SlotIndex * TextureSize;

			// Render the single gradient to the render target
			RenderGradient(SrcData, Gradient, StartXY, SizeXY, bDisableAllAdjustments);

			UpdateTexture(*this);
		}
	}	
}

// Render any textures
void UCurveLinearColorAtlas::UpdateTextures()
{
	LLM_SCOPE(ELLMTag::Textures);

	// Save off the data needed to render each gradient.
	// Callback into the section owner to get the Gradients array
	const int32 TextureDataSize = Source.CalcMipSize(0);
	SrcData.Empty();
	SrcData.AddUninitialized(TextureDataSize);

	int32 NumSlotsToRender = FMath::Min(GradientCurves.Num(), (int32)MaxSlotsPerTexture());
	for (int32 i = 0; i < NumSlotsToRender; ++i)
	{
		int32 StartXY = i * TextureSize;
		RenderGradient(SrcData, GradientCurves[i], StartXY, SizeXY, bDisableAllAdjustments);
	}

	for (uint32 y = GradientCurves.Num(); y < TextureHeight; y++)
	{
		// Create base mip for the texture we created.
		for (uint32 x = 0; x < TextureSize; x++)
		{
			SrcData[y*TextureSize + x] = FLinearColor::White;
		}
	}

	UpdateTexture(*this);

	bIsDirty = false;
}

#endif

bool UCurveLinearColorAtlas::GetCurveIndex(UCurveLinearColor* InCurve, int32& Index)
{
	Index = GradientCurves.Find(InCurve);
	if (Index != INDEX_NONE)
	{
		return true;
	}
	return false;
}

bool UCurveLinearColorAtlas::GetCurvePosition(UCurveLinearColor* InCurve, float& Position)
{
	int32 Index = GradientCurves.Find(InCurve);
	Position = 0.0f;
	if (Index != INDEX_NONE)
	{
		Position = (float)Index;
		return true;
	}
	return false;
}

