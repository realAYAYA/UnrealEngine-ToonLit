// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeBlueprintBrushBase.h"
#include "CoreMinimal.h"
#include "LandscapeProxy.h"
#include "Landscape.h"
#include "LandscapeEditTypes.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapePrivate.h"
#include "Misc/MapErrors.h"
#include "Misc/UObjectToken.h"
#include "Logging/MessageLog.h"
#include "Logging/TokenizedMessage.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LandscapeBlueprintBrushBase)

#define LOCTEXT_NAMESPACE "Landscape"

#if WITH_EDITOR
static const uint32 InvalidLastRequestLayersContentUpdateFrameNumber = 0;

static TAutoConsoleVariable<int32> CVarLandscapeBrushPadding(
	TEXT("landscape.BrushFramePadding"),
	5,
	TEXT("The number of frames to wait before pushing a full Landscape update when a brush is calling RequestLandscapeUpdate"));
#endif

ALandscapeBlueprintBrushBase::ALandscapeBlueprintBrushBase(const FObjectInitializer& ObjectInitializer)
#if WITH_EDITORONLY_DATA
	: OwningLandscape(nullptr)
	, UpdateOnPropertyChange(true)
	, AffectHeightmap(false)
	, AffectWeightmap(false)
	, AffectVisibilityLayer(false)
	, bIsVisible(true)
	, LastRequestLayersContentUpdateFrameNumber(InvalidLastRequestLayersContentUpdateFrameNumber)
#endif
{
#if WITH_EDITOR
	USceneComponent* SceneComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	RootComponent = SceneComp;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_DuringPhysics;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.SetTickFunctionEnable(true);
	bIsEditorOnlyActor = true;
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS

UTextureRenderTarget2D* ALandscapeBlueprintBrushBase::Render_Implementation(bool InIsHeightmap, UTextureRenderTarget2D* InCombinedResult, const FName& InWeightmapLayerName)
{
	return Render_Native(InIsHeightmap, InCombinedResult, InWeightmapLayerName);
}

UTextureRenderTarget2D* ALandscapeBlueprintBrushBase::RenderLayer_Implementation(const FLandscapeBrushParameters& InParameters)
{
	return RenderLayer_Native(InParameters);
}

UTextureRenderTarget2D* ALandscapeBlueprintBrushBase::RenderLayer_Native(const FLandscapeBrushParameters& InParameters)
{
	const bool bIsHeightmap = InParameters.LayerType == ELandscapeToolTargetType::Heightmap;

	// Without any implementation, we call the former Render method so content created before the deprecation will still work as expected.
	return Render(bIsHeightmap, InParameters.CombinedResult, InParameters.WeightmapLayerName);
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

void ALandscapeBlueprintBrushBase::Initialize_Implementation(const FTransform& InLandscapeTransform, const FIntPoint& InLandscapeSize, const FIntPoint& InLandscapeRenderTargetSize)
{
	Initialize_Native(InLandscapeTransform, InLandscapeSize, InLandscapeRenderTargetSize);
}

void ALandscapeBlueprintBrushBase::RequestLandscapeUpdate(bool bInUserTriggered)
{
#if WITH_EDITORONLY_DATA
	UE_LOG(LogLandscape, Verbose, TEXT("ALandscapeBlueprintBrushBase::RequestLandscapeUpdate"));
	if (OwningLandscape)
	{
		uint32 ModeMask = 0;
		if (AffectsHeightmap())
		{
			ModeMask |= ELandscapeLayerUpdateMode::Update_Heightmap_Editing_NoCollision;
		}
		if (AffectsWeightmap() || AffectsVisibilityLayer())
		{
			ModeMask |= ELandscapeLayerUpdateMode::Update_Weightmap_Editing_NoCollision;
		}
		if (ModeMask)
		{
			OwningLandscape->RequestLayersContentUpdateForceAll((ELandscapeLayerUpdateMode)ModeMask, bInUserTriggered);
			// Just in case differentiate between 0 (default value and frame number)
			LastRequestLayersContentUpdateFrameNumber = GFrameNumber == InvalidLastRequestLayersContentUpdateFrameNumber ? GFrameNumber + 1 : GFrameNumber;
		}
	}
#endif
}

#if WITH_EDITOR
void ALandscapeBlueprintBrushBase::PushDeferredLayersContentUpdate()
{
#if WITH_EDITORONLY_DATA
	// Avoid computing collision and client updates every frame
	// Wait until we didn't trigger any more landscape update requests (padding of a couple of frames)
	if (OwningLandscape != nullptr &&
		LastRequestLayersContentUpdateFrameNumber != InvalidLastRequestLayersContentUpdateFrameNumber &&
		LastRequestLayersContentUpdateFrameNumber + CVarLandscapeBrushPadding.GetValueOnAnyThread() <= GFrameNumber)
	{
		uint32 ModeMask = 0;
		if (AffectsHeightmap())
		{
			ModeMask |= ELandscapeLayerUpdateMode::Update_Heightmap_All;
		}
		if (AffectsWeightmap() || AffectsVisibilityLayer())
		{
			ModeMask |= ELandscapeLayerUpdateMode::Update_Weightmap_All;
		}
		if (ModeMask)
		{
			OwningLandscape->RequestLayersContentUpdateForceAll((ELandscapeLayerUpdateMode)ModeMask);
		}
		LastRequestLayersContentUpdateFrameNumber = InvalidLastRequestLayersContentUpdateFrameNumber;
	}
#endif
}

void ALandscapeBlueprintBrushBase::Tick(float DeltaSeconds)
{
	// Forward the Tick to the instances class of this BP
	if (GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
	{
		TGuardValue<bool> AutoRestore(GAllowActorScriptExecutionInEditor, true);
		ReceiveTick(DeltaSeconds);
	}

	Super::Tick(DeltaSeconds);
}

bool ALandscapeBlueprintBrushBase::ShouldTickIfViewportsOnly() const
{
	return true;
}

bool ALandscapeBlueprintBrushBase::IsLayerUpdatePending() const
{
	return GFrameNumber < LastRequestLayersContentUpdateFrameNumber + CVarLandscapeBrushPadding.GetValueOnAnyThread();
}

void ALandscapeBlueprintBrushBase::SetIsVisible(bool bInIsVisible)
{
#if WITH_EDITORONLY_DATA
	Modify();
	bIsVisible = bInIsVisible;
	if (OwningLandscape)
	{
		OwningLandscape->OnBlueprintBrushChanged();
	}
#endif
}


PRAGMA_DISABLE_DEPRECATION_WARNINGS
void ALandscapeBlueprintBrushBase::SetAffectsHeightmap(bool bAffectsHeightmap)
{
	SetCanAffectHeightmap(bAffectsHeightmap);
}

void ALandscapeBlueprintBrushBase::SetAffectsWeightmap(bool bAffectsWeightmap)
{
	SetCanAffectWeightmap(bAffectsWeightmap);
}

void ALandscapeBlueprintBrushBase::SetAffectsVisibilityLayer(bool bInAffectsVisibilityLayer)
{
	SetCanAffectVisibilityLayer(bInAffectsVisibilityLayer);
}

bool ALandscapeBlueprintBrushBase::IsAffectingWeightmapLayer(const FName& InLayerName) const
{
	return AffectsWeightmapLayer(InLayerName);
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void ALandscapeBlueprintBrushBase::SetCanAffectHeightmap(bool bInCanAffectHeightmap)
{
#if WITH_EDITORONLY_DATA
	if (bInCanAffectHeightmap != AffectHeightmap)
	{
		Modify();
		AffectHeightmap = bInCanAffectHeightmap;
		if (OwningLandscape)
		{
			OwningLandscape->OnBlueprintBrushChanged();
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void ALandscapeBlueprintBrushBase::SetCanAffectWeightmap(bool bInCanAffectWeightmap)
{
#if WITH_EDITORONLY_DATA
	if (bInCanAffectWeightmap != AffectWeightmap)
	{
		Modify();
		AffectWeightmap = bInCanAffectWeightmap;
		if (OwningLandscape)
		{
			OwningLandscape->OnBlueprintBrushChanged();
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void ALandscapeBlueprintBrushBase::SetCanAffectVisibilityLayer(bool bInCanAffectVisibilityLayer)
{
#if WITH_EDITORONLY_DATA
	if (bInCanAffectVisibilityLayer != AffectVisibilityLayer)
	{
		Modify();
		AffectVisibilityLayer = bInCanAffectVisibilityLayer;
		if (OwningLandscape)
		{
			OwningLandscape->OnBlueprintBrushChanged();
		}
	}
#endif // WITH_EDITORONLY_DATA
}

bool ALandscapeBlueprintBrushBase::AffectsWeightmapLayer(const FName& InLayerName) const
{
#if WITH_EDITORONLY_DATA
	if (!CanAffectWeightmap())
	{
		return false;
	}

	return AffectedWeightmapLayers.Contains(InLayerName);
#else // WITH_EDITORONLY_DATA
	return false;
#endif // !WITH_EDITORONLY_DATA
}

void ALandscapeBlueprintBrushBase::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
#if WITH_EDITORONLY_DATA
	RequestLandscapeUpdate();
#endif
}

void ALandscapeBlueprintBrushBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
#if WITH_EDITORONLY_DATA
	if (OwningLandscape && UpdateOnPropertyChange)
	{
		OwningLandscape->OnBlueprintBrushChanged();
	}
#endif
}

void ALandscapeBlueprintBrushBase::Destroyed()
{
	Super::Destroyed();
#if WITH_EDITORONLY_DATA
	if (OwningLandscape && !GIsReinstancing)
	{
		OwningLandscape->RemoveBrush(this);
	}
	OwningLandscape = nullptr;
#endif
}

void ALandscapeBlueprintBrushBase::CheckForErrors()
{
	Super::CheckForErrors();

	if (GetWorld() && !IsTemplate())
	{
		if (OwningLandscape == nullptr)
		{
			FMessageLog("MapCheck").Error()
				->AddToken(FUObjectToken::Create(this))
				->AddToken(FTextToken::Create(LOCTEXT("MapCheck_Message_MissingLandscape", "This brush requires a Landscape. Add one to the map or remove the brush actor.")))
				->AddToken(FMapErrorToken::Create(TEXT("LandscapeBrushMissingLandscape")));
		}
	}
}

void ALandscapeBlueprintBrushBase::GetRenderDependencies(TSet<UObject*>& OutDependencies)
{
	TArray<UObject*> BPDependencies;
	GetBlueprintRenderDependencies(BPDependencies);

	OutDependencies.Append(BPDependencies);
}

void ALandscapeBlueprintBrushBase::SetOwningLandscape(ALandscape* InOwningLandscape)
{
#if WITH_EDITORONLY_DATA
	if (OwningLandscape == InOwningLandscape)
	{
		return;
	}

	const bool bAlwaysMarkDirty = false;
	Modify(bAlwaysMarkDirty);

	if (OwningLandscape)
	{
		OwningLandscape->OnBlueprintBrushChanged();
	}

	OwningLandscape = InOwningLandscape;

	if (OwningLandscape)
	{
		OwningLandscape->OnBlueprintBrushChanged();
	}
#endif
}

ALandscape* ALandscapeBlueprintBrushBase::GetOwningLandscape() const
{
#if WITH_EDITORONLY_DATA
	return OwningLandscape;
#else
	return nullptr;
#endif
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
