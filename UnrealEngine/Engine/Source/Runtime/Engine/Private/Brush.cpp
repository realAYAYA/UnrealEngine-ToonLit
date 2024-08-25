// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Brush.cpp: Brush Actor implementation
=============================================================================*/

#include "Engine/Brush.h"
#include "Engine/Polys.h"
#include "Engine/Level.h"
#include "EngineLogs.h"
#include "Model.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"
#include "Engine/BrushBuilder.h"
#include "Components/BrushComponent.h"
#include "ActorEditorUtils.h"
#include "UObject/UnrealType.h"

#if WITH_EDITOR
#include "Editor.h"
#else
#include "Engine/Engine.h"
#endif

#if WITH_EDITOR
/** Define static delegate */
ABrush::FOnBrushRegistered ABrush::OnBrushRegistered;

/** An array to keep track of all the levels that need rebuilding. This is checked via NeedsRebuild() in the editor tick and triggers a csg rebuild. */
TArray< TWeakObjectPtr< ULevel > > ABrush::LevelsToRebuild;

/** Whether BSP regeneration should be suppressed or not */
bool ABrush::bSuppressBSPRegeneration = false;

// Debug purposes only; an attempt to catch the cause of UE-36265
const TCHAR* ABrush::GGeometryRebuildCause = nullptr;
#endif

DEFINE_LOG_CATEGORY_STATIC(LogBrush, Log, All);

ABrush::ABrush(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BrushComponent = CreateDefaultSubobject<UBrushComponent>(TEXT("BrushComponent0"));
	BrushComponent->Mobility = EComponentMobility::Static;
	BrushComponent->SetGenerateOverlapEvents(false);
	BrushComponent->SetCanEverAffectNavigation(false);

	RootComponent = BrushComponent;
	
	SetHidden(true);
	bNotForClientOrServer = false;
	SetCanBeDamaged(false);
	bCollideWhenPlacing = true;
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
}

#if WITH_EDITOR
void ABrush::PostEditMove(bool bFinished)
{
	bInManipulation = !bFinished;

	if( BrushComponent )
	{
		BrushComponent->ReregisterComponent();
	}

	Super::PostEditMove(bFinished);
}

void ABrush::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	// Prior to reregistering the BrushComponent (done in the Super), request an update to the Body Setup to take into account any change
	// in the mirroring of the Actor. This will actually be updated when the component is reregistered.
	if (BrushComponent && PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetName() == TEXT("RelativeScale3D"))
	{
		BrushComponent->RequestUpdateBrushCollision();
	}

	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}

FName ABrush::GetCustomIconName() const
{
	if (BrushType == Brush_Add)
	{
		static const FName AdditiveIconName("ClassIcon.BrushAdditive");
		return AdditiveIconName;
	}
	else if (BrushType == Brush_Subtract)
	{
		static const FName SubtactiveIconName("ClassIcon.BrushSubtractive");
		return SubtactiveIconName;
	}

	return NAME_None;
}

void ABrush::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if(Brush)
	{
		Brush->BuildBound();
	}

	bool bIsBuilderBrush = FActorEditorUtils::IsABuilderBrush( this );
	if (!bIsBuilderBrush && (BrushType == Brush_Default))
	{
		// Don't allow non-builder brushes to be set to the default brush type
		BrushType = Brush_Add;
	}
	else if (bIsBuilderBrush && (BrushType != Brush_Default))
	{
		// Don't allow the builder brush to be set to the anything other than the default brush type
		BrushType = Brush_Default;
	}

	if (!bSuppressBSPRegeneration && IsStaticBrush() && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive && GUndo)
	{
		// Don't rebuild BSP if only the actor label has changed
		static const FName ActorLabelName("ActorLabel");
		if (!PropertyChangedEvent.Property || PropertyChangedEvent.Property->GetFName() != ActorLabelName)
		{
			// BSP can only be rebuilt during a transaction
			GEditor->RebuildAlteredBSP();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void ABrush::CopyPosRotScaleFrom( ABrush* Other )
{
	check(BrushComponent);
	check(Other);
	check(Other->BrushComponent);

	SetActorLocationAndRotation(Other->GetActorLocation(), Other->GetActorRotation(), false);
	if( GetRootComponent() != NULL )
	{
		SetPivotOffset(Other->GetPivotOffset());
	}

	if(Brush)
	{
		Brush->BuildBound();
	}

	ReregisterAllComponents();
}

bool ABrush::NeedsRebuild(TArray< TWeakObjectPtr< ULevel > >* OutLevels)
{
	LevelsToRebuild.RemoveAllSwap([](const TWeakObjectPtr<ULevel>& Level) { return !Level.IsValid(); });

	if (OutLevels)
	{
		*OutLevels = LevelsToRebuild;
	}

	return(LevelsToRebuild.Num() > 0);
}

void ABrush::SetNeedRebuild(ULevel* InLevel)
{
	if (InLevel)
	{
		LevelsToRebuild.AddUnique(InLevel);
	}
}

void ABrush::InitPosRotScale()
{
	check(BrushComponent);

	SetActorLocationAndRotation(FVector::ZeroVector, FQuat::Identity, false);
	SetPivotOffset( FVector::ZeroVector );
}

void ABrush::SetIsTemporarilyHiddenInEditor( bool bIsHidden )
{
	if (IsTemporarilyHiddenInEditor() != bIsHidden)
	{
		Super::SetIsTemporarilyHiddenInEditor(bIsHidden);
		
		ULevel* Level = GetLevel();
		UModel* Model = Level ? ToRawPtr(Level->Model) : nullptr;

		if (Level && Model)
		{
			bool bAnySurfaceWasFound = false;
			for (int32 SurfIndex = 0; SurfIndex < Model->Surfs.Num(); ++SurfIndex)
			{
				FBspSurf &Surf = Model->Surfs[SurfIndex ];

				if (Surf.Actor == this)
				{
					Model->ModifySurf( SurfIndex , false );
					bAnySurfaceWasFound = true;
					Surf.bHiddenEdTemporary = bIsHidden;
				}
			}

			if (bAnySurfaceWasFound)
			{
				Level->UpdateModelComponents();
				Level->InvalidateModelSurface();
			}
		}
	}
}

bool ABrush::SetIsHiddenEdLayer(bool bIsHiddenEdLayer)
{
	if (Super::SetIsHiddenEdLayer(bIsHiddenEdLayer))
	{
		ULevel* Level = GetLevel();
		UModel* Model = Level ? ToRawPtr(Level->Model) : nullptr;
		if (Level && Model)
		{
			bool bAnySurfaceWasFound = false;
			for (FBspSurf& Surf : Model->Surfs)
			{
				if (Surf.Actor == this)
				{
					Surf.bHiddenEdLayer = bIsHiddenEdLayer;
					bAnySurfaceWasFound = true;
				}
			}

			if (bAnySurfaceWasFound)
			{
				Level->UpdateModelComponents();
				Model->InvalidSurfaces = true;
			}
		}
		return true;
	}
	return false;
}

bool ABrush::SupportsLayers() const
{
	return !FActorEditorUtils::IsABuilderBrush(this) && Super::SupportsLayers();
}

bool ABrush::SupportsExternalPackaging() const
{
	// Base class ABrush actors do not support OFPA
	return GetClass() != ABrush::StaticClass() && Super::SupportsExternalPackaging();
}

void ABrush::PostLoad()
{
	Super::PostLoad();

	if (BrushBuilder && BrushBuilder->GetOuter() != this)
	{
		BrushBuilder = DuplicateObject<UBrushBuilder>(BrushBuilder, this);
	}

	// Assign the default material to brush polys with NULL material references.
	if ( Brush && Brush->Polys )
	{
		if ( IsStaticBrush() )
		{
			for( int32 PolyIndex = 0 ; PolyIndex < Brush->Polys->Element.Num() ; ++PolyIndex )
			{
				FPoly& CurrentPoly = Brush->Polys->Element[PolyIndex];
				if ( !CurrentPoly.Material )
				{
					CurrentPoly.Material = UMaterial::GetDefaultMaterial(MD_Surface);
				}
			}
		}

#if WITH_EDITOR
		// Fix up any broken poly normals.
		// They have not been getting fixed up after vertex editing since at least UE2!
		for(FPoly& Poly : Brush->Polys->Element)
		{
			FVector3f Normal = Poly.Normal;
			if(!Poly.CalcNormal())
			{
				if(!Poly.Normal.Equals(Normal))
				{
					UE_LOG(LogBrush, Log, TEXT("%s had invalid poly normals which have been fixed. Resave the level '%s' to remove this warning."), *Brush->GetName(), *GetLevel()->GetOuter()->GetName());
					if(IsStaticBrush())
					{
						UE_LOG(LogBrush, Log, TEXT("%s had invalid poly normals which have been fixed. Resave the level '%s' to remove this warning."), *Brush->GetName(), *GetLevel()->GetOuter()->GetName());

						// Flag BSP as needing rebuild
						SetNeedRebuild(GetLevel());
					}
				}
			}
		}
#endif

		// if the polys of the brush have the wrong outer, fix it up to be the UModel (my Brush member)
		// UModelFactory::FactoryCreateText was passing in the ABrush as the Outer instead of the UModel
		if (Brush->Polys->GetOuter() == this)
		{
			Brush->Polys->Rename(*Brush->Polys->GetName(), Brush, REN_ForceNoResetLoaders);
		}
	}

	if ( BrushComponent && !BrushComponent->BrushBodySetup )
	{
		UE_LOG(LogPhysics, Log, TEXT("%s does not have BrushBodySetup. No collision."), *GetName());
	}
}

void ABrush::Destroyed()
{
	Super::Destroyed();

	if(GIsEditor && IsStaticBrush() && !GetWorld()->IsGameWorld())
	{
		// Trigger a csg rebuild if we're in the editor.
		SetNeedRebuild(GetLevel());
	}
}

void ABrush::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	if ( GIsEditor )
	{
		OnBrushRegistered.Broadcast(this);
	}
}
#endif

bool ABrush::IsLevelBoundsRelevant() const
{
	// exclude default brush
	ULevel* Level = GetLevel();
	return (Level && this != Level->Actors[1].Get());
}

void ABrush::RebuildNavigationData()
{
	// empty in base class
}

FColor ABrush::GetWireColor() const
{
	FColor Color = GEngine->C_BrushWire;

	if( IsStaticBrush() )
	{
		Color = bColored ?						BrushColor :
				BrushType == Brush_Subtract ?	GEngine->C_SubtractWire :
				BrushType != Brush_Add ?		GEngine->C_BrushWire :
				(PolyFlags & PF_Portal) ?		GEngine->C_SemiSolidWire :
				(PolyFlags & PF_NotSolid) ?		GEngine->C_NonSolidWire :
				(PolyFlags & PF_Semisolid) ?	GEngine->C_ScaleBoxHi :
												GEngine->C_AddWire;
	}
	else if( IsVolumeBrush() )
	{
		Color = bColored ? BrushColor : GEngine->C_Volume;
	}
	else if( IsBrushShape() )
	{
		Color = bColored ? BrushColor : GEngine->C_BrushShape;
	}

	return Color;
}

bool ABrush::IsStaticBrush() const
{
	return BrushComponent && (BrushComponent->Mobility == EComponentMobility::Static);
}

#if WITH_EDITOR
bool ABrush::Modify(bool bAlwaysMarkDirty)
{
	bool bSavedToTransactionBuffer = Super::Modify(bAlwaysMarkDirty);

	if(Brush)
	{
		bSavedToTransactionBuffer = Brush->Modify(bAlwaysMarkDirty) || bSavedToTransactionBuffer;
	}
	return bSavedToTransactionBuffer;
}
#endif
