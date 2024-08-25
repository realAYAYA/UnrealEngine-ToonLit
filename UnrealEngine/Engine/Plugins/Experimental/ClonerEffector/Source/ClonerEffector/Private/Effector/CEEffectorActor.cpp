// Copyright Epic Games, Inc. All Rights Reserved.

#include "Effector/CEEffectorActor.h"

#include "Cloner/CEClonerActor.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Effector/CEEffectorComponent.h"
#include "Math/Vector.h"
#include "Subsystems/CEEffectorSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogCEEffectorActor, Log, All);

ACEEffectorActor::FOnEffectorIdentifierChanged ACEEffectorActor::OnEffectorRefreshClonerDelegate;

ACEEffectorActor::ACEEffectorActor()
{
	SetCanBeDamaged(false);
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* SceneComponent = CreateDefaultSubobject<UCEEffectorComponent>(TEXT("AvaEffectorComponent"));
#if WITH_EDITORONLY_DATA
	SceneComponent->bVisualizeComponent = true;
#endif
	SetRootComponent(SceneComponent);

	// Sphere
	InnerSphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("AvaInnerSphereComponent"));;
	InnerSphereComponent->ShapeColor = FColor::Blue;
	InnerSphereComponent->SetLineThickness(VisualizerThickness);
	InnerSphereComponent->SetSphereRadius(InnerRadius);
	InnerSphereComponent->SetHiddenInGame(true);
#if WITH_EDITOR
	// Do not show bounding box around cloner for better visibility
	InnerSphereComponent->SetIsVisualizationComponent(true);
#endif
	InnerSphereComponent->bIsEditorOnly = false;
	InnerSphereComponent->SetupAttachment(SceneComponent);

	OuterSphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("AvaOuterSphereComponent"));
	OuterSphereComponent->ShapeColor = FColor::Red;
	OuterSphereComponent->SetLineThickness(VisualizerThickness);
	OuterSphereComponent->SetSphereRadius(OuterRadius);
	OuterSphereComponent->SetHiddenInGame(true);
#if WITH_EDITOR
	// Do not show bounding box around cloner for better visibility
	OuterSphereComponent->SetIsVisualizationComponent(true);
#endif
	OuterSphereComponent->bIsEditorOnly = false;
	OuterSphereComponent->SetupAttachment(SceneComponent);

	// Box
	InnerBoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("AvaInnerBoxComponent"));
	InnerBoxComponent->ShapeColor = FColor::Blue;
	InnerBoxComponent->SetLineThickness(VisualizerThickness);
	InnerBoxComponent->SetBoxExtent(InnerExtent);
	InnerBoxComponent->SetHiddenInGame(true);
#if WITH_EDITOR
	// Do not show bounding box around cloner for better visibility
	InnerBoxComponent->SetIsVisualizationComponent(true);
#endif
	InnerBoxComponent->bIsEditorOnly = false;
	InnerBoxComponent->SetupAttachment(SceneComponent);

	OuterBoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("AvaOuterBoxComponent"));
	OuterBoxComponent->ShapeColor = FColor::Red;
	OuterBoxComponent->SetLineThickness(VisualizerThickness);
	OuterBoxComponent->SetBoxExtent(OuterExtent);
	OuterBoxComponent->SetHiddenInGame(true);
#if WITH_EDITOR
	// Do not show bounding box around cloner for better visibility
	OuterBoxComponent->SetIsVisualizationComponent(true);
#endif
	OuterBoxComponent->bIsEditorOnly = false;
	OuterBoxComponent->SetupAttachment(SceneComponent);

	// Plane
	InnerPlaneComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("AvaInnerPlaneComponent"));
	InnerPlaneComponent->ShapeColor = FColor::Blue;
	InnerPlaneComponent->SetLineThickness(VisualizerThickness);
	InnerPlaneComponent->SetBoxExtent(InnerExtent);
	InnerPlaneComponent->SetHiddenInGame(true);
#if WITH_EDITOR
	// Do not show bounding box around cloner for better visibility
	InnerPlaneComponent->SetIsVisualizationComponent(true);
#endif
	InnerPlaneComponent->bIsEditorOnly = false;
	InnerPlaneComponent->SetupAttachment(SceneComponent);

	OuterPlaneComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("AvaOuterPlaneComponent"));
	OuterPlaneComponent->ShapeColor = FColor::Red;
	OuterPlaneComponent->SetLineThickness(VisualizerThickness);
	OuterPlaneComponent->SetBoxExtent(OuterExtent);
	OuterPlaneComponent->SetHiddenInGame(true);
#if WITH_EDITOR
	// Do not show bounding box around cloner for better visibility
	OuterPlaneComponent->SetIsVisualizationComponent(true);
#endif
	OuterPlaneComponent->bIsEditorOnly = false;
	OuterPlaneComponent->SetupAttachment(SceneComponent);

	if (!IsTemplate())
	{
		SceneComponent->TransformUpdated.AddUObject(this, &ACEEffectorActor::OnEffectorTransformed);
	}
}

#if WITH_EDITOR
FString ACEEffectorActor::GetDefaultActorLabel() const
{
	return DefaultLabel;
}

TCEPropertyChangeDispatcher<ACEEffectorActor> ACEEffectorActor::PropertyChangeDispatcher =
{
	/** Effector */
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, bEnabled), &ACEEffectorActor::OnEnabledChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, Magnitude), &ACEEffectorActor::OnMagnitudeChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, VisualizerThickness), &ACEEffectorActor::OnVisualizerThicknessChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, bVisualizerSpriteVisible), &ACEEffectorActor::OnVisualizerSpriteVisibleChanged },
	/** Type */
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, Type), &ACEEffectorActor::OnTypeChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, Easing), &ACEEffectorActor::OnEasingChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, OuterRadius), &ACEEffectorActor::OnSphereChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, InnerRadius), &ACEEffectorActor::OnSphereChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, InnerExtent), &ACEEffectorActor::OnBoxChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, OuterExtent), &ACEEffectorActor::OnBoxChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, PlaneSpacing), &ACEEffectorActor::OnPlaneChanged },
	/** Mode */
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, Mode), &ACEEffectorActor::OnModeChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, Offset), &ACEEffectorActor::OnTransformOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, Rotation), &ACEEffectorActor::OnTransformOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, Scale), &ACEEffectorActor::OnTransformOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, TargetActorWeak), &ACEEffectorActor::OnTargetActorChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, LocationStrength), &ACEEffectorActor::OnNoiseFieldOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, RotationStrength), &ACEEffectorActor::OnNoiseFieldOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, ScaleStrength), &ACEEffectorActor::OnNoiseFieldOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, Pan), &ACEEffectorActor::OnNoiseFieldOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, Frequency), &ACEEffectorActor::OnNoiseFieldOptionsChanged },
	/** Force */
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, bOrientationForceEnabled), &ACEEffectorActor::OnForceEnabledChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, OrientationForceRate), &ACEEffectorActor::OnForceOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, OrientationForceMin), &ACEEffectorActor::OnForceOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, OrientationForceMax), &ACEEffectorActor::OnForceOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, bVortexForceEnabled), &ACEEffectorActor::OnForceEnabledChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, VortexForceAmount), &ACEEffectorActor::OnForceOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, VortexForceAxis), &ACEEffectorActor::OnForceOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, bCurlNoiseForceEnabled), &ACEEffectorActor::OnForceEnabledChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, CurlNoiseForceStrength), &ACEEffectorActor::OnForceOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, CurlNoiseForceFrequency), &ACEEffectorActor::OnForceOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, bAttractionForceEnabled), &ACEEffectorActor::OnForceEnabledChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, AttractionForceStrength), &ACEEffectorActor::OnForceOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, AttractionForceFalloff), &ACEEffectorActor::OnForceOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, bGravityForceEnabled), &ACEEffectorActor::OnForceEnabledChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, GravityForceAcceleration), &ACEEffectorActor::OnForceOptionsChanged },
};

void ACEEffectorActor::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	PropertyChangeDispatcher.OnPropertyChanged(this, InPropertyChangedEvent);
}

void ACEEffectorActor::PostEditImport()
{
	Super::PostEditImport();

	OnEffectorChanged();
}

void ACEEffectorActor::PostEditUndo()
{
	Super::PostEditUndo();

	OnEffectorChanged();
}
#endif

void ACEEffectorActor::Destroyed()
{
	Super::Destroyed();

	// Remove this effector from the effector channel
	if (UCEEffectorSubsystem* EffectorSubsystem = UCEEffectorSubsystem::Get(GetWorld()))
	{
		EffectorSubsystem->UnregisterChannelEffector(this);
	}
}

void ACEEffectorActor::PostActorCreated()
{
	Super::PostActorCreated();

	OnEffectorChanged();
}

void ACEEffectorActor::PostLoad()
{
	Super::PostLoad();

	if (!InternalCloners.IsEmpty())
	{
		for (const TWeakObjectPtr<ACEClonerActor>& ClonerWeak : InternalCloners)
		{
			if (ACEClonerActor* Cloner = ClonerWeak.Get())
			{
				Cloner->LinkEffector(this);
			}
		}

		InternalCloners.Empty();
	}

	OnEffectorChanged();
}

void ACEEffectorActor::PostDuplicate(EDuplicateMode::Type InDuplicateMode)
{
	Super::PostDuplicate(InDuplicateMode);

	OnEffectorChanged();
}

int32 ACEEffectorActor::GetChannelIdentifier() const
{
	const int32 Identifier = ChannelData.GetIdentifier();

	if (Identifier == INDEX_NONE)
	{
		UCEEffectorSubsystem* EffectorSubsystem = !!GetWorld() ? UCEEffectorSubsystem::Get(GetWorld()) : nullptr;

		// Register this effector to the effector channel
		if (EffectorSubsystem)
		{
			EffectorSubsystem->RegisterChannelEffector(const_cast<ACEEffectorActor*>(this));
		}
	}

	return ChannelData.GetIdentifier();
}

FCEClonerEffectorChannelData& ACEEffectorActor::GetChannelData()
{
	return ChannelData;
}

void ACEEffectorActor::SetType(ECEClonerEffectorType InType)
{
	if (InType == Type)
	{
		return;
	}

	Type = InType;
	OnTypeChanged();
}

void ACEEffectorActor::SetEasing(ECEClonerEasing InEasing)
{
	if (InEasing == Easing)
	{
		return;
	}

	Easing = InEasing;
	OnEasingChanged();
}

void ACEEffectorActor::SetMagnitude(float InMagnitude)
{
	if (FMath::IsNearlyEqual(InMagnitude, Magnitude))
	{
		return;
	}

	if (InMagnitude < 0.f || InMagnitude > 1.f)
	{
		return;
	}

	Magnitude = InMagnitude;
	OnMagnitudeChanged();
}

void ACEEffectorActor::SetOuterRadius(float InRadius)
{
	if (FMath::IsNearlyEqual(InRadius, OuterRadius))
	{
		return;
	}

	if (InRadius < 0.f || InRadius < InnerRadius)
	{
		return;
	}

	OuterRadius = InRadius;
	OnSphereChanged();
}

void ACEEffectorActor::SetInnerRadius(float InRadius)
{
	if (FMath::IsNearlyEqual(InRadius, InnerRadius))
	{
		return;
	}

	if (InRadius < 0.f || InRadius > OuterRadius)
	{
		return;
	}

	InnerRadius = InRadius;
	OnSphereChanged();
}

void ACEEffectorActor::SetInnerExtent(const FVector& InExtent)
{
	if (InExtent.Equals(InnerExtent))
	{
		return;
	}

	if (InExtent.X < 0 || InExtent.Y < 0 || InExtent.Z < 0)
	{
		return;
	}

	InnerExtent = InExtent;
	OnBoxChanged();
}

void ACEEffectorActor::SetOuterExtent(const FVector& InExtent)
{
	if (InExtent.Equals(OuterExtent))
	{
		return;
	}

	if (InExtent.X < 0 || InExtent.Y < 0 || InExtent.Z < 0)
	{
		return;
	}

	OuterExtent = InExtent;
	OnBoxChanged();
}

void ACEEffectorActor::SetPlaneSpacing(float InSpacing)
{
	if (FMath::IsNearlyEqual(InSpacing, PlaneSpacing))
	{
		return;
	}

	if (InSpacing < 0.f)
	{
		return;
	}

	PlaneSpacing = InSpacing;
	OnPlaneChanged();
}

void ACEEffectorActor::SetOffset(const FVector& InOffset)
{
	if (InOffset.Equals(Offset))
	{
		return;
	}

	Offset = InOffset;
	OnTransformOptionsChanged();
}

void ACEEffectorActor::SetRotation(const FRotator& InRotation)
{
	if (InRotation.Equals(Rotation))
	{
		return;
	}

	constexpr float MinRotation = -180.f;
	if (InRotation.Pitch < MinRotation || InRotation.Roll < MinRotation || InRotation.Yaw < MinRotation)
	{
		return;
	}

	constexpr float MaxRotation = 180.f;
	if (InRotation.Pitch > MaxRotation || InRotation.Roll > MaxRotation || InRotation.Yaw > MaxRotation)
	{
		return;
	}

	Rotation = InRotation;
	OnTransformOptionsChanged();
}

void ACEEffectorActor::SetScale(const FVector& InScale)
{
	if (InScale.Equals(Scale))
	{
		return;
	}

	if (InScale.X < 0.f || InScale.Y < 0.f || InScale.Z < 0.f)
	{
		return;
	}

	Scale = InScale;
	OnTransformOptionsChanged();
}

void ACEEffectorActor::SetEnabled(bool bInEnable)
{
	if (bInEnable == bEnabled)
	{
		return;
	}

	bEnabled = bInEnable;
	OnEnabledChanged();
}

void ACEEffectorActor::SetVisualizerThickness(float InThickness)
{
	if (FMath::IsNearlyEqual(VisualizerThickness, InThickness))
	{
		return;
	}

	if (VisualizerThickness < 0.1f || VisualizerThickness > 10.f)
	{
		return;
	}

	VisualizerThickness = InThickness;
	OnVisualizerThicknessChanged();
}

void ACEEffectorActor::SetMode(ECEClonerEffectorMode InMode)
{
	if (Mode == InMode)
	{
		return;
	}

	Mode = InMode;
	OnModeChanged();
}

void ACEEffectorActor::SetTargetActor(AActor* InTargetActor)
{
	if (InTargetActor == TargetActorWeak.Get())
	{
		return;
	}

	TargetActorWeak = InTargetActor;
	OnTargetActorChanged();
}

void ACEEffectorActor::SetAttractionForceStrength(float InForceStrength)
{
	if (FMath::IsNearlyEqual(AttractionForceStrength, InForceStrength))
	{
		return;
	}

	AttractionForceStrength = InForceStrength;
	OnForceOptionsChanged();
}

void ACEEffectorActor::SetAttractionForceFalloff(float InForceFalloff)
{
	if (FMath::IsNearlyEqual(AttractionForceFalloff, InForceFalloff))
	{
		return;
	}

	AttractionForceFalloff = InForceFalloff;
	OnForceOptionsChanged();
}

void ACEEffectorActor::SetGravityForceEnabled(bool bInForceEnabled)
{
	if (bGravityForceEnabled == bInForceEnabled)
	{
		return;
	}

	bGravityForceEnabled = bInForceEnabled;
	OnForceEnabledChanged();
}

void ACEEffectorActor::SetGravityForceAcceleration(const FVector& InAcceleration)
{
	if (GravityForceAcceleration.Equals(InAcceleration))
	{
		return;
	}

	GravityForceAcceleration = InAcceleration;
	OnForceOptionsChanged();
}

void ACEEffectorActor::SetLocationStrength(const FVector& InStrength)
{
	if (LocationStrength.Equals(InStrength))
	{
		return;
	}

	LocationStrength = InStrength;
	OnNoiseFieldOptionsChanged();
}

void ACEEffectorActor::SetRotationStrength(const FRotator& InStrength)
{
	if (RotationStrength.Equals(InStrength))
	{
		return;
	}

	RotationStrength = InStrength;
	OnNoiseFieldOptionsChanged();
}

void ACEEffectorActor::SetScaleStrength(const FVector& InStrength)
{
	if (ScaleStrength.Equals(InStrength))
	{
		return;
	}

	ScaleStrength = InStrength;
	OnNoiseFieldOptionsChanged();
}

void ACEEffectorActor::SetPan(const FVector& InPan)
{
	if (Pan.Equals(InPan))
	{
		return;
	}

	Pan = InPan;
	OnNoiseFieldOptionsChanged();
}

void ACEEffectorActor::SetFrequency(float InFrequency)
{
	if (FMath::IsNearlyEqual(Frequency, InFrequency))
	{
		return;
	}

	if (InFrequency < 0.f)
	{
		return;
	}

	Frequency = InFrequency;
	OnNoiseFieldOptionsChanged();
}

void ACEEffectorActor::SetOrientationForceEnabled(bool bInForceEnabled)
{
	if (bOrientationForceEnabled == bInForceEnabled)
	{
		return;
	}

	bOrientationForceEnabled = bInForceEnabled;
	OnForceEnabledChanged();
}

void ACEEffectorActor::SetOrientationForceRate(float InForceOrientationRate)
{
	if (FMath::IsNearlyEqual(OrientationForceRate, InForceOrientationRate))
	{
		return;
	}

	OrientationForceRate = InForceOrientationRate;
	OnForceOptionsChanged();
}

void ACEEffectorActor::SetOrientationForceMin(const FVector& InForceOrientationMin)
{
	if (OrientationForceMin.Equals(InForceOrientationMin))
	{
		return;
	}

	OrientationForceMin = InForceOrientationMin;
	OnForceOptionsChanged();
}

void ACEEffectorActor::SetOrientationForceMax(const FVector& InForceOrientationMax)
{
	if (OrientationForceMax.Equals(InForceOrientationMax))
	{
		return;
	}

	OrientationForceMax = InForceOrientationMax;
	OnForceOptionsChanged();
}

void ACEEffectorActor::SetVortexForceEnabled(bool bInForceEnabled)
{
	if (bVortexForceEnabled == bInForceEnabled)
	{
		return;
	}

	bVortexForceEnabled = bInForceEnabled;
	OnForceEnabledChanged();
}

void ACEEffectorActor::SetVortexForceAmount(float InForceVortexAmount)
{
	if (FMath::IsNearlyEqual(VortexForceAmount, InForceVortexAmount))
	{
		return;
	}

	VortexForceAmount = InForceVortexAmount;
	OnForceOptionsChanged();
}

void ACEEffectorActor::SetVortexForceAxis(const FVector& InForceVortexAxis)
{
	if (VortexForceAxis.Equals(InForceVortexAxis))
	{
		return;
	}

	VortexForceAxis = InForceVortexAxis;
	OnForceOptionsChanged();
}

void ACEEffectorActor::SetCurlNoiseForceEnabled(bool bInForceEnabled)
{
	if (bCurlNoiseForceEnabled == bInForceEnabled)
	{
		return;
	}

	bCurlNoiseForceEnabled = bInForceEnabled;
	OnForceEnabledChanged();
}

void ACEEffectorActor::SetCurlNoiseForceStrength(float InForceCurlNoiseStrength)
{
	if (FMath::IsNearlyEqual(CurlNoiseForceStrength, InForceCurlNoiseStrength))
	{
		return;
	}

	CurlNoiseForceStrength = InForceCurlNoiseStrength;
	OnForceOptionsChanged();
}

void ACEEffectorActor::SetCurlNoiseForceFrequency(float InForceCurlNoiseFrequency)
{
	if (FMath::IsNearlyEqual(CurlNoiseForceFrequency, InForceCurlNoiseFrequency))
	{
		return;
	}

	CurlNoiseForceFrequency = InForceCurlNoiseFrequency;
	OnForceOptionsChanged();
}

void ACEEffectorActor::SetAttractionForceEnabled(bool bInForceEnabled)
{
	if (bAttractionForceEnabled == bInForceEnabled)
	{
		return;
	}

	bAttractionForceEnabled = bInForceEnabled;
	OnForceEnabledChanged();
}

#if WITH_EDITOR
void ACEEffectorActor::SetVisualizerSpriteVisible(bool bInVisible)
{
	if (bVisualizerSpriteVisible == bInVisible)
	{
		return;
	}

	bVisualizerSpriteVisible = bInVisible;
	OnVisualizerSpriteVisibleChanged();
}
#endif

void ACEEffectorActor::OnEffectorTransformed(USceneComponent* InUpdatedComponent, EUpdateTransformFlags InUpdateTransformFlags, ETeleportType InTeleport)
{
	OnTransformChanged();
	// Update when scaled or rotated
	OnSphereChanged();
	OnBoxChanged();
	OnPlaneChanged();
	// update if self
	const AActor* InternalTargetActor = InternalTargetActorWeak.Get();
	if (InternalTargetActor == this)
	{
		OnTargetOptionsChanged();
	}
}

void ACEEffectorActor::OnEnabledChanged()
{
	if (bEnabled)
	{
		OnEffectorChanged();
	}
	else // Disabled
	{
		OnEffectorDisabled();
		// Hide visualization components
		OnTypeChanged();
		// Editor
		OnVisualizerThicknessChanged();
		OnVisualizerSpriteVisibleChanged();
	}
}

void ACEEffectorActor::OnEffectorDisabled()
{
	OnMagnitudeChanged();
}

void ACEEffectorActor::OnModeChanged()
{
	ChannelData.Mode = Mode;

	OnTransformOptionsChanged();
	OnTargetActorChanged();
	OnNoiseFieldOptionsChanged();
}

void ACEEffectorActor::OnTypeChanged()
{
	// Sphere
	if (InnerSphereComponent && OuterSphereComponent)
	{
		InnerSphereComponent->SetVisibility(Type == ECEClonerEffectorType::Sphere);
		OuterSphereComponent->SetVisibility(Type == ECEClonerEffectorType::Sphere);
	}

	// Box
	if (InnerBoxComponent && OuterBoxComponent)
	{
		InnerBoxComponent->SetVisibility(Type == ECEClonerEffectorType::Box);
		OuterBoxComponent->SetVisibility(Type == ECEClonerEffectorType::Box);
	}

	// Plane
	if (InnerPlaneComponent && OuterPlaneComponent)
	{
		InnerPlaneComponent->SetVisibility(Type == ECEClonerEffectorType::Plane);
		OuterPlaneComponent->SetVisibility(Type == ECEClonerEffectorType::Plane);
	}

	ChannelData.Type = Type;

	// Update type data
	OnSphereChanged();
	OnBoxChanged();
	OnPlaneChanged();
}

void ACEEffectorActor::OnEffectorChanged()
{
	if (!bEnabled)
	{
		OnEnabledChanged();
	}
	else
	{
		OnModeChanged();
		OnTypeChanged();
		OnEasingChanged();
		OnTransformChanged();
		OnMagnitudeChanged();
		OnForceOptionsChanged();
		// Editor
		OnVisualizerThicknessChanged();
		OnVisualizerSpriteVisibleChanged();
	}
}

void ACEEffectorActor::OnEasingChanged()
{
	ChannelData.Easing = Easing;
}

void ACEEffectorActor::OnTransformChanged()
{
	ChannelData.Location = GetActorLocation();
	ChannelData.Rotation = GetActorRotation().Quaternion();
	ChannelData.Scale = GetActorScale();
}

void ACEEffectorActor::OnBoxChanged()
{
	InnerExtent = ClampVector(InnerExtent, FVector::ZeroVector, OuterExtent);
	// Max()
	OuterExtent = OuterExtent.ComponentMax(InnerExtent);

	if (InnerBoxComponent && OuterBoxComponent)
	{
		InnerBoxComponent->SetBoxExtent(InnerExtent);
		OuterBoxComponent->SetBoxExtent(OuterExtent);
	}

	if (Type != ECEClonerEffectorType::Box)
	{
		return;
	}

	ChannelData.InnerExtent = GetInnerExtent();
	ChannelData.OuterExtent = GetOuterExtent();
}

void ACEEffectorActor::OnSphereChanged()
{
	InnerRadius = FMath::Clamp(InnerRadius, 0.f, OuterRadius);
	OuterRadius = FMath::Max(OuterRadius, InnerRadius);

	if (InnerSphereComponent && OuterSphereComponent)
	{
		InnerSphereComponent->SetSphereRadius(InnerRadius);
		OuterSphereComponent->SetSphereRadius(OuterRadius);
	}

	if (Type != ECEClonerEffectorType::Sphere)
	{
		return;
	}

	ChannelData.InnerExtent = FVector(GetInnerRadius());
	ChannelData.OuterExtent = FVector(GetOuterRadius());
}

void ACEEffectorActor::OnPlaneChanged()
{
	static const FVector PlaneAxis = -FVector::LeftVector;
	const FVector InnerPlane = PlaneAxis * FVector(-PlaneSpacing/2);
	const FVector OuterPlane = PlaneAxis * FVector(PlaneSpacing/2);

	if (InnerPlaneComponent && OuterPlaneComponent)
	{
		static const FVector InnerSize(100, 0, 100);
		InnerPlaneComponent->SetRelativeLocation(InnerPlane);
		InnerPlaneComponent->SetBoxExtent(InnerSize);

		static const FVector OuterSize(200, 0, 200);
		OuterPlaneComponent->SetRelativeLocation(OuterPlane);
		OuterPlaneComponent->SetBoxExtent(OuterSize);
	}

	if (Type != ECEClonerEffectorType::Plane)
	{
		return;
	}

	ChannelData.InnerExtent = PlaneAxis;
	ChannelData.OuterExtent = FVector(PlaneSpacing);
}

void ACEEffectorActor::OnMagnitudeChanged()
{
	const float EffectorMagnitude = bEnabled ? GetMagnitude() : 0.f;
	ChannelData.Magnitude = EffectorMagnitude;
}

void ACEEffectorActor::OnTransformOptionsChanged()
{
	if (Mode != ECEClonerEffectorMode::Default)
	{
		return;
	}

	FVector ScaleDelta = GetScale();
	ScaleDelta.X = FMath::Max(ScaleDelta.X, UE_KINDA_SMALL_NUMBER);
	ScaleDelta.Y = FMath::Max(ScaleDelta.Y, UE_KINDA_SMALL_NUMBER);
	ScaleDelta.Z = FMath::Max(ScaleDelta.Z, UE_KINDA_SMALL_NUMBER);

	ChannelData.LocationDelta = GetOffset();
	ChannelData.RotationDelta = GetRotation().Quaternion();
	ChannelData.ScaleDelta = ScaleDelta;
}

void ACEEffectorActor::OnTargetActorChanged()
{
	AActor* TargetActor = TargetActorWeak.Get();
	AActor* const InternalTargetActor = InternalTargetActorWeak.Get();
	if (TargetActor && TargetActor == InternalTargetActor)
	{
		OnTargetOptionsChanged();
		return;
	}

	// unbind, except if it is self
	if (InternalTargetActor && InternalTargetActor->GetRootComponent())
	{
		if (InternalTargetActor != this)
		{
			InternalTargetActor->OnDestroyed.RemoveAll(this);
			InternalTargetActor->GetRootComponent()->TransformUpdated.RemoveAll(this);
			InternalTargetActorWeak.Reset();
		}
	}

	// set self by default if invalid
	if (!TargetActor || !TargetActor->GetRootComponent())
	{
		TargetActor = this;
	}

	// bind to transform event, do not bind to self since we already do that
	if (TargetActor != this)
	{
		TargetActor->GetRootComponent()->TransformUpdated.RemoveAll(this);
		TargetActor->GetRootComponent()->TransformUpdated.AddUObject(this, &ACEEffectorActor::OnTargetActorTransformChanged);
		TargetActor->OnDestroyed.RemoveAll(this);
		TargetActor->OnDestroyed.AddUniqueDynamic(this, &ACEEffectorActor::OnTargetActorDestroyed);
	}

	TargetActorWeak = TargetActor;
	InternalTargetActorWeak = TargetActor;
	OnTargetOptionsChanged();
}

void ACEEffectorActor::OnTargetActorDestroyed(AActor* InActor)
{
	const AActor* TargetActor = TargetActorWeak.Get();
	if (TargetActor == InActor)
	{
		TargetActorWeak = this;
		OnTargetActorChanged();
	}
}

void ACEEffectorActor::OnNoiseFieldOptionsChanged()
{
	if (Mode != ECEClonerEffectorMode::NoiseField)
	{
		return;
	}

	ChannelData.LocationDelta = LocationStrength;
	ChannelData.RotationDelta = RotationStrength.Quaternion();
	ChannelData.ScaleDelta = ScaleStrength;
	ChannelData.Frequency = Frequency;
	ChannelData.Pan = Pan;
}

void ACEEffectorActor::OnTargetActorTransformChanged(USceneComponent*, EUpdateTransformFlags, ETeleportType)
{
	OnTargetOptionsChanged();
}

void ACEEffectorActor::OnTargetOptionsChanged()
{
	if (Mode != ECEClonerEffectorMode::Target)
	{
		return;
	}

	const AActor* InternalTargetActor = InternalTargetActorWeak.Get();
	if (!InternalTargetActor)
	{
		return;
	}

	ChannelData.LocationDelta = InternalTargetActor->GetActorLocation();
	ChannelData.RotationDelta = FQuat::Identity;
	ChannelData.ScaleDelta = FVector::OneVector;
}

void ACEEffectorActor::OnVisualizerThicknessChanged()
{
	VisualizerThickness = FMath::Clamp(VisualizerThickness, 0.1f, 10.f);

	if (InnerSphereComponent && OuterSphereComponent)
	{
		InnerSphereComponent->SetLineThickness(VisualizerThickness);
		OuterSphereComponent->SetLineThickness(VisualizerThickness);
	}

	if (InnerBoxComponent && OuterBoxComponent)
	{
		InnerBoxComponent->SetLineThickness(VisualizerThickness);
		OuterBoxComponent->SetLineThickness(VisualizerThickness);
	}

	if (InnerPlaneComponent && OuterPlaneComponent)
	{
		InnerPlaneComponent->SetLineThickness(VisualizerThickness);
		OuterPlaneComponent->SetLineThickness(VisualizerThickness);
	}
}

void ACEEffectorActor::OnVisualizerSpriteVisibleChanged()
{
#if WITH_EDITOR
	UE::ClonerEffector::SetBillboardComponentSprite(this, TEXT("/Script/Engine.Texture2D'/ClonerEffector/Textures/T_EffectorIcon.T_EffectorIcon'"));
	UE::ClonerEffector::SetBillboardComponentVisibility(this, bVisualizerSpriteVisible);
#endif
}

void ACEEffectorActor::OnForceOptionsChanged()
{
	if (bOrientationForceEnabled)
	{
		ChannelData.OrientationForceRate = OrientationForceRate;
		ChannelData.OrientationForceMin = OrientationForceMin;
		ChannelData.OrientationForceMax = OrientationForceMax;
	}
	else
	{
		ChannelData.OrientationForceRate = 0.f;
		ChannelData.OrientationForceMin = FVector::ZeroVector;
		ChannelData.OrientationForceMax = FVector::ZeroVector;
	}

	if (bVortexForceEnabled)
	{
		ChannelData.VortexForceAmount = VortexForceAmount;
		ChannelData.VortexForceAxis = VortexForceAxis;
	}
	else
	{
		ChannelData.VortexForceAmount = 0.f;
		ChannelData.VortexForceAxis = FVector::ZeroVector;
	}

	if (bCurlNoiseForceEnabled)
	{
		ChannelData.CurlNoiseForceStrength = CurlNoiseForceStrength;
		ChannelData.CurlNoiseForceFrequency = CurlNoiseForceFrequency;
	}
	else
	{
		ChannelData.CurlNoiseForceStrength = 0.f;
		ChannelData.CurlNoiseForceFrequency = 0.f;
	}

	if (bAttractionForceEnabled)
	{
		ChannelData.AttractionForceStrength = AttractionForceStrength;
		ChannelData.AttractionForceFalloff = AttractionForceFalloff;
	}
	else
	{
		ChannelData.AttractionForceStrength = 0.f;
		ChannelData.AttractionForceFalloff = 0.f;
	}

	if (bGravityForceEnabled)
	{
		ChannelData.GravityForceAcceleration = GravityForceAcceleration;
	}
	else
	{
		ChannelData.GravityForceAcceleration = FVector::ZeroVector;
	}
}

void ACEEffectorActor::OnForceEnabledChanged()
{
	if (bEnabled)
	{
		// Refresh cloners to reset clones transform
		OnEffectorRefreshClonerDelegate.Broadcast(this);
	}

	OnForceOptionsChanged();
}
