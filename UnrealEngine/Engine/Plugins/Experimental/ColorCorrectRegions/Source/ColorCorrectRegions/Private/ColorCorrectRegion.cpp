// Copyright Epic Games, Inc. All Rights Reserved.

#include "ColorCorrectRegion.h"
#include "Async/Async.h"
#include "ColorCorrectRegionsModule.h"
#include "ColorCorrectRegionsSubsystem.h"
#include "ColorCorrectWindow.h"
#include "Components/BillboardComponent.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "Engine/CollisionProfile.h"
#include "Engine/GameEngine.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"

#if WITH_EDITOR
#include "IDisplayClusterLightCardExtenderModule.h"
#endif

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

	IdentityComponent = CreateDefaultSubobject<UColorCorrectionInvisibleComponent>("IdentityComponent");
	IdentityComponent->SetupAttachment(RootComponent);
	IdentityComponent->CastShadow = false;
	IdentityComponent->SetHiddenInGame(false);

#if WITH_METADATA
	if (!Cast<AColorCorrectionWindow>(this))
	{
		CreateIcon();
	}
#endif

#if WITH_EDITOR
	if (!IsTemplate())
	{
		IDisplayClusterLightCardExtenderModule& LightCardExtenderModule = IDisplayClusterLightCardExtenderModule::Get();
		LightCardExtenderModule.GetOnSequencerTimeChanged().AddUObject(this, &AColorCorrectRegion::OnSequencerTimeChanged);
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

	if (ColorCorrectRegionsSubsystem.IsValid())
	{
		ColorCorrectRegionsSubsystem->OnActorSpawned(this);
	}
}

void AColorCorrectRegion::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ColorCorrectRegionsSubsystem.IsValid())
	{
		ColorCorrectRegionsSubsystem->OnActorDeleted(this, false);
		ColorCorrectRegionsSubsystem = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AColorCorrectRegion::BeginDestroy()
{
	if (ColorCorrectRegionsSubsystem.IsValid())
	{
		ColorCorrectRegionsSubsystem->OnActorDeleted(this, true);
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

	TransferState();


	// Check to make sure that no ids have been changed externally.
	{
		TimeWaited += DeltaTime;
		const float WaitTimeInSecs = 1.0;

		if (!ColorCorrectRegionsSubsystem.IsValid())
		{
			if (const UWorld* World = GetWorld())
			{
				ColorCorrectRegionsSubsystem = World->GetSubsystem<UColorCorrectRegionsSubsystem>();
			}
		}
		
		if (ColorCorrectRegionsSubsystem.IsValid() && TimeWaited >= WaitTimeInSecs)
		{
			ColorCorrectRegionsSubsystem->CheckAssignedActorsValidity(this);
			TimeWaited = 0;
		}

	}
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
		&& !(GetWorld()->HasBegunPlay() && IsHidden());

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

	GetActorBounds(false, TempCCRStateRenderThread->BoxOrigin, TempCCRStateRenderThread->BoxExtent);
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

	// Store component id to be used on render thread.
	if (!(TempCCRStateRenderThread->FirstPrimitiveId == IdentityComponent->GetPrimitiveSceneId()))
	{
		TempCCRStateRenderThread->FirstPrimitiveId = IdentityComponent->GetPrimitiveSceneId();
	}

	{
		ENQUEUE_RENDER_COMMAND(CopyCCProxy)([this, CCRStateToCopy = MoveTemp(TempCCRStateRenderThread)](FRHICommandListImmediate& RHICmdList)
			{
				ColorCorrectRenderProxy = CCRStateToCopy;
			}
		);
	}

}

#if WITH_EDITOR
void AColorCorrectRegion::OnSequencerTimeChanged(TWeakPtr<ISequencer> InSequencer)
{
	bNotifyOnParamSetter = false;
	UpdatePositionalParamsFromTransform();
	bNotifyOnParamSetter = true;
}
#endif

void AColorCorrectRegion::HandleAffectedActorsPropertyChange(uint32 ActorListChangeType)
{
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
		if (ColorCorrectRegionsSubsystem.IsValid())
		{
			ColorCorrectRegionsSubsystem->AssignStencilIdsToPerActorCC(this);
		}
	}

	if (ActorListChangeType == EPropertyChangeType::ArrayClear
		|| ActorListChangeType == EPropertyChangeType::ArrayRemove
		|| ActorListChangeType == EPropertyChangeType::ValueSet)
	{
		if (ColorCorrectRegionsSubsystem.IsValid())
		{
			ColorCorrectRegionsSubsystem->ClearStencilIdsToPerActorCC(this);
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

AColorCorrectRegion::~AColorCorrectRegion()
{
#if WITH_EDITOR
	if (!IsTemplate())
	{
		IDisplayClusterLightCardExtenderModule& LightCardExtenderModule = IDisplayClusterLightCardExtenderModule::Get();
		LightCardExtenderModule.GetOnSequencerTimeChanged().RemoveAll(this);
	}
#endif
}

#if WITH_EDITOR
void AColorCorrectRegion::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (!ColorCorrectRegionsSubsystem.IsValid())
	{
		if (const UWorld* World = GetWorld())
		{
			ColorCorrectRegionsSubsystem = World->GetSubsystem<UColorCorrectRegionsSubsystem>();
		}
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(AColorCorrectRegion, AffectedActors))
	{
		/** Since there might be Dialogs involved we need to run this on game thread. */
		AsyncTask(ENamedThreads::GameThread, [this, ActorListChangeType = PropertyChangedEvent.ChangeType]() 
		{
			HandleAffectedActorsPropertyChange(ActorListChangeType);
		});
	}

	// Reorder all CCRs after the Priority property has changed.
	// Also, in context of Multi-User: PropertyChangedEvent can be a stub without the actual property data. 
	// Therefore we need to refresh priority if PropertyChangedEvent.Property is nullptr. 
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AColorCorrectRegion, Priority) || PropertyChangedEvent.Property == nullptr)
	{
		if (ColorCorrectRegionsSubsystem.IsValid())
		{
			ColorCorrectRegionsSubsystem->SortRegionsByPriority();
		}
	}

	// Stage actor properties
	{
		const FStructProperty* StructProperty = CastField<FStructProperty>(PropertyChangedEvent.MemberProperty);
		const bool bIsOrientation = StructProperty ? StructProperty->Struct == FDisplayClusterPositionalParams::StaticStruct() : false;
	
		if (bIsOrientation)
		{
			UpdateStageActorTransform();
			// Updates MU in real-time. Skip our method as the positional coordinates are already correct.
			AActor::PostEditMove(PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive);
		}
		else if (
			PropertyName == USceneComponent::GetRelativeLocationPropertyName() ||
			PropertyName == USceneComponent::GetRelativeRotationPropertyName() ||
			PropertyName == USceneComponent::GetRelativeScale3DPropertyName())
		{
			bNotifyOnParamSetter = false;
			UpdatePositionalParamsFromTransform();
			bNotifyOnParamSetter = true;
		}
	}

	// Call after stage actor transform is updated, so any observers will have both the correct actor transform and
	// positional properties.
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void AColorCorrectRegion::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	bNotifyOnParamSetter = false;
	UpdatePositionalParamsFromTransform();
	bNotifyOnParamSetter = true;
}

void AColorCorrectRegion::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	Super::PostTransacted(TransactionEvent);
	FixMeshComponentReferences();
}
#endif //WITH_EDITOR

#define NOTIFY_PARAM_SETTER()\
	if (bNotifyOnParamSetter)\
	{\
		UpdateStageActorTransform();\
	}\

void AColorCorrectRegion::SetLongitude(double InValue)
{
	PositionalParams.Longitude = InValue;
	NOTIFY_PARAM_SETTER()
}

double AColorCorrectRegion::GetLongitude() const
{
	return PositionalParams.Longitude;
}

void AColorCorrectRegion::SetLatitude(double InValue)
{
	PositionalParams.Latitude = InValue;
	NOTIFY_PARAM_SETTER()
}

double AColorCorrectRegion::GetLatitude() const
{
	return PositionalParams.Latitude;
}

void AColorCorrectRegion::SetDistanceFromCenter(double InValue)
{
	PositionalParams.DistanceFromCenter = InValue;
	NOTIFY_PARAM_SETTER()
}

double AColorCorrectRegion::GetDistanceFromCenter() const
{
	return PositionalParams.DistanceFromCenter;
}

void AColorCorrectRegion::SetSpin(double InValue)
{
	PositionalParams.Spin = InValue;
	NOTIFY_PARAM_SETTER()
}

double AColorCorrectRegion::GetSpin() const
{
	return PositionalParams.Spin;
}

void AColorCorrectRegion::SetPitch(double InValue)
{
	PositionalParams.Pitch = InValue;
	NOTIFY_PARAM_SETTER()
}

double AColorCorrectRegion::GetPitch() const
{
	return PositionalParams.Pitch;
}

void AColorCorrectRegion::SetYaw(double InValue)
{
	PositionalParams.Yaw = InValue;
	NOTIFY_PARAM_SETTER()
}

double AColorCorrectRegion::GetYaw() const
{
	return PositionalParams.Yaw;
}

void AColorCorrectRegion::SetRadialOffset(double InValue)
{
	PositionalParams.RadialOffset = InValue;
	NOTIFY_PARAM_SETTER()
}

double AColorCorrectRegion::GetRadialOffset() const
{
	return PositionalParams.RadialOffset;
}

void AColorCorrectRegion::SetScale(const FVector2D& InScale)
{
	PositionalParams.Scale = InScale;
	NOTIFY_PARAM_SETTER()
}

FVector2D AColorCorrectRegion::GetScale() const
{
	return PositionalParams.Scale;
}

void AColorCorrectRegion::SetOrigin(const FTransform& InOrigin)
{
	Origin = InOrigin;
}

FTransform AColorCorrectRegion::GetOrigin() const
{
	return Origin;
}

void AColorCorrectRegion::SetPositionalParams(const FDisplayClusterPositionalParams& InParams)
{
	PositionalParams = InParams;
	NOTIFY_PARAM_SETTER()
}

FDisplayClusterPositionalParams AColorCorrectRegion::GetPositionalParams() const
{
	return PositionalParams;
}

void AColorCorrectRegion::GetPositionalProperties(FPositionalPropertyArray& OutPropertyPairs) const
{
	void* Container = (void*)(&PositionalParams);

	const TSet<FName>& PropertyNames = GetPositionalPropertyNames();
	OutPropertyPairs.Reserve(PropertyNames.Num());

	for (const FName& PropertyName : PropertyNames)
	{
		if (FProperty* Property = FindFProperty<FProperty>(FDisplayClusterPositionalParams::StaticStruct(), PropertyName))
		{
			OutPropertyPairs.Emplace(Container, Property);
		}
	}

	if (FStructProperty* ParamsProperty = FindFProperty<FStructProperty>(GetClass(), GET_MEMBER_NAME_CHECKED(AColorCorrectRegion, PositionalParams)))
	{
		OutPropertyPairs.Emplace((void*)this, ParamsProperty);
	}
}

FName AColorCorrectRegion::GetPositionalPropertiesMemberName() const
{
	return GET_MEMBER_NAME_CHECKED(AColorCorrectRegion, PositionalParams);
}

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
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		MeshComponent->CastShadow = false;
		MeshComponent->SetHiddenInGame(true);
	}
	ChangeShapeVisibilityForActorType();
}

void AColorCorrectionRegion::ChangeShapeVisibilityForActorType()
{
	ChangeShapeVisibilityForActorTypeInternal<EColorCorrectRegionsType>(Type);
}

#if WITH_EDITOR
void AColorCorrectionRegion::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(AColorCorrectRegion, Type) || PropertyChangedEvent.Property == nullptr)
	{
		ChangeShapeVisibilityForActorType();
	}
}

FName AColorCorrectionRegion::GetCustomIconName() const
{
	return TEXT("CCR.OutlinerThumbnail");
}

void AColorCorrectionRegion::FixMeshComponentReferences()
{
	FixMeshComponentReferencesInternal<EColorCorrectRegionsType>(Type);
}
#endif

#undef NOTIFY_PARAM_SETTER
