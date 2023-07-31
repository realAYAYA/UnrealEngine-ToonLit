// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapePatchManager.h"

#include "Modules/ModuleManager.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Landscape.h"
#include "LandscapeDataAccess.h"
#include "LandscapePatchComponent.h"
#include "LandscapePatchLogging.h"
#include "RenderGraph.h" // RDG_EVENT_NAME
#include "LandscapeModule.h"
#include "LandscapeEditorServices.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LandscapePatchManager)

#define LOCTEXT_NAMESPACE "LandscapePatchManager"

// TODO: Not sure if using this kind of constructor is a proper thing to do vs some other hook...
ALandscapePatchManager::ALandscapePatchManager(const FObjectInitializer& ObjectInitializer)
	: ALandscapeBlueprintBrushBase(ObjectInitializer)
{
#if WITH_EDITOR
	SetAffectsHeightmap(true);
	SetAffectsWeightmap(true);
#endif
}

void ALandscapePatchManager::Initialize_Native(const FTransform & InLandscapeTransform,
	const FIntPoint& InLandscapeSize,
	const FIntPoint& InLandscapeRenderTargetSize)
{
	// Get a transform from pixel coordinate in heightmap to world space coordinate. Note that we can't
	// store the inverse directly because a FTransform can't properly represent a TRS inverse when the
	// original TRS has non-uniform scaling).

	// The pixel to landscape-space transform is unrotated, (S_p * x + T_p). The landscape to world
	// transform gets applied on top of this: (R_l * S_l * (S_p * x + T_p)) + T_L. Collapsing this
	// down to pixel to world TRS, we get: R_l * (S_l * S_p) * x + (R_l * S_l * T_p + T_L)

	// To go from stored height value to unscaled height, we divide by 128 and subtract 256. We can get these
	// values from the constants in LandscapeDataAccess.h (we distribute the multiplication by LANDSCAPE_ZSCALE
	// so that translation happens after scaling like in TRS)
	const double HEIGHTMAP_TO_OBJECT_HEIGHT_SCALE = LANDSCAPE_ZSCALE;
	const double HEIGHTMAP_TO_OBJECT_HEIGHT_OFFSET = -LandscapeDataAccess::MidValue * LANDSCAPE_ZSCALE;

	// S_p: the pixel coordinate scale is actually the same as xy object-space coordinates because one quad is 1 unit,
	// so we only need to scale the height.
	FVector3d PixelToObjectSpaceScale = FVector3d(
		1,
		1,
		HEIGHTMAP_TO_OBJECT_HEIGHT_SCALE
	);

	// T_p: the center of the pixel
	FVector3d PixelToObjectSpaceTranslate = FVector3d(
		-0.5,
		-0.5,
		HEIGHTMAP_TO_OBJECT_HEIGHT_OFFSET
	);

	// S_l* S_p: composed scale
	HeightmapCoordsToWorld.SetScale3D(InLandscapeTransform.GetScale3D() * PixelToObjectSpaceScale);

	// R_l
	HeightmapCoordsToWorld.SetRotation(InLandscapeTransform.GetRotation());

	// R_l * S_l * T_p + T_L: composed translation
	HeightmapCoordsToWorld.SetTranslation(InLandscapeTransform.TransformVector(PixelToObjectSpaceTranslate)
		+ InLandscapeTransform.GetTranslation());
}

UTextureRenderTarget2D* ALandscapePatchManager::Render_Native(bool InIsHeightmap,
	UTextureRenderTarget2D* InCombinedResult,
	const FName& InWeightmapLayerName)
{
	// Used to determine whether we need to remove any invalid brushes
	bool bHaveInvalidPatches = false;

	// TODO: There are many uncertainties in how we iterate across the height patches and have them
	// apply themselves. For one thing we may want to pass around a render graph, in which case this
	// loop will happen on the render thread somehow. For another, it's not yet determined what all
	// of this will look like when we have the ability to render to just a subsection of the entire
	// height map.
	// So for now we do the simplest thing, and that is to have the height patches act as if they were
	// independent brushes.
	for (TSoftObjectPtr<ULandscapePatchComponent>& Component : PatchComponents)
	{
		if (Component.IsPending())
		{
			Component.LoadSynchronous();
		}

		if (Component.IsValid())
		{
			if (Component->IsEnabled())
			{
				InCombinedResult = Component->Render_Native(InIsHeightmap, InCombinedResult, InWeightmapLayerName);
			}
		}
		else if (Component.IsNull())
		{
			// Theoretically when components are marked for destruction, they should remove themselves from
			// the patch manager in their OnComponentDestroyed call. However there seem to be ways to end up
			// with destroyed patches not being removed, for instance through saving the manager but not the
			// patch actor.
			UE_LOG(LogLandscapePatch, Warning, TEXT("ALandscapePatchManager: Found an invalid patch in patch manager. It will be removed."));
			bHaveInvalidPatches = true;
		}
		else
		{
			// This means that IsPending() was true, but LoadSynchronous() failed, which we generally don't
			// expect to happen. However, it can happen in some edge cases such as if you force delete a patch
			// holder blueprint and don't save the patch manager afterward. Whatever the reason, this is likely
			// a dead patch that actually needs removal.
			UE_LOG(LogLandscapePatch, Warning, TEXT("ALandscapePatchManager: Found a pending patch pointer in patch manager that "
				"turned out to be invalid. It will be removed."));
			Component = nullptr;
			bHaveInvalidPatches = true;
		}
	}

	if (bHaveInvalidPatches)
	{
		PatchComponents.RemoveAll([](TSoftObjectPtr<ULandscapePatchComponent> Component) {
			return Component.IsNull();
		});
	}

	return InCombinedResult;
}

void ALandscapePatchManager::SetTargetLandscape(ALandscape* InTargetLandscape)
{
#if WITH_EDITOR
	if (OwningLandscape != InTargetLandscape)
	{
		if (OwningLandscape)
		{
			OwningLandscape->RemoveBrush(this);
		}

		if (InTargetLandscape && ensure(InTargetLandscape->CanHaveLayersContent()))
		{
			static const FName PatchLayerName = FName("LandscapePatches");

			ILandscapeModule& LandscapeModule = FModuleManager::GetModuleChecked<ILandscapeModule>("Landscape");
			int32 PatchLayerIndex = LandscapeModule.GetLandscapeEditorServices()->GetOrCreateEditLayer(PatchLayerName, InTargetLandscape);
			
			// Among other things, this will call SetOwningLandscape on us.
			InTargetLandscape->AddBrushToLayer(PatchLayerIndex, this);

			// It's not clear whether this is really necessary, but we do it for consistency because Landscape does this in its
			// PostLoad for all its brushes (through FLandscapeLayerBrush::SetOwner). One would think that it would be done 
			// in AddBrushToLayer if it were at all important, but it currently isn't...
			if (this->GetTypedOuter<ULevel>() != InTargetLandscape->GetTypedOuter<ULevel>())
			{
				// Change owner to be that level
				this->Rename(nullptr, InTargetLandscape->GetTypedOuter<ULevel>());
			}
		}
	}
#endif
}

bool ALandscapePatchManager::ContainsPatch(TObjectPtr<ULandscapePatchComponent> Patch) const
{
	return PatchComponents.Contains(Patch);
}

void ALandscapePatchManager::AddPatch(TObjectPtr<ULandscapePatchComponent> Patch)
{
	if (Patch)
	{
		Modify();
		PatchComponents.AddUnique(TSoftObjectPtr<ULandscapePatchComponent>(Patch.Get()));

		// No need to update if the patch is disabled. Important to avoid needlessly updating while dragging a blueprint with
		// a disabled patch (since construction scripts constantly add and remove).
		if (Patch->IsEnabled())
		{
			RequestLandscapeUpdate();
		}
	}
}

bool ALandscapePatchManager::RemovePatch(TObjectPtr<ULandscapePatchComponent> Patch)
{
	bool bRemoved = false;

	if (Patch)
	{
		Modify();
		bRemoved = PatchComponents.Remove(TSoftObjectPtr<ULandscapePatchComponent>(Patch.Get())) > 0;

		// No need to update if the patch was already disabled.Important to avoid needlessly updating while dragging 
		// a blueprint with a disabled patch (since construction scripts constantly add and remove).
		if (bRemoved && Patch->IsEnabled())
		{
			RequestLandscapeUpdate();
		}
	}
	
	return bRemoved;
}

int32 ALandscapePatchManager::GetIndexOfPatch(TObjectPtr<const ULandscapePatchComponent> Patch) const
{
	return PatchComponents.IndexOfByKey(Patch);
}

void ALandscapePatchManager::MovePatchToIndex(TObjectPtr<ULandscapePatchComponent> Patch, int32 Index)
{
	if (!Patch || Index < 0 || GetIndexOfPatch(Patch) == Index)
	{
		return;
	}

	// It might seem like the index needs adjusting if we're removing before the given index, but that
	// is not the case if our goal is for the index of the patch to be the given index at the end (rather
	// than our goal being that the patch be in a particular position relative to the existing patches).
	RemovePatch(Patch);

	Index = FMath::Clamp(Index, 0, PatchComponents.Num());
	PatchComponents.Insert(TSoftObjectPtr<ULandscapePatchComponent>(Patch.Get()), Index);

	if (Patch->IsEnabled())
	{
		RequestLandscapeUpdate();
	}
}

#if WITH_EDITOR
bool ALandscapePatchManager::IsAffectingWeightmapLayer(const FName& InLayerName) const
{
	for (const TSoftObjectPtr<ULandscapePatchComponent>& Component : PatchComponents)
	{
		if (Component.IsPending())
		{
			Component.LoadSynchronous();
		}

		if (Component.IsValid() && Component->IsEnabled() && Component->IsAffectingWeightmapLayer(InLayerName))
		{
			return true;
		}
	}

	return false;
}

void ALandscapePatchManager::PostEditUndo()
{
	RequestLandscapeUpdate();
}

void ALandscapePatchManager::SetOwningLandscape(ALandscape* InOwningLandscape)
{
	Super::SetOwningLandscape(InOwningLandscape);

	DetailPanelLandscape = OwningLandscape;
}

// We override PostEditChange to allow the users to change the owning landscape via a property displayed in the detail panel.
void ALandscapePatchManager::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Do a bunch of checks to make sure that we don't try to do anything when the editing is happening inside the blueprint editor.
	UWorld* World = GetWorld();
	if (IsTemplate() || !IsValid(this) || !IsValid(World) || World->WorldType != EWorldType::Editor)
	{
		return;
	}

	if (PropertyChangedEvent.Property
		&& (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(ALandscapePatchManager, DetailPanelLandscape)))
	{
		SetTargetLandscape(DetailPanelLandscape.Get());
	}
}
#endif

#undef LOCTEXT_NAMESPACE
