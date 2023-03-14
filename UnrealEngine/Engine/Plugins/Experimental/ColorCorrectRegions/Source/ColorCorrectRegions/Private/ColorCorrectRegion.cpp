// Copyright Epic Games, Inc. All Rights Reserved.

#include "ColorCorrectRegion.h"
#include "ColorCorrectWindow.h"
#include "Components/BillboardComponent.h"
#include "ColorCorrectRegionsSubsystem.h"
#include "ColorCorrectRegionsModule.h"
#include "UObject/ConstructorHelpers.h"
#include "CoreMinimal.h"
#include "Engine/GameEngine.h"
#include "Engine/Classes/Components/MeshComponent.h"
#include "Materials/Material.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Texture2D.h"

ENUM_RANGE_BY_COUNT(EColorCorrectRegionsType, EColorCorrectRegionsType::MAX)


AColorCorrectRegion::AColorCorrectRegion(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Type(EColorCorrectRegionsType::Sphere)
	, Priority(0)
	, Intensity(1.0)
	, Inner(0.5)
	, Outer(1.0)
	, Falloff(1.0)
	, Invert(false)
	, TemperatureType(EColorCorrectRegionTemperatureType::ColorTemperature)
	, Temperature(6500)
	, Tint(0)
	, Enabled(true)
	, bEnablePerActorCC(false)
	, PerActorColorCorrection(EColorCorrectRegionStencilType::IncludeStencil)
	, ColorCorrectRegionsSubsystem(nullptr)
	, ColorCorrectRenderProxy(MakeShared<FColorCorrectRenderProxy>())
{
	PrimaryActorTick.bCanEverTick = true;

	// Add a scene component as our root
	RootComponent = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);

#if WITH_METADATA
	if (!Cast<AColorCorrectionWindow>(this))
	{
		CreateIcon();
	}
#endif
}

void AColorCorrectRegion::BeginPlay()
{	
	Super::BeginPlay();
	if (const UWorld* World = GetWorld())
	{
		ColorCorrectRegionsSubsystem = World->GetSubsystem<UColorCorrectRegionsSubsystem>();
	}

	if (ColorCorrectRegionsSubsystem)
	{
		ColorCorrectRegionsSubsystem->OnActorSpawned(this);
	}
}

void AColorCorrectRegion::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ColorCorrectRegionsSubsystem)
	{
		ColorCorrectRegionsSubsystem->OnActorDeleted(this);
		ColorCorrectRegionsSubsystem = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AColorCorrectRegion::BeginDestroy()
{
	if (ColorCorrectRegionsSubsystem)
	{
		ColorCorrectRegionsSubsystem->OnActorDeleted(this);
		ColorCorrectRegionsSubsystem = nullptr;
	}
	
	Super::BeginDestroy();
}

bool AColorCorrectRegion::ShouldTickIfViewportsOnly() const
{
	return true;
}

void AColorCorrectRegion::TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	Super::Tick(DeltaTime);

	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("CCR.TickActor %s"), *GetName()));

	HandleAffectedActorsPropertyChange();
	TransferState();


	// Check to make sure that no ids have been changed externally.
	{
		TimeWaited += DeltaTime;
		const float WaitTimeInSecs = 1.0;

		if (!ColorCorrectRegionsSubsystem)
		{
			if (const UWorld* World = GetWorld())
			{
				ColorCorrectRegionsSubsystem = World->GetSubsystem<UColorCorrectRegionsSubsystem>();
			}
		}
		
		if (ColorCorrectRegionsSubsystem && TimeWaited >= WaitTimeInSecs)
		{
			ColorCorrectRegionsSubsystem->CheckAssignedActorsValidity(this);
			TimeWaited = 0;
		}

	}
}

void AColorCorrectRegion::Cleanup()
{
	ColorCorrectRegionsSubsystem = nullptr;
}

void AColorCorrectRegion::TransferState()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("CCR.TransferState %s"), *GetName()));

	FColorCorrectRenderProxyPtr TempCCRStateRenderThread = MakeShared<FColorCorrectRenderProxy>();
	if (AColorCorrectionWindow* CCWindow = Cast<AColorCorrectionWindow>(this))
	{
		TempCCRStateRenderThread->WindowType = CCWindow->WindowType;
	}
	else
	{
		TempCCRStateRenderThread->Type = Type;
	}

	TempCCRStateRenderThread->bIsActiveThisFrame = IsValid(this)
		&& Enabled
#if WITH_EDITOR
		&& !IsHiddenEd()
#endif 
		&& !IsHidden();

	TempCCRStateRenderThread->World = GetWorld();
	TempCCRStateRenderThread->Priority = Priority;
	TempCCRStateRenderThread->Intensity = Intensity;

	// Inner could be larger than outer, in which case we need to make sure these are swapped.
	TempCCRStateRenderThread->Inner = FMath::Min<float>(Outer, Inner);
	TempCCRStateRenderThread->Outer = FMath::Max<float>(Outer, Inner);

	if (TempCCRStateRenderThread->Inner == TempCCRStateRenderThread->Outer)
	{
		TempCCRStateRenderThread->Inner -= 0.0001;
	}

	TempCCRStateRenderThread->Falloff = Falloff;
	TempCCRStateRenderThread->Invert = Invert;
	TempCCRStateRenderThread->TemperatureType = TemperatureType;
	TempCCRStateRenderThread->Temperature = Temperature;
	TempCCRStateRenderThread->Tint = Tint;
	TempCCRStateRenderThread->ColorGradingSettings = ColorGradingSettings;
	TempCCRStateRenderThread->bEnablePerActorCC = bEnablePerActorCC;
	TempCCRStateRenderThread->PerActorColorCorrection = PerActorColorCorrection;

	GetActorBounds(true, TempCCRStateRenderThread->BoxOrigin, TempCCRStateRenderThread->BoxExtent);
	TempCCRStateRenderThread->ActorLocation = (FVector3f)GetActorLocation();
	TempCCRStateRenderThread->ActorRotation = (FVector3f)GetActorRotation().Euler();
	TempCCRStateRenderThread->ActorScale = (FVector3f)GetActorScale();

	// Transfer Stencil Ids.
	{

		for (const TSoftObjectPtr<AActor>& StencilActor : AffectedActors)
		{
			if (!StencilActor.IsValid())
			{
				continue;
			}
			TArray<UPrimitiveComponent*> PrimitiveComponents;
			StencilActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
			for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
			{
				if (PrimitiveComponent->bRenderCustomDepth)
				{
					TempCCRStateRenderThread->StencilIds.Add(static_cast<uint32>(PrimitiveComponent->CustomDepthStencilValue));
				}
			}
		}
	}

	// Display Cluster uses HiddenPrimitives to hide Primitive components from view. 
	// Store component id to be used on render thread.
	if (const UStaticMeshComponent* FirstMeshComponent = FindComponentByClass<UStaticMeshComponent>())
	{
		if (!(TempCCRStateRenderThread->FirstPrimitiveId == FirstMeshComponent->ComponentId))
		{
			TempCCRStateRenderThread->FirstPrimitiveId = FirstMeshComponent->ComponentId;
		}
	}

	{
		ENQUEUE_RENDER_COMMAND(CopyCCProxy)([this, CCRStateToCopy = MoveTemp(TempCCRStateRenderThread)](FRHICommandListImmediate& RHICmdList)
			{
				ColorCorrectRenderProxy = CCRStateToCopy;
			}
		);
	}

}

void AColorCorrectRegion::HandleAffectedActorsPropertyChange()
{
	if (bActorListIsDirty)
	{
		bool bEventHandled = false;
		bActorListIsDirty = false;
		if (ActorListChangeType == EPropertyChangeType::ArrayAdd
			|| ActorListChangeType == EPropertyChangeType::ValueSet)
		{
			// In case user assigns Color Correct Region or Window, we should remove it as it is invalid operation.
			{
				TArray<TSoftObjectPtr<AActor>> ActorsToRemove;
				for (const TSoftObjectPtr<AActor>& StencilActor : AffectedActors)
				{
					if (AColorCorrectRegion* CCRCast = Cast<AColorCorrectRegion>(StencilActor.Get()))
					{
						ActorsToRemove.Add(StencilActor);
					}
				}
				if (ActorsToRemove.Num() > 0)
				{
					UE_LOG(ColorCorrectRegions, Warning, TEXT("Color Correct Region or Window assignment to Per Actor CC is not supported."));
				}
				for (const TSoftObjectPtr<AActor>& StencilActor : ActorsToRemove)
				{
					AffectedActors.Remove(StencilActor);
					AffectedActors.FindOrAdd(TSoftObjectPtr<AActor>());
				}
			}
			bEventHandled = true;
			if (ColorCorrectRegionsSubsystem)
			{
				ColorCorrectRegionsSubsystem->AssignStencilIdsToPerActorCC(this);
			}
		}

		if (ActorListChangeType == EPropertyChangeType::ArrayClear
			|| ActorListChangeType == EPropertyChangeType::ArrayRemove
			|| ActorListChangeType == EPropertyChangeType::ValueSet)
		{
			bEventHandled = true;
			if (ColorCorrectRegionsSubsystem)
			{
				ColorCorrectRegionsSubsystem->ClearStencilIdsToPerActorCC(this);
			}
		}
	}
}

#if WITH_METADATA
void AColorCorrectRegion::CreateIcon()
{
	// Create billboard component
	if (GIsEditor && !IsRunningCommandlet())
	{
		// Structure to hold one-time initialization

		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> SpriteTextureObject;
			FName ID_ColorCorrectRegion;
			FText NAME_ColorCorrectRegion;

			FConstructorStatics()
				: SpriteTextureObject(TEXT("/ColorCorrectRegions/Icons/S_ColorCorrectRegionIcon"))
				, ID_ColorCorrectRegion(TEXT("Color Correct Region"))
				, NAME_ColorCorrectRegion(NSLOCTEXT("SpriteCategory", "ColorCorrectRegion", "Color Correct Region"))
			{
			}
		};

		static FConstructorStatics ConstructorStatics;

		SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Color Correct Region Icon"));

		if (SpriteComponent)
		{
			SpriteComponent->Sprite = ConstructorStatics.SpriteTextureObject.Get();
			SpriteComponent->SpriteInfo.Category = ConstructorStatics.ID_ColorCorrectRegion;
			SpriteComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_ColorCorrectRegion;
			SpriteComponent->SetIsVisualizationComponent(true);
			SpriteComponent->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
			SpriteComponent->SetMobility(EComponentMobility::Movable);
			SpriteComponent->bHiddenInGame = true;
			SpriteComponent->bIsScreenSizeScaled = true;

			SpriteComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
		}
	}

}
#endif 

#if WITH_EDITOR
void AColorCorrectRegion::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (!ColorCorrectRegionsSubsystem)
	{
		if (const UWorld* World = GetWorld())
		{
			ColorCorrectRegionsSubsystem = World->GetSubsystem<UColorCorrectRegionsSubsystem>();
		}
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(AColorCorrectRegion, AffectedActors))
	{
		/** Since there might be Dialogs involved we need to dirty this CCR and handle the rest on Tick. */
		ActorListChangeType = PropertyChangedEvent.ChangeType;
		bActorListIsDirty = true;
	}

	// Reorder all CCRs after the Priority property has changed.
	// Also, in context of Multi-User: PropertyChangedEvent can be a stub without the actual property data. 
	// Therefore we need to refresh priority if PropertyChangedEvent.Property is nullptr. 
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AColorCorrectRegion, Priority) || PropertyChangedEvent.Property == nullptr)
	{
		if (ColorCorrectRegionsSubsystem)
		{
			ColorCorrectRegionsSubsystem->SortRegionsByPriority();
		}
	}
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AColorCorrectRegion, Type) || PropertyChangedEvent.Property == nullptr)
	{
		if (ColorCorrectRegionsSubsystem)
		{
			ColorCorrectRegionsSubsystem->OnLevelsChanged();
		}
	}
}
#endif //WITH_EDITOR


AColorCorrectionRegion::AColorCorrectionRegion(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UMaterial* Material = LoadObject<UMaterial>(NULL, TEXT("/ColorCorrectRegions/Materials/M_ColorCorrectRegionTransparentPreview.M_ColorCorrectRegionTransparentPreview"), NULL, LOAD_None, NULL);
	const TArray<UStaticMesh*> StaticMeshes =
	{
		Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), NULL, TEXT("/Engine/BasicShapes/Sphere"))),
		Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), NULL, TEXT("/Engine/BasicShapes/Cube"))),
		Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), NULL, TEXT("/Engine/BasicShapes/Cylinder"))),
		Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), NULL, TEXT("/Engine/BasicShapes/Cone")))
	};

	for (EColorCorrectRegionsType CCRType : TEnumRange<EColorCorrectRegionsType>())
	{
		UStaticMeshComponent* MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(*UEnum::GetValueAsString(CCRType));
		MeshComponents.Add(MeshComponent);
		MeshComponent->SetupAttachment(RootComponent);
		MeshComponent->SetStaticMesh(StaticMeshes[static_cast<uint8>(CCRType)]);
		MeshComponent->SetMaterial(0, Material);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		MeshComponent->SetCollisionProfileName(TEXT("OverlapAll"));
		MeshComponent->CastShadow = false;
		MeshComponent->SetHiddenInGame(true);
	}

	SetMeshVisibilityForRegionType();

}

void AColorCorrectionRegion::SetMeshVisibilityForRegionType()
{
	for (EColorCorrectRegionsType CCRType : TEnumRange<EColorCorrectRegionsType>())
	{
		uint8 TypeIndex = static_cast<uint8>(CCRType);

		if (CCRType == Type)
		{
			MeshComponents[TypeIndex]->SetVisibility(true, true);
		}
		else
		{
			MeshComponents[TypeIndex]->SetVisibility(false, true);
		}
	}
}

#if WITH_EDITOR
void AColorCorrectionRegion::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(AColorCorrectionRegion, Type))
	{
		SetMeshVisibilityForRegionType();
	}
}
FName AColorCorrectionRegion::GetCustomIconName() const
{
	return TEXT("CCR.OutlinerThumbnail");
}
#endif
