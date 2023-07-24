// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraRendererDecals.h"

#include "NiagaraCullProxyComponent.h"
#include "NiagaraDecalRendererProperties.h"
#include "NiagaraEmitter.h"
#include "NiagaraSceneProxy.h"
#include "NiagaraSystemInstance.h"

#include "Async/Async.h"
#include "Components/LineBatchComponent.h"
#include "Engine/World.h"
#include "SceneInterface.h"

namespace NiagaraRendererDecalsLocal
{
	static bool GRendererEnabled = true;
	static FAutoConsoleVariableRef CVarRendererEnabled(
		TEXT("fx.Niagara.DecalRenderer.Enabled"),
		GRendererEnabled,
		TEXT("If == 0, Niagara Decal Renderers are disabled."),
		ECVF_Default
	);

#define NIAGARARENDERERDECALS_DEBUG_ENABLED !UE_BUILD_SHIPPING

#if NIAGARARENDERERDECALS_DEBUG_ENABLED
	static bool GDrawDebug = false;
	static FAutoConsoleVariableRef CVarDrawDebug(
		TEXT("fx.Niagara.DecalRenderer.DrawDebug"),
		GDrawDebug,
		TEXT("When none zero will draw debug information."),
		ECVF_Default
	);

	void DrawDebugDecal(UWorld* World, TConstArrayView<FDeferredDecalUpdateParams> AllUpdateParams, double WorldTime)
	{
		if (GDrawDebug == false)
		{
			return;
		}

		// Line Batcher is not thread safe and we could be on any thread, therefore send a task to draw the information safely
		AsyncTask(
			ENamedThreads::GameThread,
			[WeakWorld=MakeWeakObjectPtr(World), AllUpdateParams_GT=TArray<FDeferredDecalUpdateParams>(AllUpdateParams), WorldTime]
			{
				UWorld* World = WeakWorld.Get();
				if (World == nullptr || World->LineBatcher == nullptr)
				{
					return;
				}
				ULineBatchComponent* LineBatcher = World->LineBatcher;
				
				for (const FDeferredDecalUpdateParams& UpdateParams : AllUpdateParams_GT)
				{
					if ( UpdateParams.OperationType == FDeferredDecalUpdateParams::EOperationType::RemoveFromSceneAndDelete )
					{
						continue;
					}

					const float Fade = (WorldTime - UpdateParams.FadeStartDelay) / UpdateParams.FadeDuration;
					const FBox BoundsBox(-FVector(1), FVector(1));
					//const uint8 IntFade = FMath::Clamp(uint8(255.0f * Fade), 0, 255);
					const FColor DecalColor = UpdateParams.DecalColor.ToFColor(false);
					LineBatcher->DrawSolidBox(BoundsBox, UpdateParams.Transform, DecalColor, 0, 0.0f);

					LineBatcher->DrawCircle(UpdateParams.Bounds.Origin, FVector::XAxisVector, FVector::YAxisVector, FColor::Red, UpdateParams.Bounds.SphereRadius, 16, 0);
					LineBatcher->DrawCircle(UpdateParams.Bounds.Origin, FVector::XAxisVector, FVector::ZAxisVector, FColor::Red, UpdateParams.Bounds.SphereRadius, 16, 0);
					LineBatcher->DrawCircle(UpdateParams.Bounds.Origin, FVector::YAxisVector, FVector::ZAxisVector, FColor::Red, UpdateParams.Bounds.SphereRadius, 16, 0);
				}
			}
		);
	}
#endif
}

FNiagaraRendererDecals::FNiagaraRendererDecals(ERHIFeatureLevel::Type FeatureLevel, const UNiagaraRendererProperties *InProps, const FNiagaraEmitterInstance* Emitter)
	: FNiagaraRenderer(FeatureLevel, InProps, Emitter)
{
}

FNiagaraRendererDecals::~FNiagaraRendererDecals()
{
	check(ActiveDecalProxies.Num() == 0);
}

void FNiagaraRendererDecals::ReleaseAllDecals() const
{
	if (ActiveDecalProxies.Num() > 0)
	{
		USceneComponent* OwnerComponent = WeakOwnerComponent.Get();
		UWorld* World = OwnerComponent ? OwnerComponent->GetWorld() : nullptr;
		if (World && World->Scene)
		{
			TArray<FDeferredDecalUpdateParams> DecalUpdates;
			DecalUpdates.AddDefaulted(ActiveDecalProxies.Num());
			for (int i = 0; i < ActiveDecalProxies.Num(); ++i)
			{
				DecalUpdates[i].OperationType = FDeferredDecalUpdateParams::EOperationType::RemoveFromSceneAndDelete;
				DecalUpdates[i].DecalProxy = ActiveDecalProxies[i];
			}

			World->Scene->BatchUpdateDecals(MoveTemp(DecalUpdates));
			ActiveDecalProxies.Empty();
		}
	}
}

void FNiagaraRendererDecals::DestroyRenderState_Concurrent()
{
	ReleaseAllDecals();

	ensureMsgf(ActiveDecalProxies.Num() == 0, TEXT("ActiveDecalProxies have been leaked?"));
	ActiveDecalProxies.Empty();
}

FPrimitiveViewRelevance FNiagaraRendererDecals::GetViewRelevance(const FSceneView* View, const FNiagaraSceneProxy *SceneProxy)const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = false;
	Result.bShadowRelevance = false;
	Result.bDynamicRelevance = false;
	Result.bOpaque = false;
	Result.bHasSimpleLights = false;

	return Result;
}

FNiagaraDynamicDataBase* FNiagaraRendererDecals::GenerateDynamicData(const FNiagaraSceneProxy* Proxy, const UNiagaraRendererProperties* InProperties, const FNiagaraEmitterInstance* Emitter) const
{
	using namespace NiagaraRendererDecalsLocal;

	// Get DataToRender
	const FNiagaraDataSet& DataSet = Emitter->GetData();
	const FNiagaraDataBuffer* DataToRender = DataSet.GetCurrentData();
	FNiagaraSystemInstance* SystemInstance = Emitter->GetParentSystemInstance();
	if (DataToRender == nullptr || SystemInstance == nullptr || !IsRendererEnabled(InProperties, Emitter))
	{
		ReleaseAllDecals();
		return nullptr;
	}

	UWorld* World = SystemInstance->GetWorld();
	check(World && World->Scene);

	USceneComponent* OwnerComponent = SystemInstance->GetAttachComponent();
	check(OwnerComponent);

	WeakOwnerComponent = OwnerComponent;
	const UNiagaraDecalRendererProperties* RendererProperties = CastChecked<const UNiagaraDecalRendererProperties>(InProperties);

	const bool bUseLocalSpace = UseLocalSpace(Proxy);
	const FTransform LocalToWorld = SystemInstance->GetWorldTransform();
	const FNiagaraLWCConverter LwcConverter = SystemInstance->GetLWCConverter(bUseLocalSpace);
	const FNiagaraPosition DefaultPos = bUseLocalSpace ? FVector::ZeroVector : LocalToWorld.GetLocation();
	const FQuat4f DefaultRot = UNiagaraDecalRendererProperties::GetDefaultOrientation();
	const FVector3f DefaultSize = UNiagaraDecalRendererProperties::GetDefaultDecalSize();
	const FNiagaraBool DefaultVisible = UNiagaraDecalRendererProperties::GetDefaultDecalVisible();
	const float DefaultFade = UNiagaraDecalRendererProperties::GetDefaultDecalFade();
	const int32 DefaultSortOrder = UNiagaraDecalRendererProperties::GetDefaultDecalSortOrder();

	// Check for Material Update
	UMaterialInterface* Material = BaseMaterials_GT.Num() ? BaseMaterials_GT[0] : nullptr;
	if (Material != WeakMaterial.Get())
	{
		WeakMaterial = Material;
		ReleaseAllDecals();
	}

	if (RendererProperties->MaterialParameters.HasAnyBindings())
	{
		ProcessMaterialParameterBindings(RendererProperties->MaterialParameters, Emitter, MakeArrayView(BaseMaterials_GT));
	}

	// Generate list of decals to update
	int NumActiveDecalProxies = 0;

	TArray<FDeferredDecalUpdateParams> DecalUpdates;
	DecalUpdates.Reserve(DataToRender->GetNumInstances());

	// Particles Source mode?
	if (RendererProperties->SourceMode == ENiagaraRendererSourceDataMode::Particles)
	{
		// Create all our data readers
		const auto PositionReader = RendererProperties->PositionDataSetAccessor.GetReader(DataSet);
		const auto RotationReader = RendererProperties->DecalOrientationDataSetAccessor.GetReader(DataSet);
		const auto SizeReader = RendererProperties->DecalSizeDataSetAccessor.GetReader(DataSet);
		const auto ColorReader = RendererProperties->DecalColorDataSetAccessor.GetReader(DataSet);
		const auto FadeReader = RendererProperties->DecalFadeDataSetAccessor.GetReader(DataSet);
		const auto SortOrderReader = RendererProperties->DecalSortOrderDataSetAccessor.GetReader(DataSet);
		const auto VisibleReader = RendererProperties->DecalVisibleAccessor.GetReader(DataSet);
		const auto VisTagReader = RendererProperties->RendererVisibilityTagAccessor.GetReader(DataSet);

		for (uint32 ParticleIndex = 0; ParticleIndex < DataToRender->GetNumInstances(); ++ParticleIndex)
		{
			// Check if the decal is visible
			const bool bVisible = VisibleReader.GetSafe(ParticleIndex, DefaultVisible).GetValue();
			const int32 VisTag = VisTagReader.GetSafe(ParticleIndex, RendererProperties->RendererVisibility);
			if (!bVisible || VisTag != RendererProperties->RendererVisibility)
			{
				continue;
			}

			// Grab Decal Attributes
			const FVector3f SimPos = PositionReader.GetSafe(ParticleIndex, DefaultPos);
			const FQuat4f SimRot = RotationReader.GetSafe(ParticleIndex, DefaultRot).GetNormalized();
			const FVector Position = bUseLocalSpace ? LocalToWorld.TransformPosition(FVector(SimPos)) : LwcConverter.ConvertSimulationPositionToWorld(SimPos);
			const FQuat Rotation = bUseLocalSpace ? LocalToWorld.TransformRotation(FQuat(SimRot)) : FQuat(SimRot);
			const FVector Size = FVector(SizeReader.GetSafe(ParticleIndex, DefaultSize) * 0.5f);
			const FLinearColor DecalColor = ColorReader.GetSafe(ParticleIndex, FLinearColor::White);
			const float Fade = FadeReader.GetSafe(ParticleIndex, DefaultFade);
			const int32 SortOrder = SortOrderReader.GetSafe(ParticleIndex, DefaultSortOrder);

			// Create Update Parameters
			FDeferredDecalUpdateParams& UpdateParams = DecalUpdates.AddDefaulted_GetRef();
			if (NumActiveDecalProxies < ActiveDecalProxies.Num())
			{
				UpdateParams.DecalProxy = ActiveDecalProxies[NumActiveDecalProxies];
				UpdateParams.OperationType = FDeferredDecalUpdateParams::EOperationType::Update;
			}
			else
			{
				UpdateParams.DecalProxy = new FDeferredDecalProxy(OwnerComponent, Material);
				UpdateParams.OperationType = FDeferredDecalUpdateParams::EOperationType::AddToSceneAndUpdate;
				ActiveDecalProxies.Add(UpdateParams.DecalProxy);
			}
			++NumActiveDecalProxies;

			UpdateParams.Transform = FTransform(Rotation, Position, FVector(Size));
			UpdateParams.Bounds = FBoxSphereBounds(FSphere(Position, Size.GetAbsMax() * 2.0));
			UpdateParams.AbsSpawnTime = World->TimeSeconds - FMath::Clamp(1.0f - Fade, 0.0f, 1.0f);
			UpdateParams.FadeStartDelay = 0.0f;
			UpdateParams.FadeDuration = 1.0f;
			UpdateParams.FadeScreenSize = RendererProperties->DecalScreenSizeFade;
			UpdateParams.DecalColor = DecalColor;
			UpdateParams.SortOrder = SortOrder;
		}
	}
	// Emitter source mode
	else
	{
		//-OPT: Emitter mode we should be able to cache the parameter offset rather than search each frame
		const FNiagaraParameterStore& ParameterStore = Emitter->GetRendererBoundVariables();
		const bool bVisible = ParameterStore.GetParameterValueOrDefault(RendererProperties->DecalVisibleBinding.GetParamMapBindableVariable(), DefaultVisible).GetValue();
		const int32 VisTag = ParameterStore.GetParameterValueOrDefault(RendererProperties->RendererVisibilityTagBinding.GetParamMapBindableVariable(), RendererProperties->RendererVisibility);
		if (bVisible && VisTag == RendererProperties->RendererVisibility)
		{
			const FVector3f SimPos = ParameterStore.GetParameterValueOrDefault(RendererProperties->PositionBinding.GetParamMapBindableVariable(), DefaultPos);
			const FQuat4f SimRot = ParameterStore.GetParameterValueOrDefault(RendererProperties->DecalOrientationBinding.GetParamMapBindableVariable(), DefaultRot).GetNormalized();
			const FVector Position = bUseLocalSpace ? LocalToWorld.TransformPosition(FVector(SimPos)) : LwcConverter.ConvertSimulationPositionToWorld(SimPos);
			const FQuat Rotation = bUseLocalSpace ? LocalToWorld.TransformRotation(FQuat(SimRot)) : FQuat(SimRot);
			const FVector Size = FVector(ParameterStore.GetParameterValueOrDefault(RendererProperties->DecalSizeBinding.GetParamMapBindableVariable(), DefaultSize) * 0.5f);
			const FLinearColor DecalColor = ParameterStore.GetParameterValueOrDefault(RendererProperties->DecalColorBinding.GetParamMapBindableVariable(), FLinearColor::White);
			const float Fade = ParameterStore.GetParameterValueOrDefault(RendererProperties->DecalFadeBinding.GetParamMapBindableVariable(), DefaultFade);
			const int32 SortOrder = ParameterStore.GetParameterValueOrDefault(RendererProperties->DecalSortOrderBinding.GetParamMapBindableVariable(), DefaultSortOrder);

			// Create Update Parameters
			FDeferredDecalUpdateParams& UpdateParams = DecalUpdates.AddDefaulted_GetRef();
			if (NumActiveDecalProxies < ActiveDecalProxies.Num())
			{
				UpdateParams.DecalProxy = ActiveDecalProxies[NumActiveDecalProxies];
				UpdateParams.OperationType = FDeferredDecalUpdateParams::EOperationType::Update;
			}
			else
			{
				UpdateParams.DecalProxy = new FDeferredDecalProxy(OwnerComponent, Material);
				UpdateParams.OperationType = FDeferredDecalUpdateParams::EOperationType::AddToSceneAndUpdate;
				ActiveDecalProxies.Add(UpdateParams.DecalProxy);
			}
			++NumActiveDecalProxies;

			UpdateParams.Transform = FTransform(Rotation, Position, FVector(Size));
			UpdateParams.Bounds = FBoxSphereBounds(FSphere(Position, Size.GetAbsMax() * 2.0));
			UpdateParams.AbsSpawnTime = World->TimeSeconds - FMath::Clamp(1.0f - Fade, 0.0f, 1.0f);
			UpdateParams.FadeStartDelay = 0.0f;
			UpdateParams.FadeDuration = 1.0f;
			UpdateParams.FadeScreenSize = RendererProperties->DecalScreenSizeFade;
			UpdateParams.DecalColor = DecalColor;
			UpdateParams.SortOrder = SortOrder;
		}
	}

	// Remove any unused decals
	while (ActiveDecalProxies.Num() > NumActiveDecalProxies)
	{
		FDeferredDecalUpdateParams& UpdateParams = DecalUpdates.AddDefaulted_GetRef();
		UpdateParams.OperationType = FDeferredDecalUpdateParams::EOperationType::RemoveFromSceneAndDelete;
		UpdateParams.DecalProxy = ActiveDecalProxies.Pop();
	}

	// Send updates to RT
	if (DecalUpdates.Num() > 0)
	{
	#if NIAGARARENDERERDECALS_DEBUG_ENABLED
		DrawDebugDecal(World, DecalUpdates, World->TimeSeconds);
	#endif
		World->Scene->BatchUpdateDecals(MoveTemp(DecalUpdates));
	}

	return nullptr;
}
