// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraLightRendererProperties.h"
#include "NiagaraRenderer.h"
#include "NiagaraConstants.h"
#include "NiagaraRendererLights.h"
#include "Modules/ModuleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraLightRendererProperties)

#if WITH_EDITOR
#include "Widgets/Images/SImage.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/SWidget.h"
#include "Styling/SlateBrush.h"
#include "AssetThumbnail.h"
#include "Widgets/Text/STextBlock.h"
#endif


#define LOCTEXT_NAMESPACE "UNiagaraLightRendererProperties"

TArray<TWeakObjectPtr<UNiagaraLightRendererProperties>> UNiagaraLightRendererProperties::LightRendererPropertiesToDeferredInit;

UNiagaraLightRendererProperties::UNiagaraLightRendererProperties()
	: bUseInverseSquaredFalloff(1)
	, bAffectsTranslucency(0)
	, bAlphaScalesBrightness(0)
	, RadiusScale(1.0f)
	, DefaultExponent(1.0f)
	, ColorAdd(FVector(0.0f, 0.0f, 0.0f))
{
	AttributeBindings.Reserve(7);
	AttributeBindings.Add(&LightRenderingEnabledBinding);
	AttributeBindings.Add(&LightExponentBinding);
	AttributeBindings.Add(&PositionBinding);
	AttributeBindings.Add(&ColorBinding);
	AttributeBindings.Add(&RadiusBinding);
	AttributeBindings.Add(&VolumetricScatteringBinding);
	AttributeBindings.Add(&RendererVisibilityTagBinding);
}

void UNiagaraLightRendererProperties::PostLoad()
{
	Super::PostLoad();
	
#if WITH_EDITORONLY_DATA
	ChangeToPositionBinding(PositionBinding);
#endif
	PostLoadBindings(ENiagaraRendererSourceDataMode::Particles);
}

void UNiagaraLightRendererProperties::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		// We can end up hitting PostInitProperties before the Niagara Module has initialized bindings this needs, mark this object for deferred init and early out.
		if (FModuleManager::Get().IsModuleLoaded("Niagara") == false)
		{
			LightRendererPropertiesToDeferredInit.Add(this);
			return;
		}
		else if (!PositionBinding.IsValid())
		{
			PositionBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_POSITION);
			ColorBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_COLOR);
			RadiusBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_LIGHT_RADIUS);
			LightExponentBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_LIGHT_EXPONENT);
			LightRenderingEnabledBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_LIGHT_ENABLED);
			VolumetricScatteringBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_LIGHT_VOLUMETRIC_SCATTERING);
			RendererVisibilityTagBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_VISIBILITY_TAG);
		}
	}
}

/** The bindings depend on variables that are created during the NiagaraModule startup. However, the CDO's are build prior to this being initialized, so we defer setting these values until later.*/
void UNiagaraLightRendererProperties::InitCDOPropertiesAfterModuleStartup()
{
	UNiagaraLightRendererProperties* CDO = CastChecked<UNiagaraLightRendererProperties>(UNiagaraLightRendererProperties::StaticClass()->GetDefaultObject());
	CDO->PositionBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_POSITION);
	CDO->ColorBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_COLOR);
	CDO->RadiusBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_LIGHT_RADIUS);
	CDO->LightExponentBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_LIGHT_EXPONENT);
	CDO->LightRenderingEnabledBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_LIGHT_ENABLED);
	CDO->VolumetricScatteringBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_LIGHT_VOLUMETRIC_SCATTERING);
	CDO->RendererVisibilityTagBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_VISIBILITY_TAG);

	for (TWeakObjectPtr<UNiagaraLightRendererProperties>& WeakLightRendererProperties : LightRendererPropertiesToDeferredInit)
	{
		if (WeakLightRendererProperties.Get())
		{
			if (!WeakLightRendererProperties->PositionBinding.IsValid())
			{
				WeakLightRendererProperties->PositionBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_POSITION);
				WeakLightRendererProperties->ColorBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_COLOR);
				WeakLightRendererProperties->RadiusBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_LIGHT_RADIUS);
				WeakLightRendererProperties->LightExponentBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_LIGHT_EXPONENT);
				WeakLightRendererProperties->LightRenderingEnabledBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_LIGHT_ENABLED);
				WeakLightRendererProperties->VolumetricScatteringBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_LIGHT_VOLUMETRIC_SCATTERING);
				WeakLightRendererProperties->RendererVisibilityTagBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_VISIBILITY_TAG);
			}
		}
	}
}

FNiagaraRenderer* UNiagaraLightRendererProperties::CreateEmitterRenderer(ERHIFeatureLevel::Type FeatureLevel, const FNiagaraEmitterInstance* Emitter, const FNiagaraSystemInstanceController& InController)
{
	FNiagaraRenderer* NewRenderer = new FNiagaraRendererLights(FeatureLevel, this, Emitter);
	NewRenderer->Initialize(this, Emitter, InController);
	return NewRenderer;
}

class FNiagaraBoundsCalculatorLights : public FNiagaraBoundsCalculator
{
public:
	FNiagaraBoundsCalculatorLights(const FNiagaraDataSetAccessor<FNiagaraPosition>& InPositionAccessor, const FNiagaraDataSetAccessor<float>& InRadiusAccessor)
		: PositionAccessor(InPositionAccessor)
		, RadiusAccessor(InRadiusAccessor)
	{}

	FNiagaraBoundsCalculatorLights() = delete;
	virtual ~FNiagaraBoundsCalculatorLights() = default;

	virtual void InitAccessors(const FNiagaraDataSetCompiledData* CompiledData) override {}
	virtual FBox CalculateBounds(const FTransform& SystemTransform, const FNiagaraDataSet& DataSet, const int32 NumInstances) const override
	{
		if (!NumInstances || !PositionAccessor.IsValid())
		{
			return FBox(ForceInit);
		}

		FNiagaraPosition BoundsMin(ForceInitToZero);
		FNiagaraPosition BoundsMax(ForceInitToZero);
		PositionAccessor.GetReader(DataSet).GetMinMax(BoundsMin, BoundsMax);
		FBox Bounds(BoundsMin, BoundsMax);

		if (RadiusAccessor.IsValid())
		{
			return Bounds.ExpandBy(RadiusAccessor.GetReader(DataSet).GetMax());
		}

		const FNiagaraVariable DefaultRadius = FNiagaraConstants::GetAttributeWithDefaultValue(SYS_PARAM_PARTICLES_LIGHT_RADIUS);

		return Bounds.ExpandBy(DefaultRadius.GetValue<float>());
	}

protected:
	const FNiagaraDataSetAccessor<FNiagaraPosition>& PositionAccessor;
	const FNiagaraDataSetAccessor<float>& RadiusAccessor;
};

FNiagaraBoundsCalculator* UNiagaraLightRendererProperties::CreateBoundsCalculator()
{
	if (GetCurrentSourceMode() == ENiagaraRendererSourceDataMode::Emitter)
	{
		return nullptr;
	}

	return new FNiagaraBoundsCalculatorLights(PositionDataSetAccessor, RadiusDataSetAccessor);
}

void UNiagaraLightRendererProperties::GetUsedMaterials(const FNiagaraEmitterInstance* InEmitter, TArray<UMaterialInterface*>& OutMaterials) const
{
	//OutMaterials.Add(Material);
	//Material should live here.
}

void UNiagaraLightRendererProperties::CacheFromCompiledData(const FNiagaraDataSetCompiledData* CompiledData)
{
	PositionDataSetAccessor.Init(CompiledData, PositionBinding.GetDataSetBindableVariable().GetName());
	ColorDataSetAccessor.Init(CompiledData, ColorBinding.GetDataSetBindableVariable().GetName());
	RadiusDataSetAccessor.Init(CompiledData, RadiusBinding.GetDataSetBindableVariable().GetName());
	ExponentDataSetAccessor.Init(CompiledData, LightExponentBinding.GetDataSetBindableVariable().GetName());
	ScatteringDataSetAccessor.Init(CompiledData, VolumetricScatteringBinding.GetDataSetBindableVariable().GetName());
	EnabledDataSetAccessor.Init(CompiledData, LightRenderingEnabledBinding.GetDataSetBindableVariable().GetName());
	RendererVisibilityTagAccessor.Init(CompiledData, RendererVisibilityTagBinding.GetDataSetBindableVariable().GetName());
}

#if WITH_EDITORONLY_DATA

const TArray<FNiagaraVariable>& UNiagaraLightRendererProperties::GetOptionalAttributes()
{
	static TArray<FNiagaraVariable> Attrs;
	if (Attrs.Num() == 0)
	{
		Attrs.Add(SYS_PARAM_PARTICLES_POSITION);
		Attrs.Add(SYS_PARAM_PARTICLES_COLOR);
		Attrs.Add(SYS_PARAM_PARTICLES_LIGHT_RADIUS);
		Attrs.Add(SYS_PARAM_PARTICLES_LIGHT_EXPONENT);
		Attrs.Add(SYS_PARAM_PARTICLES_LIGHT_ENABLED);
		Attrs.Add(SYS_PARAM_PARTICLES_LIGHT_VOLUMETRIC_SCATTERING);
		Attrs.Add(SYS_PARAM_PARTICLES_VISIBILITY_TAG);
	}
	return Attrs;
}

void UNiagaraLightRendererProperties::GetRendererWidgets(const FNiagaraEmitterInstance* InEmitter, TArray<TSharedPtr<SWidget>>& OutWidgets, TSharedPtr<FAssetThumbnailPool> InThumbnailPool) const
{
	TSharedRef<SWidget> LightWidget = SNew(SImage)
		.Image(FSlateIconFinder::FindIconBrushForClass(GetClass()));
	OutWidgets.Add(LightWidget);
}

void UNiagaraLightRendererProperties::GetRendererTooltipWidgets(const FNiagaraEmitterInstance* InEmitter, TArray<TSharedPtr<SWidget>>& OutWidgets, TSharedPtr<FAssetThumbnailPool> InThumbnailPool) const
{
	TSharedRef<SWidget> LightTooltip = SNew(STextBlock)
		.Text(LOCTEXT("LightRenderer", "Light Renderer"));
	OutWidgets.Add(LightTooltip);
}

void UNiagaraLightRendererProperties::GetRendererFeedback(const FVersionedNiagaraEmitter& InEmitter, TArray<FText>& OutErrors, TArray<FText>& OutWarnings, TArray<FText>& OutInfo) const
{
	Super::GetRendererFeedback(InEmitter, OutErrors, OutWarnings, OutInfo);
}

#endif // WITH_EDITORONLY_DATA

#undef LOCTEXT_NAMESPACE
