// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGTextureSampler.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGCrc.h"
#include "Helpers/PCGBlueprintHelpers.h"
#include "Helpers/PCGDynamicTrackingHelpers.h"
#include "Helpers/PCGHelpers.h"
#include "Helpers/PCGSettingsHelpers.h"

#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGTextureSampler)

#define LOCTEXT_NAMESPACE "PCGTextureSamplerElement"

#if WITH_EDITOR
void UPCGTextureSamplerSettings::GetStaticTrackedKeys(FPCGSelectionKeyToSettingsMap& OutKeysToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const
{
	if (IsPropertyOverriddenByPin(GET_MEMBER_NAME_CHECKED(UPCGTextureSamplerSettings, Texture)) || Texture.IsNull())
	{
		// Dynamic tracking or null settings
		return;
	}

	FPCGSelectionKey Key = FPCGSelectionKey::CreateFromPath(Texture.ToSoftObjectPath());

	OutKeysToSettings.FindOrAdd(Key).Emplace(this, /*bCulling=*/false);
}

void UPCGTextureSamplerSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) 
{
	if (PropertyChangedEvent.Property)
	{
		const FName PropertyName = PropertyChangedEvent.Property->GetFName();

		if (PropertyName == GET_MEMBER_NAME_CHECKED(UPCGTextureSamplerSettings, Texture))
		{
			UpdateDisplayTextureArrayIndex();
		} 
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UPCGTextureSamplerSettings::PostLoad()
{
	Super::PostLoad();

	UpdateDisplayTextureArrayIndex();
}
#endif

#if WITH_EDITOR
FText UPCGTextureSamplerSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Generates points by sampling the given texture.\n"
		"If the texture is CPU-accessible, the sampler will prefer the CPU version of the texture.\n"
		"Otherwise, the texture will be read back from the GPU if one is present.");
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGTextureSamplerSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Texture);

	return Properties;
}

FPCGElementPtr UPCGTextureSamplerSettings::CreateElement() const
{
	return MakeShared<FPCGTextureSamplerElement>();
}

#if WITH_EDITOR
void UPCGTextureSamplerSettings::UpdateDisplayTextureArrayIndex()
{
	UTexture* NewTexture = Texture.LoadSynchronous();
	bDisplayTextureArrayIndex = NewTexture && NewTexture->IsA<UTexture2DArray>();
}
#endif

bool FPCGTextureSamplerElement::PrepareDataInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGTextureSamplerElement::Execute);

	FPCGTextureSamplerContext* Context = static_cast<FPCGTextureSamplerContext*>(InContext);
	check(Context);

	const UPCGTextureSamplerSettings* Settings = Context->GetInputSettings<UPCGTextureSamplerSettings>();
	check(Settings);

	if (Settings->Texture.IsNull())
	{
		return true;
	}

	if (!Context->WasLoadRequested())
	{
		return Context->RequestResourceLoad(Context, { Settings->Texture.ToSoftObjectPath() }, !Settings->bSynchronousLoad);
	}

	return true;
}

bool FPCGTextureSamplerElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGTextureSamplerElement::Execute);

	FPCGTextureSamplerContext* Context = static_cast<FPCGTextureSamplerContext*>(InContext);
	check(Context);

	if (Context->bIsPaused)
	{
		return false;
	}

	if (Context->bTextureReadbackDone)
	{
		return true;
	}

	const UPCGTextureSamplerSettings* Settings = Context->GetInputSettings<UPCGTextureSamplerSettings>();
	check(Settings);

	if (Settings->Texture.IsNull())
	{
		return true;
	}

	UTexture* Texture = Settings->Texture.Get();

	if (!Texture)
	{
		PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("CouldNotResolveTexture", "Texture at path '{0}' could not be loaded"), FText::FromString(Settings->Texture.ToString())));
		return true;
	}

	uint32 TextureArrayIndex = 0;

	if (UTexture2DArray* TextureArray = Cast<UTexture2DArray>(Texture))
	{
#if WITH_EDITOR
		const int32 ArraySize = TextureArray->SourceTextures.Num();
#else
		const int32 ArraySize = TextureArray->GetArraySize();
#endif

		if (Settings->TextureArrayIndex >= 0 && Settings->TextureArrayIndex < ArraySize)
		{
			TextureArrayIndex = Settings->TextureArrayIndex;
		}
		else
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("InvalidTextureIndex", "Array index {0} was out of bounds for TextureArray at path '{1}'."), Settings->TextureArrayIndex, FText::FromString(Settings->Texture.ToString())));
			return true;
		}
	}
	else if (!Texture->IsA<UTexture2D>())
	{
		PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("InvalidTextureType", "Texture at path '{0}' is not a valid type. Must be UTexture2D or UTexture2DArray."), FText::FromString(Settings->Texture.ToString())));
		return true;
	}

	const FTransform& Transform = Settings->Transform;
	const bool bUseAbsoluteTransform = Settings->bUseAbsoluteTransform;
	const EPCGTextureDensityFunction DensityFunction = Settings->DensityFunction;
	const EPCGTextureColorChannel ColorChannel = Settings->ColorChannel;
	const EPCGTextureFilter Filter = Settings->Filter;
	const float TexelSize = Settings->TexelSize;
	const bool bUseAdvancedTiling = Settings->bUseAdvancedTiling;
	const FVector2D& Tiling = Settings->Tiling;
	const FVector2D& CenterOffset = Settings->CenterOffset;
	const float Rotation = Settings->Rotation;
	const bool bUseTileBounds = Settings->bUseTileBounds;
	const FVector2D& TileBoundsMin = Settings->TileBoundsMin;
	const FVector2D& TileBoundsMax = Settings->TileBoundsMax;
#if WITH_EDITOR
	const bool bForceEditorOnlyCPUSampling = Settings->bForceEditorOnlyCPUSampling;
#else
	const bool bForceEditorOnlyCPUSampling = false;
#endif

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	FPCGTaggedData& Output = Outputs.Emplace_GetRef();

	UPCGTextureData* TextureData = NewObject<UPCGTextureData>();
	Output.Data = TextureData;

	AActor* OriginalActor = UPCGBlueprintHelpers::GetOriginalComponent(*Context)->GetOwner();
	FTransform FinalTransform = Transform;
	if (!bUseAbsoluteTransform)
	{
		FTransform OriginalActorTransform = OriginalActor->GetTransform();
		FinalTransform = Transform * OriginalActorTransform;

		FBox OriginalActorLocalBounds = PCGHelpers::GetActorLocalBounds(OriginalActor);
		FinalTransform.SetScale3D(FinalTransform.GetScale3D() * 0.5 * (OriginalActorLocalBounds.Max - OriginalActorLocalBounds.Min));
	}

	// Initialize & set properties
	Context->bIsPaused = true;

	auto PostInitializeCallback = [Context, TextureData]()
	{
		Context->bIsPaused = false;
		Context->bTextureReadbackDone = true;

		if (!TextureData->IsValid())
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("TextureDataInitFailed", "Texture data failed to initialize, check log for more information"));
		}
	};

	TextureData->Initialize(Texture, TextureArrayIndex, FinalTransform, PostInitializeCallback, bForceEditorOnlyCPUSampling);

	TextureData->DensityFunction = DensityFunction;
	TextureData->ColorChannel = ColorChannel;
	TextureData->Filter = Filter;
	TextureData->TexelSize = TexelSize;
	TextureData->bUseAdvancedTiling = bUseAdvancedTiling;
	TextureData->Tiling = Tiling;
	TextureData->CenterOffset = CenterOffset;
	TextureData->Rotation = Rotation;
	TextureData->bUseTileBounds = bUseTileBounds;
	TextureData->TileBounds = FBox2D(TileBoundsMin, TileBoundsMax);

#if WITH_EDITOR
	// If we have an override, register for dynamic tracking.
	if (Context->IsValueOverriden(GET_MEMBER_NAME_CHECKED(UPCGTextureSamplerSettings, Texture)))
	{
		FPCGDynamicTrackingHelper::AddSingleDynamicTrackingKey(Context, FPCGSelectionKey::CreateFromPath(Texture), /*bIsCulled=*/false);
	}
#endif // WITH_EDITOR

	return false;
}

FPCGContext* FPCGTextureSamplerElement::CreateContext()
{
	return new FPCGTextureSamplerContext();
}

void UPCGTextureSamplerSettings::SetTexture(TSoftObjectPtr<UTexture> InTexture)
{
	Texture = InTexture;

#if WITH_EDITOR
	UpdateDisplayTextureArrayIndex();
#endif // WITH_EDITOR
}

void FPCGTextureSamplerElement::GetDependenciesCrc(const FPCGDataCollection& InInput, const UPCGSettings* InSettings, UPCGComponent* InComponent, FPCGCrc& OutCrc) const
{
	FPCGCrc Crc;
	IPCGElement::GetDependenciesCrc(InInput, InSettings, InComponent, Crc);

	if (const UPCGTextureSamplerSettings* Settings = Cast<UPCGTextureSamplerSettings>(InSettings))
	{
		// If not using absolute transform, depend on actor transform and bounds, and therefore take dependency on actor data.
		bool bUseAbsoluteTransform;
		PCGSettingsHelpers::GetOverrideValue(InInput, Settings, GET_MEMBER_NAME_CHECKED(UPCGTextureSamplerSettings, bUseAbsoluteTransform), Settings->bUseAbsoluteTransform, bUseAbsoluteTransform);
		if (!bUseAbsoluteTransform && InComponent)
		{
			if (const UPCGData* Data = InComponent->GetActorPCGData())
			{
				Crc.Combine(Data->GetOrComputeCrc(/*bFullDataCrc=*/false));
			}
		}
	}

	OutCrc = Crc;
}

#undef LOCTEXT_NAMESPACE
