// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/CEClonerActor.h"

#include "Cloner/CEClonerComponent.h"
#include "Cloner/Layouts/CEClonerCircleLayout.h"
#include "Cloner/Layouts/CEClonerCylinderLayout.h"
#include "Cloner/Layouts/CEClonerGridLayout.h"
#include "Cloner/Layouts/CEClonerHoneycombLayout.h"
#include "Cloner/Layouts/CEClonerLineLayout.h"
#include "Cloner/Layouts/CEClonerMeshLayout.h"
#include "Cloner/Layouts/CEClonerSphereUniformLayout.h"
#include "Cloner/Layouts/CEClonerSplineLayout.h"
#include "Effector/CEEffectorActor.h"
#include "Engine/StaticMeshActor.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraDataInterfaceCurve.h"
#include "NiagaraDataInterfaceSkeletalMesh.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraSystem.h"
#include "Subsystems/CEClonerSubsystem.h"
#include "Subsystems/CEEffectorSubsystem.h"
#include "UObject/ConstructorHelpers.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Selection.h"
#include "Settings/LevelEditorViewportSettings.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogCEClonerActor, Log, All);

// Sets default values
ACEClonerActor::ACEClonerActor()
{
	SetCanBeDamaged(false);
	PrimaryActorTick.bCanEverTick          = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bTickEvenWhenPaused   = true;
	PrimaryActorTick.bHighPriority         = true;

	ClonerComponent = CreateDefaultSubobject<UCEClonerComponent>(TEXT("AvaClonerComponent"));
	SetRootComponent(ClonerComponent);

	// Default Scale Curve
	LifetimeScaleCurve.AddKey(0, 1.f);
	LifetimeScaleCurve.AddKey(1, 0.f);

	if (!IsTemplate())
	{
#if WITH_EDITOR
		if (GEditor)
		{
			GEditor->GetSelectedActors()->SelectionChangedEvent.AddUObject(this, &ACEClonerActor::OnEditorSelectionChanged);
		}
#endif

		ClonerComponent->OnClonerMeshUpdated.AddUObject(this, &ACEClonerActor::OnClonerMeshUpdated);

		UCEEffectorSubsystem::OnEffectorIdentifierChangedDelegate.AddUObject(this, &ACEClonerActor::OnEffectorIdentifierChanged);
		ACEEffectorActor::OnEffectorRefreshClonerDelegate.AddUObject(this, &ACEClonerActor::OnEffectorRefreshCloner);

		const TArray<FString> LayoutNames = GetClonerLayoutNames();

		// Apply default layout
		LayoutName = !LayoutNames.IsEmpty() ? FName(LayoutNames[0]) : NAME_None;

#if WITH_EDITOR
		if (const UCEClonerSubsystem* ClonerSubsystem = UCEClonerSubsystem::Get())
		{
			UCEClonerSubsystem::OnCVarChanged().AddUObject(this, &ACEClonerActor::OnCVarChanged);
			bReduceMotionGhosting = ClonerSubsystem->IsNoFlickerEnabled();
		}
#endif
	}
}

#if WITH_EDITOR
FString ACEClonerActor::GetDefaultActorLabel() const
{
	return DefaultLabel;
}

void ACEClonerActor::PostEditUndo()
{
	Super::PostEditUndo();

	ForceUpdateCloner();
}

const TCEPropertyChangeDispatcher<ACEClonerActor> ACEClonerActor::PropertyChangeDispatcher =
{
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, bEnabled), &ACEClonerActor::OnEnabledChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, Seed), &ACEClonerActor::OnSeedChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, EffectorsWeak), &ACEClonerActor::OnEffectorsChanged },
	/** Layout */
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, LayoutName), &ACEClonerActor::OnLayoutNameChanged },
	/** Advanced */
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, bVisualizerSpriteVisible), &ACEClonerActor::OnVisualizerSpriteVisibleChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, bReduceMotionGhosting), &ACEClonerActor::OnReduceMotionGhostingChanged },
	/** Spawn */
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, SpawnLoopMode), &ACEClonerActor::OnSpawnOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, SpawnLoopInterval), &ACEClonerActor::OnSpawnOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, SpawnLoopIterations), &ACEClonerActor::OnSpawnOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, SpawnBehaviorMode), &ACEClonerActor::OnSpawnOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, SpawnRate), &ACEClonerActor::OnSpawnOptionsChanged },
	/** Lifetime */
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, bLifetimeEnabled), &ACEClonerActor::OnLifetimeOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, LifetimeMin), &ACEClonerActor::OnLifetimeOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, LifetimeMax), &ACEClonerActor::OnLifetimeOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, bLifetimeScaleEnabled), &ACEClonerActor::OnLifetimeOptionsChanged },
	/** Renderer */
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, MeshRenderMode), &ACEClonerActor::OnMeshRenderModeChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, MeshFacingMode), &ACEClonerActor::OnMeshRendererOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, bMeshCastShadows), &ACEClonerActor::OnMeshRendererOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, DefaultMeshes), &ACEClonerActor::OnDefaultMeshesChanged },
	/** Progress */
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, bInvertProgress), &ACEClonerActor::OnProgressChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, Progress), &ACEClonerActor::OnProgressChanged },
	/** Step */
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, bDeltaStepEnabled), &ACEClonerActor::OnDeltaStepChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, DeltaStepRotation), &ACEClonerActor::OnDeltaStepChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, DeltaStepScale), &ACEClonerActor::OnDeltaStepChanged },
	/** Range */
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, bRangeEnabled), &ACEClonerActor::OnRangeOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, RangeOffsetMin), &ACEClonerActor::OnRangeOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, RangeOffsetMax), &ACEClonerActor::OnRangeOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, RangeRotationMin), &ACEClonerActor::OnRangeOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, RangeRotationMax), &ACEClonerActor::OnRangeOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, RangeScaleMin), &ACEClonerActor::OnRangeOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, RangeScaleMax), &ACEClonerActor::OnRangeOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, bRangeScaleUniform), &ACEClonerActor::OnRangeOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, RangeScaleUniformMin), &ACEClonerActor::OnRangeOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEClonerActor, RangeScaleUniformMax), &ACEClonerActor::OnRangeOptionsChanged },
};

void ACEClonerActor::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	PropertyChangeDispatcher.OnPropertyChanged(this, InPropertyChangedEvent);
}
#endif

void ACEClonerActor::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		// Register new type def for niagara

		constexpr ENiagaraTypeRegistryFlags MeshFlags =
			ENiagaraTypeRegistryFlags::AllowAnyVariable |
			ENiagaraTypeRegistryFlags::AllowParameter;

		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(StaticEnum<ECEClonerMeshRenderMode>()), MeshFlags);
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(StaticEnum<ECEClonerGridConstraint>()), MeshFlags);
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(StaticEnum<ECEClonerPlane>()), MeshFlags);
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(StaticEnum<ECEClonerAxis>()), MeshFlags);
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(StaticEnum<ECEClonerEasing>()), MeshFlags);
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(StaticEnum<ECEClonerMeshAsset>()), MeshFlags);
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(StaticEnum<ECEClonerMeshSampleData>()), MeshFlags);
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(StaticEnum<ECEClonerEffectorType>()), MeshFlags);
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(StaticEnum<ECEClonerTextureSampleChannel>()), MeshFlags);
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(StaticEnum<ECEClonerCompareMode>()), MeshFlags);
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(StaticEnum<ECEClonerEffectorMode>()), MeshFlags);
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(StaticEnum<ECEClonerSpawnLoopMode>()), MeshFlags);
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(StaticEnum<ECEClonerSpawnBehaviorMode>()), MeshFlags);
	}
}

void ACEClonerActor::PostLoad()
{
	Super::PostLoad();

	// Migrate old properties to new layout system
	if (!bDeprecatedPropertiesMigrated)
	{
		// Grid options
		if (UCEClonerGridLayout* GridLayout = FindOrAddLayout<UCEClonerGridLayout>())
		{
			GridLayout->SetCountX(GridOptions_DEPRECATED.Count.X);
			GridLayout->SetCountY(GridOptions_DEPRECATED.Count.Y);
			GridLayout->SetCountZ(GridOptions_DEPRECATED.Count.Z);
			GridLayout->SetSpacingX(GridOptions_DEPRECATED.Spacing.X);
			GridLayout->SetSpacingY(GridOptions_DEPRECATED.Spacing.Y);
			GridLayout->SetSpacingZ(GridOptions_DEPRECATED.Spacing.Z);
			GridLayout->SetConstraint(GridOptions_DEPRECATED.Constraint);
			GridLayout->SetInvertConstraint(GridOptions_DEPRECATED.bInvertConstraint);
			GridLayout->SetSphereConstraint(GridOptions_DEPRECATED.SphereConstraint);
			GridLayout->SetCylinderConstraint(GridOptions_DEPRECATED.CylinderConstraint);
			GridLayout->SetTextureConstraint(GridOptions_DEPRECATED.TextureConstraint);
		}

		// Line options
		if (UCEClonerLineLayout* LineLayout = FindOrAddLayout<UCEClonerLineLayout>())
		{
			LineLayout->SetCount(LineOptions_DEPRECATED.Count);
			LineLayout->SetSpacing(LineOptions_DEPRECATED.Spacing);
			LineLayout->SetAxis(LineOptions_DEPRECATED.Axis);
			LineLayout->SetDirection(LineOptions_DEPRECATED.Direction);
			LineLayout->SetRotation(LineOptions_DEPRECATED.Rotation);
		}

		// Circle options
		if (UCEClonerCircleLayout* CircleLayout = FindOrAddLayout<UCEClonerCircleLayout>())
		{
			CircleLayout->SetCount(CircleOptions_DEPRECATED.Count);
			CircleLayout->SetRadius(CircleOptions_DEPRECATED.Radius);
			CircleLayout->SetAngleStart(CircleOptions_DEPRECATED.AngleStart);
			CircleLayout->SetAngleRatio(CircleOptions_DEPRECATED.AngleRatio);
			CircleLayout->SetOrientMesh(CircleOptions_DEPRECATED.bOrientMesh);
			CircleLayout->SetPlane(CircleOptions_DEPRECATED.Plane);
			CircleLayout->SetRotation(CircleOptions_DEPRECATED.Rotation);
			CircleLayout->SetScale(CircleOptions_DEPRECATED.Scale);
		}

		// Cylinder options
		if (UCEClonerCylinderLayout* CylinderLayout = FindOrAddLayout<UCEClonerCylinderLayout>())
		{
			CylinderLayout->SetBaseCount(CylinderOptions_DEPRECATED.BaseCount);
			CylinderLayout->SetHeightCount(CylinderOptions_DEPRECATED.HeightCount);
			CylinderLayout->SetHeight(CylinderOptions_DEPRECATED.Height);
			CylinderLayout->SetRadius(CylinderOptions_DEPRECATED.Radius);
			CylinderLayout->SetAngleStart(CylinderOptions_DEPRECATED.AngleStart);
			CylinderLayout->SetAngleRatio(CylinderOptions_DEPRECATED.AngleRatio);
			CylinderLayout->SetOrientMesh(CylinderOptions_DEPRECATED.bOrientMesh);
			CylinderLayout->SetPlane(CylinderOptions_DEPRECATED.Plane);
			CylinderLayout->SetRotation(CylinderOptions_DEPRECATED.Rotation);
			CylinderLayout->SetScale(CylinderOptions_DEPRECATED.Scale);
		}

		// Sphere options
		if (UCEClonerSphereUniformLayout* SphereLayout = FindOrAddLayout<UCEClonerSphereUniformLayout>())
		{
			SphereLayout->SetCount(SphereOptions_DEPRECATED.Count);
			SphereLayout->SetRadius(SphereOptions_DEPRECATED.Radius);
			SphereLayout->SetRatio(SphereOptions_DEPRECATED.Ratio);
			SphereLayout->SetOrientMesh(SphereOptions_DEPRECATED.bOrientMesh);
			SphereLayout->SetRotation(SphereOptions_DEPRECATED.Rotation);
			SphereLayout->SetScale(SphereOptions_DEPRECATED.Scale);
		}

		// Honeycomb options
		if (UCEClonerHoneycombLayout* HoneycombLayout = FindOrAddLayout<UCEClonerHoneycombLayout>())
		{
			HoneycombLayout->SetPlane(HoneycombOptions_DEPRECATED.Plane);
			HoneycombLayout->SetWidthCount(HoneycombOptions_DEPRECATED.WidthCount);
			HoneycombLayout->SetHeightCount(HoneycombOptions_DEPRECATED.HeightCount);
			HoneycombLayout->SetWidthOffset(HoneycombOptions_DEPRECATED.WidthOffset);
			HoneycombLayout->SetHeightOffset(HoneycombOptions_DEPRECATED.HeightOffset);
			HoneycombLayout->SetHeightSpacing(HoneycombOptions_DEPRECATED.HeightSpacing);
			HoneycombLayout->SetWidthSpacing(HoneycombOptions_DEPRECATED.WidthSpacing);
		}

		// Sample mesh options
		if (UCEClonerMeshLayout* MeshLayout = FindOrAddLayout<UCEClonerMeshLayout>())
		{
			MeshLayout->SetCount(SampleMeshOptions_DEPRECATED.Count);
			MeshLayout->SetAsset(SampleMeshOptions_DEPRECATED.Asset);
			MeshLayout->SetSampleData(SampleMeshOptions_DEPRECATED.SampleData);
			MeshLayout->SetSampleActorWeak(SampleMeshOptions_DEPRECATED.SampleActor);
		}

		// Sample spline options
		if (UCEClonerSplineLayout* SplineLayout = FindOrAddLayout<UCEClonerSplineLayout>())
		{
			SplineLayout->SetCount(SampleSplineOptions_DEPRECATED.Count);
			SplineLayout->SetSplineActorWeak(SampleSplineOptions_DEPRECATED.SplineActor);
		}

		// Set previous layout
		if (const UCEClonerSubsystem* Subsystem = UCEClonerSubsystem::Get())
		{
			const int32 LayoutIdx = static_cast<int32>(Layout_DEPRECATED);
			SetLayoutName(Subsystem->GetLayoutNames()[LayoutIdx]);
		}

		bDeprecatedPropertiesMigrated = true;

		UE_LOG(LogCEClonerActor, Log, TEXT("%s : Cloner deprecated properties migrated"), *GetActorNameOrLabel());
	}

	for (TMap<FName, TObjectPtr<UCEClonerLayoutBase>>::TIterator It(LayoutInstances); It; ++It)
	{
		if (!It->Value)
		{
			It.RemoveCurrent();
			continue;
		}

		// Update layout name if changed
		const FName NewLayoutName = It->Value->GetLayoutName();

		if (It->Key != NewLayoutName)
		{
			LayoutInstances.Add(NewLayoutName, It->Value);
			It.RemoveCurrent();

			UE_LOG(LogCEClonerActor, Log, TEXT("%s : Cloner layout name changed %s"), *GetActorNameOrLabel(), *NewLayoutName.ToString());
		}
	}
}

void ACEClonerActor::PostActorCreated()
{
	Super::PostActorCreated();

#if WITH_EDITOR
	bSpawnDefaultActorAttached = true;
#endif
}

void ACEClonerActor::Tick(float InDeltaTime)
{
	Super::Tick(InDeltaTime);

	if (!bClonerInitialized)
	{
		InitializeCloner();
	}

	if (!bEnabled)
	{
		return;
	}

	TreeUpdateDeltaTime += InDeltaTime;
	if (TreeUpdateDeltaTime >= TreeUpdateInterval)
	{
		TreeUpdateDeltaTime -= TreeUpdateInterval;

		if (ClonerComponent)
		{
			ClonerComponent->UpdateClonerAttachmentTree();
			ClonerComponent->UpdateClonerRenderState();
		}
	}

	if (bNeedsRefresh)
	{
		bNeedsRefresh = false;
		RequestClonerUpdate(true);
	}
}

void ACEClonerActor::UpdateLayoutOptions()
{
	OnMeshRenderModeChanged();
	OnSeedChanged();

	// update layouts options
	if (UCEClonerLayoutBase* CurrentLayout = ClonerComponent->GetClonerActiveLayout())
	{
		CurrentLayout->UpdateLayoutParameters();
	}

	OnMeshRendererOptionsChanged();
	OnDefaultMeshesChanged();
	OnDeltaStepChanged();
	OnRangeOptionsChanged();
	OnSpawnOptionsChanged();
	OnLifetimeOptionsChanged();
}

void ACEClonerActor::SetTreeUpdateInterval(float InInterval)
{
	if (InInterval == TreeUpdateInterval)
	{
		return;
	}

	TreeUpdateInterval = InInterval;
}

void ACEClonerActor::SetMeshRenderMode(ECEClonerMeshRenderMode InMode)
{
	if (InMode == MeshRenderMode)
	{
		return;
	}

	MeshRenderMode = InMode;
	OnMeshRenderModeChanged();
}

void ACEClonerActor::SetMeshFacingMode(ENiagaraMeshFacingMode InMode)
{
	if (MeshFacingMode == InMode)
	{
		return;
	}

	MeshFacingMode = InMode;
	OnMeshRendererOptionsChanged();
}

void ACEClonerActor::SetMeshCastShadows(bool InbCastShadows)
{
	if (bMeshCastShadows == InbCastShadows)
	{
		return;
	}

	bMeshCastShadows = InbCastShadows;
	OnMeshRendererOptionsChanged();
}

void ACEClonerActor::SetDefaultMeshes(const TArray<TObjectPtr<UStaticMesh>>& InMeshes)
{
	DefaultMeshes = InMeshes;
	OnDefaultMeshesChanged();
}

void ACEClonerActor::BP_SetDefaultMeshes(const TArray<UStaticMesh*>& InMeshes)
{
	DefaultMeshes.Empty(InMeshes.Num());
	Algo::Transform(InMeshes, DefaultMeshes, [](UStaticMesh* InMesh)->TObjectPtr<UStaticMesh>
	{
		return InMesh;
	});
	OnDefaultMeshesChanged();
}

TArray<UStaticMesh*> ACEClonerActor::BP_GetDefaultMeshes() const
{
	TArray<UStaticMesh*> Meshes;
	Meshes.Reserve(DefaultMeshes.Num());
	Algo::Transform(DefaultMeshes, Meshes, [](const TObjectPtr<UStaticMesh>& InMesh)->UStaticMesh*
	{
		return InMesh;
	});
	return Meshes;
}

void ACEClonerActor::SetSeed(int32 InSeed)
{
	if (InSeed == Seed)
	{
		return;
	}

	Seed = InSeed;
	OnSeedChanged();
}

void ACEClonerActor::SetDeltaStepEnabled(bool bInEnabled)
{
	if (bDeltaStepEnabled == bInEnabled)
	{
		return;
	}

	bDeltaStepEnabled = bInEnabled;
	OnDeltaStepChanged();
}

void ACEClonerActor::SetDeltaStepRotation(const FRotator& InRotation)
{
	if (InRotation == DeltaStepRotation)
	{
		return;
	}

	DeltaStepRotation = InRotation;
	OnDeltaStepChanged();
}

void ACEClonerActor::SetDeltaStepScale(const FVector& InScale)
{
	if (InScale == DeltaStepScale)
	{
		return;
	}

	DeltaStepScale = InScale;
	OnDeltaStepChanged();
}

void ACEClonerActor::SetEnabled(bool bInEnable)
{
	if (bInEnable == bEnabled)
	{
		return;
	}

	bEnabled = bInEnable;
	OnEnabledChanged();
}

void ACEClonerActor::SetInvertProgress(bool bInInvertProgress)
{
	if (bInvertProgress == bInInvertProgress)
	{
		return;
	}

	bInvertProgress = bInInvertProgress;
	OnProgressChanged();
}

void ACEClonerActor::SetProgress(float InProgress)
{
	if (Progress == InProgress)
	{
		return;
	}

	if (InProgress < 0.f || InProgress > 1.f)
	{
		return;
	}

	Progress = InProgress;
	OnProgressChanged();
}

void ACEClonerActor::SetLayoutName(FName InLayoutName)
{
	if (LayoutName == InLayoutName)
	{
		return;
	}

	const TArray<FString> LayoutNames = GetClonerLayoutNames();
	if (!LayoutNames.Contains(InLayoutName))
	{
		return;
	}

	LayoutName = InLayoutName;
	OnLayoutNameChanged();
}

void ACEClonerActor::SetRangeEnabled(bool bInRangeEnabled)
{
	if (bRangeEnabled == bInRangeEnabled)
	{
		return;
	}

	bRangeEnabled = bInRangeEnabled;
	OnRangeOptionsChanged();
}

void ACEClonerActor::SetRangeOffsetMin(const FVector& InRangeOffsetMin)
{
	if (RangeOffsetMin == InRangeOffsetMin)
	{
		return;
	}

	RangeOffsetMin = InRangeOffsetMin;
	OnRangeOptionsChanged();
}

void ACEClonerActor::SetRangeOffsetMax(const FVector& InRangeOffsetMax)
{
	if (RangeOffsetMax == InRangeOffsetMax)
	{
		return;
	}

	RangeOffsetMax = InRangeOffsetMax;
	OnRangeOptionsChanged();
}

void ACEClonerActor::SetRangeRotationMin(const FRotator& InRangeRotationMin)
{
	if (RangeRotationMin == InRangeRotationMin)
	{
		return;
	}

	RangeRotationMin = InRangeRotationMin;
	OnRangeOptionsChanged();
}

void ACEClonerActor::SetRangeRotationMax(const FRotator& InRangeRotationMax)
{
	if (RangeRotationMax == InRangeRotationMax)
	{
		return;
	}

	RangeRotationMax = InRangeRotationMax;
	OnRangeOptionsChanged();
}

void ACEClonerActor::SetRangeScaleUniform(bool bInRangeScaleUniform)
{
	if (bRangeScaleUniform == bInRangeScaleUniform)
	{
		return;
	}

	bRangeScaleUniform = bInRangeScaleUniform;
	OnRangeOptionsChanged();
}

void ACEClonerActor::SetRangeScaleMin(const FVector& InRangeScaleMin)
{
	if (RangeScaleMin == InRangeScaleMin)
	{
		return;
	}

	RangeScaleMin = InRangeScaleMin;
	OnRangeOptionsChanged();
}

void ACEClonerActor::SetRangeScaleMax(const FVector& InRangeScaleMax)
{
	if (RangeScaleMax == InRangeScaleMax)
	{
		return;
	}

	RangeScaleMax = InRangeScaleMax;
	OnRangeOptionsChanged();
}

void ACEClonerActor::SetRangeScaleUniformMin(float InRangeScaleUniformMin)
{
	if (RangeScaleUniformMin == InRangeScaleUniformMin)
	{
		return;
	}

	RangeScaleUniformMin = InRangeScaleUniformMin;
	OnRangeOptionsChanged();
}

void ACEClonerActor::SetRangeScaleUniformMax(float InRangeScaleUniformMax)
{
	if (RangeScaleUniformMax == InRangeScaleUniformMax)
	{
		return;
	}

	RangeScaleUniformMax = InRangeScaleUniformMax;
	OnRangeOptionsChanged();
}

void ACEClonerActor::OnMeshRenderModeChanged()
{
	if (!bEnabled)
	{
		return;
	}

	if (const UCEClonerLayoutBase* LayoutSystem = GetActiveLayout())
	{
		FNiagaraUserRedirectionParameterStore& ExposedParameters = LayoutSystem->GetSystem()->GetExposedParameters();

		static const FNiagaraVariable MeshModeVar(FNiagaraTypeDefinition(StaticEnum<ECEClonerMeshRenderMode>()), TEXT("MeshRenderMode"));
		ExposedParameters.SetParameterValue<int32>(static_cast<int32>(MeshRenderMode), MeshModeVar);

		RequestClonerUpdate();
	}
}

void ACEClonerActor::OnDeltaStepChanged()
{
	if (!ClonerComponent || !bEnabled)
	{
		return;
	}

	if (bDeltaStepEnabled)
	{
		ClonerComponent->SetVectorParameter(TEXT("DeltaStepRotation"), FVector(DeltaStepRotation.Roll, DeltaStepRotation.Pitch, DeltaStepRotation.Yaw));

		ClonerComponent->SetVectorParameter(TEXT("DeltaStepScale"), DeltaStepScale);
	}
	else
	{
		ClonerComponent->SetVectorParameter(TEXT("DeltaStepRotation"), FVector::ZeroVector);

		ClonerComponent->SetVectorParameter(TEXT("DeltaStepScale"), FVector::ZeroVector);
	}

	RequestClonerUpdate();
}

void ACEClonerActor::OnSeedChanged()
{
	if (!ClonerComponent || !bEnabled)
	{
		return;
	}

	ClonerComponent->SetRandomSeedOffset(Seed);

	RequestClonerUpdate();
}

void ACEClonerActor::OnProgressChanged()
{
	if (!ClonerComponent || !bEnabled)
	{
		return;
	}

	ClonerComponent->SetFloatParameter(TEXT("ParticleProgress"), Progress * (bInvertProgress ? -1 : 1));
}

void ACEClonerActor::SetSpawnLoopMode(ECEClonerSpawnLoopMode InMode)
{
	if (SpawnLoopMode == InMode)
	{
		return;
	}

	SpawnLoopMode = InMode;
	OnSpawnOptionsChanged();
}

void ACEClonerActor::SetSpawnLoopIterations(int32 InIterations)
{
	if (SpawnLoopIterations == InIterations)
	{
		return;
	}

	if (InIterations < 1)
	{
		return;
	}

	SpawnLoopIterations = InIterations;
	OnSpawnOptionsChanged();
}

void ACEClonerActor::SetSpawnLoopInterval(float InInterval)
{
	if (SpawnLoopInterval == InInterval)
	{
		return;
	}

	if (InInterval < 0.f)
	{
		return;
	}

	SpawnLoopInterval = InInterval;
	OnSpawnOptionsChanged();
}

void ACEClonerActor::SetSpawnBehaviorMode(ECEClonerSpawnBehaviorMode InMode)
{
	if (SpawnBehaviorMode == InMode)
	{
		return;
	}

	SpawnBehaviorMode = InMode;
	OnSpawnOptionsChanged();
}

void ACEClonerActor::SetSpawnRate(float InRate)
{
	if (SpawnRate == InRate)
	{
		return;
	}

	if (InRate < 0)
	{
		return;
	}

	SpawnRate = InRate;
	OnSpawnOptionsChanged();
}

void ACEClonerActor::SetLifetimeEnabled(bool bInEnabled)
{
	if (bLifetimeEnabled == bInEnabled)
	{
		return;
	}

	bLifetimeEnabled = bInEnabled;
	OnLifetimeOptionsChanged();
}

void ACEClonerActor::SetLifetimeMin(float InMin)
{
	if (LifetimeMin == InMin)
	{
		return;
	}

	if (InMin < 0)
	{
		return;
	}

	LifetimeMin = InMin;
	OnLifetimeOptionsChanged();
}

void ACEClonerActor::SetLifetimeMax(float InMax)
{
	if (LifetimeMax == InMax)
	{
		return;
	}

	if (InMax < 0)
	{
		return;
	}

	LifetimeMax = InMax;
	OnLifetimeOptionsChanged();
}

void ACEClonerActor::SetLifetimeScaleEnabled(bool bInEnabled)
{
	if (bLifetimeScaleEnabled == bInEnabled)
	{
		return;
	}

	bLifetimeScaleEnabled = bInEnabled;
	OnLifetimeOptionsChanged();
}

void ACEClonerActor::SetLifetimeScaleCurve(const FRichCurve& InCurve)
{
	LifetimeScaleCurve = InCurve;
	OnLifetimeOptionsChanged();
}

UCEClonerLayoutBase* ACEClonerActor::GetActiveLayout() const
{
	if (ClonerComponent)
	{
		return ClonerComponent->GetClonerActiveLayout();
	}

	return nullptr;
}

const FCEClonerEffectorDataInterfaces* ACEClonerActor::GetEffectorDataInterfaces() const
{
	if (const UCEClonerLayoutBase* LayoutSystem = GetActiveLayout())
	{
		return &LayoutSystem->GetDataInterfaces();
	}

	return nullptr;
}

int32 ACEClonerActor::GetMeshCount() const
{
	if (ClonerComponent)
	{
		if (const UCEClonerLayoutBase* LayoutSystem = GetActiveLayout())
		{
			return LayoutSystem->GetMeshRenderer()->Meshes.Num();
		}
	}
	return 0;
}

bool ACEClonerActor::LinkEffector(ACEEffectorActor* InEffector)
{
	if (!IsValid(InEffector) || EffectorsWeak.Contains(InEffector))
	{
		return false;
	}

	EffectorsWeak.Add(InEffector);

	OnEffectorsChanged();

	UE_LOG(LogCEClonerActor, Log, TEXT("%s : Effector %s linked to Cloner"), *GetActorNameOrLabel(), *InEffector->GetActorNameOrLabel());

	return true;
}

bool ACEClonerActor::UnlinkEffector(ACEEffectorActor* InEffector)
{
	if (!InEffector)
	{
		return false;
	}

	if (EffectorsWeak.Remove(InEffector) > 0)
	{
		OnEffectorsChanged();

		UE_LOG(LogCEClonerActor, Log, TEXT("%s : Effector %s unlinked from Cloner"), *GetActorNameOrLabel(), *InEffector->GetActorNameOrLabel());
	}

	return true;
}

bool ACEClonerActor::IsEffectorLinked(const ACEEffectorActor* InEffector) const
{
	return InEffector && EffectorsWeak.Contains(InEffector);
}

int32 ACEClonerActor::GetEffectorCount() const
{
	return EffectorsWeak.Num();
}

void ACEClonerActor::ForEachEffector(TFunctionRef<bool(ACEEffectorActor*, int32)> InFunction)
{
	for (int32 Idx = 0; Idx < EffectorsWeak.Num(); Idx++)
	{
		ACEEffectorActor* Effector = EffectorsWeak[Idx].Get();
		if (!Effector)
		{
			continue;
		}

		if (!InFunction(Effector, Idx))
		{
			return;
		}
	}
}

void ACEClonerActor::OnEnabledChanged()
{
	if (bEnabled)
	{
		OnLayoutNameChanged();
	}
	else
	{
		ClonerComponent->DeactivateImmediate();
		ClonerComponent->SetAsset(nullptr);
	}
}

void ACEClonerActor::OnLayoutNameChanged()
{
	if (!ClonerComponent || !bEnabled)
	{
		return;
	}

	UCEClonerLayoutBase* NewActiveLayout = FindOrAddLayout(LayoutName);

	// Apply layout
	if (ClonerComponent->SetClonerActiveLayout(NewActiveLayout))
	{
		ActiveLayout = ClonerComponent->GetClonerActiveLayout();
		OnClonerSystemChanged();
	}
}

void ACEClonerActor::OnSpawnOptionsChanged()
{
	if (!ClonerComponent || !bEnabled)
	{
		return;
	}

	if (const UCEClonerLayoutBase* ActiveSystem = GetActiveLayout())
	{
		FNiagaraUserRedirectionParameterStore& ExposedParameters = ActiveSystem->GetSystem()->GetExposedParameters();

		ClonerComponent->SetFloatParameter(TEXT("SpawnLoopInterval"), SpawnLoopInterval);

		ClonerComponent->SetIntParameter(TEXT("SpawnLoopIterations"), SpawnLoopIterations);

		ClonerComponent->SetFloatParameter(TEXT("SpawnRate"), SpawnRate);

		const FNiagaraVariable SpawnBehaviorModeVar(FNiagaraTypeDefinition(StaticEnum<ECEClonerSpawnBehaviorMode>()), TEXT("SpawnBehaviorMode"));
		ExposedParameters.SetParameterValue<int32>(static_cast<int32>(SpawnBehaviorMode), SpawnBehaviorModeVar);

		const FNiagaraVariable SpawnLoopModeVar(FNiagaraTypeDefinition(StaticEnum<ECEClonerSpawnLoopMode>()), TEXT("SpawnLoopMode"));
		ExposedParameters.SetParameterValue<int32>(static_cast<int32>(SpawnLoopMode), SpawnLoopModeVar);

		RequestClonerUpdate();
	}
}

void ACEClonerActor::OnLifetimeScaleCurveChanged()
{
	if (const UNiagaraDataInterfaceCurve* LifetimeCurve = LifetimeScaleCurveDIWeak.Get())
	{
		LifetimeScaleCurve = LifetimeCurve->Curve;
		OnLifetimeOptionsChanged();
	}
}

void ACEClonerActor::OnLifetimeOptionsChanged()
{
	if (!ClonerComponent || !bEnabled)
	{
		return;
	}

	LifetimeMin = FMath::Max(0, LifetimeMin);
	LifetimeMax = FMath::Max(LifetimeMin, LifetimeMax);

	ClonerComponent->SetBoolParameter(TEXT("LifetimeEnabled"), bLifetimeEnabled);

	ClonerComponent->SetFloatParameter(TEXT("LifetimeMin"), LifetimeMin);

	ClonerComponent->SetFloatParameter(TEXT("LifetimeMax"), LifetimeMax);

	ClonerComponent->SetBoolParameter(TEXT("LifetimeScaleEnabled"), bLifetimeEnabled && bLifetimeScaleEnabled);

	if (const UCEClonerLayoutBase* ActiveSystem = GetActiveLayout())
	{
		const FNiagaraUserRedirectionParameterStore& ExposedParameters = ActiveSystem->GetSystem()->GetExposedParameters();

		static const FNiagaraVariable LifetimeScaleCurveVar(FNiagaraTypeDefinition(UNiagaraDataInterfaceCurve::StaticClass()), TEXT("LifetimeScaleCurve"));

#if WITH_EDITOR
		if (UNiagaraDataInterfaceCurve* LifetimeCurve = LifetimeScaleCurveDIWeak.Get())
		{
			LifetimeCurve->OnChanged().RemoveAll(this);
		}
#endif

		if (UNiagaraDataInterfaceCurve* LifetimeScaleCurveDI = Cast<UNiagaraDataInterfaceCurve>(ExposedParameters.GetDataInterface(LifetimeScaleCurveVar)))
		{
			LifetimeScaleCurveDIWeak = LifetimeScaleCurveDI;
			LifetimeScaleCurveDI->Curve = LifetimeScaleCurve;

#if WITH_EDITOR
			LifetimeScaleCurveDI->UpdateLUT();
			LifetimeScaleCurveDI->OnChanged().AddUObject(this, &ACEClonerActor::OnLifetimeScaleCurveChanged);
#endif
		}
	}

	RequestClonerUpdate();
}

void ACEClonerActor::OnClonerMeshUpdated(UCEClonerComponent* InClonerComponent)
{
	if (InClonerComponent == ClonerComponent)
	{
		RequestClonerUpdate();
	}
}

void ACEClonerActor::OnClonerSystemChanged()
{
	UpdateLayoutOptions();
}

void ACEClonerActor::OnEffectorIdentifierChanged(ACEEffectorActor* InEffector, int32 InOldIdentifier, int32 InNewIdentifier)
{
	if (EffectorsWeak.Contains(InEffector))
	{
		// Remove effector if it is unregistered from channel
		if (InNewIdentifier == INDEX_NONE)
		{
#if WITH_EDITOR
			Modify();
#endif

			EffectorsWeak.Remove(InEffector);
		}

		OnEffectorsChanged();
	}
}

void ACEClonerActor::OnEffectorRefreshCloner(ACEEffectorActor* InEffector)
{
	if (EffectorsWeak.Contains(InEffector))
	{
		RequestClonerUpdate();
	}
}

void ACEClonerActor::OnEffectorsChanged()
{
	const FCEClonerEffectorDataInterfaces* EffectorDataInterfaces = GetEffectorDataInterfaces();

	if (!EffectorDataInterfaces || !EffectorDataInterfaces->GetIndexArray())
	{
		return;
	}

	// Remove duplicates
	const TSet<TWeakObjectPtr<ACEEffectorActor>> SetEffectorsWeak(EffectorsWeak);
	EffectorsWeak = SetEffectorsWeak.Array();

	TArray<int32> EffectorIndexes;
	EffectorIndexes.Reserve(EffectorsWeak.Num());

	for (const TWeakObjectPtr<ACEEffectorActor>& EffectorWeak : EffectorsWeak)
	{
		if (const ACEEffectorActor* Effector = EffectorWeak.Get())
		{
			const int32 ChannelIdentifier = Effector->GetChannelIdentifier();

			if (ChannelIdentifier != INDEX_NONE)
			{
				EffectorIndexes.AddUnique(ChannelIdentifier);
			}
		}
	}

	TArray<int32>& EffectorIndexArray = EffectorDataInterfaces->GetIndexArray()->GetArrayReference();
	EffectorIndexArray.Empty(EffectorIndexes.Num());

	for (const int32 EffectorIndex : EffectorIndexes)
	{
		EffectorIndexArray.Add(EffectorIndex);
	}

	constexpr bool bImmediateUpdate = false;
	RequestClonerUpdate(bImmediateUpdate);
}

void ACEClonerActor::RequestClonerUpdate(bool bInImmediate)
{
	if (!bEnabled || !ClonerComponent)
	{
		return;
	}

	if (bInImmediate)
	{
		bNeedsRefresh = false;
		ClonerComponent->RefreshUserParameters();
	}
	else
	{
		bNeedsRefresh = true;
	}
}

void ACEClonerActor::OnRangeOptionsChanged()
{
	if (!ClonerComponent || !bEnabled)
	{
		return;
	}

	RangeScaleUniformMin = FMath::Clamp(RangeScaleUniformMin, UE_KINDA_SMALL_NUMBER, RangeScaleUniformMax);
	RangeScaleUniformMax = FMath::Max3(RangeScaleUniformMin, RangeScaleUniformMax, UE_KINDA_SMALL_NUMBER);

	RangeScaleMin.X = FMath::Clamp(RangeScaleMin.X, UE_KINDA_SMALL_NUMBER, RangeScaleMax.X);
	RangeScaleMin.Y = FMath::Clamp(RangeScaleMin.Y, UE_KINDA_SMALL_NUMBER, RangeScaleMax.Y);
	RangeScaleMin.Z = FMath::Clamp(RangeScaleMin.Z, UE_KINDA_SMALL_NUMBER, RangeScaleMax.Z);

	RangeScaleMax.X = FMath::Max3<double>(RangeScaleMin.X, RangeScaleMax.X, UE_KINDA_SMALL_NUMBER);
	RangeScaleMax.Y = FMath::Max3<double>(RangeScaleMin.Y, RangeScaleMax.Y, UE_KINDA_SMALL_NUMBER);
	RangeScaleMax.Z = FMath::Max3<double>(RangeScaleMin.Z, RangeScaleMax.Z, UE_KINDA_SMALL_NUMBER);

	ClonerComponent->SetBoolParameter(TEXT("RangeEnabled"), bRangeEnabled);

	ClonerComponent->SetVectorParameter(TEXT("RangeOffsetMin"), RangeOffsetMin);

	ClonerComponent->SetVectorParameter(TEXT("RangeOffsetMax"), RangeOffsetMax);

	ClonerComponent->SetVariableQuat(TEXT("RangeRotationMin"), RangeRotationMin.Quaternion());

	ClonerComponent->SetVariableQuat(TEXT("RangeRotationMax"), RangeRotationMax.Quaternion());

	ClonerComponent->SetBoolParameter(TEXT("RangeScaleUniform"), bRangeScaleUniform);

	ClonerComponent->SetVectorParameter(TEXT("RangeScaleMin"), RangeScaleMin);

	ClonerComponent->SetVectorParameter(TEXT("RangeScaleMax"), RangeScaleMax);

	ClonerComponent->SetFloatParameter(TEXT("RangeScaleUniformMin"), RangeScaleUniformMin);

	ClonerComponent->SetFloatParameter(TEXT("RangeScaleUniformMax"), RangeScaleUniformMax);

	RequestClonerUpdate();
}

bool ACEClonerActor::IsClonerValid() const
{
	return IsValid(ClonerComponent.Get()) && IsValid(ClonerComponent->GetAsset());
}

void ACEClonerActor::OnMeshRendererOptionsChanged()
{
	const UCEClonerLayoutBase* CurrentLayout = ClonerComponent->GetClonerActiveLayout();

	if (!CurrentLayout)
	{
		return;
	}

	UNiagaraMeshRendererProperties* MeshRenderer = CurrentLayout->GetMeshRenderer();

	if (!MeshRenderer)
	{
		return;
	}

	MeshRenderer->FacingMode = MeshFacingMode;
	MeshRenderer->bCastShadows = bMeshCastShadows;

	RequestClonerUpdate();
}

void ACEClonerActor::OnDefaultMeshesChanged()
{
	// Will force an update
	TreeUpdateDeltaTime = TreeUpdateInterval;
}

#if WITH_EDITOR
void ACEClonerActor::OnReduceMotionGhostingChanged()
{
	if (UCEClonerSubsystem* ClonerSubsystem = UCEClonerSubsystem::Get())
	{
		if (bReduceMotionGhosting)
		{
			ClonerSubsystem->EnableNoFlicker();
		}
		else
		{
			ClonerSubsystem->DisableNoFlicker();
		}
	}
}

void ACEClonerActor::OnCVarChanged()
{
	if (const UCEClonerSubsystem* ClonerSubsystem = UCEClonerSubsystem::Get())
	{
		bReduceMotionGhosting = ClonerSubsystem->IsNoFlickerEnabled();
	}
}

void ACEClonerActor::ForceUpdateCloner()
{
	if (ClonerComponent)
	{
		ClonerComponent->UpdateClonerAttachmentTree();
		ClonerComponent->UpdateClonerRenderState();

		OnLayoutNameChanged();
		OnEffectorsChanged();
	}
}

void ACEClonerActor::SpawnLinkedEffector()
{
	UWorld* ClonerWorld = GetWorld();
	if (!ClonerWorld)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.bTemporaryEditorActor = false;

	const FVector ClonerLocation = GetActorLocation();
	const FRotator ClonerRotation = GetActorRotation();

	ACEEffectorActor* EffectorActor = ClonerWorld->SpawnActor<ACEEffectorActor>(ACEEffectorActor::StaticClass(), ClonerLocation, ClonerRotation, Params);

	if (!EffectorActor)
	{
		return;
	}

	FActorLabelUtilities::RenameExistingActor(EffectorActor, EffectorActor->GetDefaultActorLabel(), true);

	// Set default offset for visual feedback
	EffectorActor->SetOffset(FVector(0, 0, 100));

	LinkEffector(EffectorActor);
}

void ACEClonerActor::SpawnDefaultActorAttached()
{
	if (!bSpawnDefaultActorAttached)
	{
		return;
	}

	bSpawnDefaultActorAttached = false;

	// Only spawn if world is valid and not a preview actor
	UWorld* World = GetWorld();
	if (!World || bIsEditorPreviewActor)
	{
		return;
	}

	// Only spawn if no actor is attached below it
	TArray<AActor*> AttachedActors;
	constexpr bool bReset = true;
	constexpr bool bRecursive = false;
	GetAttachedActors(AttachedActors, bReset, bRecursive);

	if (!AttachedActors.IsEmpty())
	{
		return;
	}

	// Find or load cube mesh
	constexpr const TCHAR* DefaultStaticMeshPath = TEXT("/Script/Engine.StaticMesh'/Engine/BasicShapes/Cube.Cube'");
	UStaticMesh* DefaultStaticMesh = LoadObject<UStaticMesh>(nullptr, DefaultStaticMeshPath);

	// Find or load default material
	constexpr const TCHAR* DefaultMaterialPath = TEXT("/Script/Engine.Material'/ClonerEffector/Materials/DefaultClonerMaterial.DefaultClonerMaterial'");
	UMaterialInterface* DefaultMaterial = LoadObject<UMaterialInterface>(nullptr, DefaultMaterialPath);

	if (!DefaultStaticMesh || !DefaultMaterial)
	{
		return;
	}

	// Spawn attached actor with same flags as this actor
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.ObjectFlags = GetFlags();
	SpawnParameters.bTemporaryEditorActor = false;

	if (AStaticMeshActor* DefaultActorAttached = World->SpawnActor<AStaticMeshActor>(GetActorLocation(), GetActorRotation(), SpawnParameters))
	{
		UStaticMeshComponent* StaticMeshComponent = DefaultActorAttached->GetStaticMeshComponent();
		StaticMeshComponent->SetStaticMesh(DefaultStaticMesh);
		StaticMeshComponent->SetMaterial(0, DefaultMaterial);

		DefaultActorAttached->SetMobility(EComponentMobility::Movable);
		DefaultActorAttached->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);

		FActorLabelUtilities::SetActorLabelUnique(DefaultActorAttached, TEXT("DefaultCube"));
	}
}

void ACEClonerActor::OnEditorSelectionChanged(UObject* InSelection)
{
	if (const USelection* ActorSelection = Cast<USelection>(InSelection))
	{
		if (ULevelEditorViewportSettings* ViewportSettings = GetMutableDefault<ULevelEditorViewportSettings>())
		{
			if (ActorSelection->Num() == 1 && ActorSelection->GetSelectedObject(0) == this)
			{
				UseSelectionOutline = ViewportSettings->bUseSelectionOutline;
				ViewportSettings->bUseSelectionOutline = false;
			}
			else if (UseSelectionOutline.IsSet())
			{
				ViewportSettings->bUseSelectionOutline = UseSelectionOutline.GetValue();
				UseSelectionOutline.Reset();
			}
		}
	}
}
#endif

void ACEClonerActor::OnVisualizerSpriteVisibleChanged()
{
#if WITH_EDITOR
	UE::ClonerEffector::SetBillboardComponentSprite(this, TEXT("/Script/Engine.Texture2D'/ClonerEffector/Textures/T_ClonerIcon.T_ClonerIcon'"));
	UE::ClonerEffector::SetBillboardComponentVisibility(this, bVisualizerSpriteVisible);
#endif
}

TArray<FString> ACEClonerActor::GetClonerLayoutNames() const
{
	TArray<FString> LayoutNamesStrings;

	if (const UCEClonerSubsystem* Subsystem = UCEClonerSubsystem::Get())
	{
		const TArray<FName>& LayoutNames = Subsystem->GetLayoutNames();
		LayoutNamesStrings.Reserve(LayoutNames.Num());

		Algo::Transform(LayoutNames, LayoutNamesStrings, [](const FName& InName)
		{
			return InName.ToString();
		});
	}

	return LayoutNamesStrings;
}

UCEClonerLayoutBase* ACEClonerActor::FindOrAddLayout(TSubclassOf<UCEClonerLayoutBase> InClass)
{
	const UCEClonerSubsystem* Subsystem = UCEClonerSubsystem::Get();

	if (!Subsystem)
	{
		return nullptr;
	}

	const FName ClassLayoutName = Subsystem->FindLayoutName(InClass);

	if (ClassLayoutName.IsNone())
	{
		return nullptr;
	}

	return FindOrAddLayout(ClassLayoutName);
}

UCEClonerLayoutBase* ACEClonerActor::FindOrAddLayout(FName InLayoutName)
{
	UCEClonerSubsystem* Subsystem = UCEClonerSubsystem::Get();
	if (!Subsystem)
	{
		return nullptr;
	}

	// Check cached layout instances
	UCEClonerLayoutBase* NewActiveLayout = nullptr;
	if (TObjectPtr<UCEClonerLayoutBase> const* LayoutInstance = LayoutInstances.Find(InLayoutName))
	{
		NewActiveLayout = *LayoutInstance;
	}

	// Create new layout instance and cache it
	if (!NewActiveLayout)
	{
		NewActiveLayout = Subsystem->CreateNewLayout(InLayoutName, this);
		LayoutInstances.Add(InLayoutName, NewActiveLayout);
	}

	return NewActiveLayout;
}

void ACEClonerActor::InitializeCloner()
{
	if (bClonerInitialized)
	{
		return;
	}

	OnVisualizerSpriteVisibleChanged();

#if WITH_EDITOR
	OnReduceMotionGhostingChanged();

	if (bSpawnDefaultActorAttached)
	{
		SpawnDefaultActorAttached();
	}
#endif

	// For new cloner instances no need to migrate anything
	bDeprecatedPropertiesMigrated = true;

	bClonerInitialized = true;

	OnLayoutNameChanged();
	OnEffectorsChanged();
}
