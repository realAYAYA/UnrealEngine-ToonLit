// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraRendererLights.h"
#include "ParticleResources.h"
#include "NiagaraDataSet.h"
#include "NiagaraDataSetAccessor.h"
#include "NiagaraSettings.h"
#include "NiagaraStats.h"
#include "NiagaraVertexFactory.h"
#include "NiagaraComponent.h"
#include "Engine/Engine.h"

#include "NiagaraLightRendererProperties.h"
#include "NiagaraRendererLights.h"
#include "NiagaraCullProxyComponent.h"


DECLARE_CYCLE_STAT(TEXT("Generate Particle Lights"), STAT_NiagaraGenLights, STATGROUP_Niagara);

struct FNiagaraDynamicDataLights : public FNiagaraDynamicDataBase
{
	FNiagaraDynamicDataLights(const FNiagaraEmitterInstance* InEmitter)
		: FNiagaraDynamicDataBase(InEmitter)
	{
	}

	TArray<FNiagaraRendererLights::SimpleLightData> LightArray;
};

//////////////////////////////////////////////////////////////////////////


FNiagaraRendererLights::FNiagaraRendererLights(ERHIFeatureLevel::Type FeatureLevel, const UNiagaraRendererProperties *InProps, const FNiagaraEmitterInstance* Emitter)
	: FNiagaraRenderer(FeatureLevel, InProps, Emitter)
{
	// todo - for platforms where we know we can't support deferred shading we can just set this to false
	bHasLights = true;
}

FPrimitiveViewRelevance FNiagaraRendererLights::GetViewRelevance(const FSceneView* View, const FNiagaraSceneProxy *SceneProxy)const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = bHasLights && SceneProxy->IsShown(View) && View->Family->EngineShowFlags.Particles && View->Family->EngineShowFlags.Niagara;
	Result.bShadowRelevance = false;
	Result.bDynamicRelevance = false;
	Result.bOpaque = false;
	Result.bHasSimpleLights = bHasLights;

	return Result;
}

/** Update render data buffer from attributes */
FNiagaraDynamicDataBase* FNiagaraRendererLights::GenerateDynamicData(const FNiagaraSceneProxy* Proxy, const UNiagaraRendererProperties* InProperties, const FNiagaraEmitterInstance* Emitter) const
{
	// particle (simple) lights are only supported with deferred shading
	if (!bHasLights || Proxy->GetScene().GetShadingPath() != EShadingPath::Deferred)
	{
		return nullptr;
	}

	SCOPE_CYCLE_COUNTER(STAT_NiagaraGenLights);

	//Bail if we don't have the required attributes to render this emitter.
	const UNiagaraLightRendererProperties* Properties = CastChecked<const UNiagaraLightRendererProperties>(InProperties);
	FNiagaraDataSet& Data = Emitter->GetData();
	FNiagaraDataBuffer* DataToRender = Data.GetCurrentData();
	if (DataToRender == nullptr || Emitter->GetParentSystemInstance() == nullptr)
	{
		return nullptr;
	}

	if (Properties->bAllowInCullProxies == false)
	{
		check(Emitter);

		FNiagaraSystemInstance* Inst = Emitter->GetParentSystemInstance();
		check(Emitter->GetParentSystemInstance());

		//TODO: Probably should push some state into the system instance for this?
		bool bIsCullProxy = Cast<UNiagaraCullProxyComponent>(Inst->GetAttachComponent()) != nullptr;
		if (bIsCullProxy)
		{
			return nullptr;
		}
	}

	FNiagaraSystemInstance* SystemInstance = Emitter->GetParentSystemInstance();
	FNiagaraDynamicDataLights* DynamicData = new FNiagaraDynamicDataLights(Emitter);

	//I'm not a great fan of pulling scalar components out to a structured vert buffer like this.
	//TODO: Experiment with a new VF that reads the data directly from the scalar layout.
	const auto PositionReader = Properties->PositionDataSetAccessor.GetReader(Data);
	const auto ColorReader = Properties->ColorDataSetAccessor.GetReader(Data);
	const auto RadiusReader = Properties->RadiusDataSetAccessor.GetReader(Data);
	const auto ExponentReader = Properties->ExponentDataSetAccessor.GetReader(Data);
	const auto ScatteringReader = Properties->ScatteringDataSetAccessor.GetReader(Data);
	const auto EnabledReader = Properties->EnabledDataSetAccessor.GetReader(Data);
	const auto VisTagReader = Properties->RendererVisibilityTagAccessor.GetReader(Data);

	// This used to use Proxy->GetLocalToWorld(), but that's a bad thing to do here, because the proxy gets updated on the render thread,
	// and this function happens during EndOfFrame updates. So instead, use the most up-to-date transform here (fixes local-space frame-behind issues)
	FTransform LocalToWorld = SystemInstance->GetWorldTransform();

	bool bUseLocalSpace = UseLocalSpace(Proxy);
	FNiagaraLWCConverter LwcConverter = SystemInstance->GetLWCConverter(bUseLocalSpace);
	const FLinearColor DefaultColor = Properties->ColorBinding.GetDefaultValue<FLinearColor>();
	const FNiagaraPosition DefaultPos = bUseLocalSpace ? FVector::ZeroVector : LocalToWorld.GetLocation();
	const float DefaultRadius = Properties->RadiusBinding.GetDefaultValue<float>();
	const float DefaultScattering = Properties->VolumetricScatteringBinding.GetDefaultValue<float>();
	const FNiagaraBool DefaultEnabled(true);
	const int32 DefaultVisibilityTag(0);
	const float DefaultExponent = Properties->DefaultExponent;

	const float InverseExposureBlend = Properties->bOverrideInverseExposureBlend ? Properties->InverseExposureBlend : GetDefault<UNiagaraSettings>()->DefaultLightInverseExposureBlend;

	for (uint32 ParticleIndex = 0; ParticleIndex < DataToRender->GetNumInstances(); ParticleIndex++)
	{
		bool bShouldRenderParticleLight = EnabledReader.GetSafe(ParticleIndex, DefaultEnabled).GetValue();
		if (bShouldRenderParticleLight && VisTagReader.IsValid())
		{
			bShouldRenderParticleLight = VisTagReader.GetSafe(ParticleIndex, DefaultVisibilityTag) == Properties->RendererVisibility;
		}
		float LightRadius = RadiusReader.GetSafe(ParticleIndex, DefaultRadius) * Properties->RadiusScale;
		if (bShouldRenderParticleLight && LightRadius > 0)
		{
			SimpleLightData& LightData = DynamicData->LightArray.AddDefaulted_GetRef();

			const FLinearColor Color = ColorReader.GetSafe(ParticleIndex, DefaultColor);
			const float Brightness = Properties->bAlphaScalesBrightness ? Color.A : 1.0f;

			LightData.LightEntry.Radius = LightRadius;
			LightData.LightEntry.Color = FVector3f(Color) * Brightness + Properties->ColorAdd;
			LightData.LightEntry.Exponent = Properties->bUseInverseSquaredFalloff ? 0 : ExponentReader.GetSafe(ParticleIndex, DefaultExponent);
			LightData.LightEntry.InverseExposureBlend = InverseExposureBlend;
			LightData.LightEntry.bAffectTranslucency = Properties->bAffectsTranslucency;
			LightData.LightEntry.VolumetricScatteringIntensity = ScatteringReader.GetSafe(ParticleIndex, DefaultScattering);
			LightData.PerViewEntry.Position = LwcConverter.ConvertSimulationPositionToWorld(PositionReader.GetSafe(ParticleIndex, DefaultPos));
			if (bUseLocalSpace)
			{
				LightData.PerViewEntry.Position = LocalToWorld.TransformPosition(LightData.PerViewEntry.Position);
			}
		}
	}

	return DynamicData;
}

void FNiagaraRendererLights::GatherSimpleLights(FSimpleLightArray& OutParticleLights)const
{
	if (const FNiagaraDynamicDataLights* DynamicData = static_cast<const FNiagaraDynamicDataLights*>(DynamicDataRender))
	{
		const int32 LightCount = DynamicData->LightArray.Num();

		OutParticleLights.InstanceData.Reserve(OutParticleLights.InstanceData.Num() + LightCount);
		OutParticleLights.PerViewData.Reserve(OutParticleLights.PerViewData.Num() + LightCount);

		for (const FNiagaraRendererLights::SimpleLightData &LightData : DynamicData->LightArray)
		{
			// When not using camera-offset, output one position for all views to share.
			OutParticleLights.PerViewData.Add(LightData.PerViewEntry);

			// Add an entry for the light instance.
			OutParticleLights.InstanceData.Add(LightData.LightEntry);
		}
	}
}
