// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraRendererRibbons.h"
#include "NiagaraConstants.h"
#include "NiagaraBoundsCalculatorHelper.h"
#include "NiagaraCustomVersion.h"
#include "NiagaraEmitterInstance.h"
#include "Modules/ModuleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraRibbonRendererProperties)

#if WITH_EDITOR
#include "Widgets/Images/SImage.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/SWidget.h"
#include "AssetThumbnail.h"
#include "Widgets/Text/STextBlock.h"
#endif

#define LOCTEXT_NAMESPACE "UNiagaraRibbonRendererProperties"

TArray<TWeakObjectPtr<UNiagaraRibbonRendererProperties>> UNiagaraRibbonRendererProperties::RibbonRendererPropertiesToDeferredInit;


FNiagaraRibbonShapeCustomVertex::FNiagaraRibbonShapeCustomVertex()
	: Position(0.0f)
	, Normal(0.0f)
	, TextureV(0.0f)
{
}

FNiagaraRibbonUVSettings::FNiagaraRibbonUVSettings()
	: DistributionMode(ENiagaraRibbonUVDistributionMode::ScaledUsingRibbonSegmentLength)
	, LeadingEdgeMode(ENiagaraRibbonUVEdgeMode::Locked)
	, TrailingEdgeMode(ENiagaraRibbonUVEdgeMode::Locked)
	, TilingLength(100.0f)
	, Offset(FVector2D(0.0f, 0.0f))
	, Scale(FVector2D(1.0f, 1.0f))
	, bEnablePerParticleUOverride(false)
	, bEnablePerParticleVRangeOverride(false)
{
}

UNiagaraRibbonRendererProperties::UNiagaraRibbonRendererProperties()
	: Material(nullptr)
	, MaterialUserParamBinding(FNiagaraTypeDefinition(UMaterialInterface::StaticClass()))
	, FacingMode(ENiagaraRibbonFacingMode::Screen)
#if WITH_EDITORONLY_DATA
	, UV0TilingDistance_DEPRECATED(0.0f)
	, UV0Scale_DEPRECATED(FVector2D(1.0f, 1.0f))
	, UV0AgeOffsetMode_DEPRECATED(ENiagaraRibbonAgeOffsetMode::Scale)
	, UV1TilingDistance_DEPRECATED(0.0f)
	, UV1Scale_DEPRECATED(FVector2D(1.0f, 1.0f))
	, UV1AgeOffsetMode_DEPRECATED(ENiagaraRibbonAgeOffsetMode::Scale)
#endif
	, MaxNumRibbons(0)
	, bUseGPUInit(false)
	, Shape(ENiagaraRibbonShapeMode::Plane)
	, bEnableAccurateGeometry(false)
	, WidthSegmentationCount(1)
	, MultiPlaneCount(2)
	, TubeSubdivisions(3)
	, CurveTension(0.f)
	, TessellationMode(ENiagaraRibbonTessellationMode::Automatic)
	, TessellationFactor(16)
	, bUseConstantFactor(false)
	, TessellationAngle(15)
	, bScreenSpaceTessellation(true)
{
	AttributeBindings.Reserve(19);
	AttributeBindings.Add(&PositionBinding);
	AttributeBindings.Add(&ColorBinding);
	AttributeBindings.Add(&VelocityBinding);
	AttributeBindings.Add(&NormalizedAgeBinding);
	AttributeBindings.Add(&RibbonTwistBinding);
	AttributeBindings.Add(&RibbonWidthBinding);
	AttributeBindings.Add(&RibbonFacingBinding);
	AttributeBindings.Add(&RibbonIdBinding);
	AttributeBindings.Add(&RibbonLinkOrderBinding);
	
	AttributeBindings.Add(&MaterialRandomBinding);
	AttributeBindings.Add(&DynamicMaterialBinding);
	AttributeBindings.Add(&DynamicMaterial1Binding);
	AttributeBindings.Add(&DynamicMaterial2Binding);
	AttributeBindings.Add(&DynamicMaterial3Binding);
	AttributeBindings.Add(&RibbonUVDistance);
	AttributeBindings.Add(&U0OverrideBinding);
	AttributeBindings.Add(&V0RangeOverrideBinding);
	AttributeBindings.Add(&U1OverrideBinding);
	AttributeBindings.Add(&V1RangeOverrideBinding);

	AttributeBindings.Add(&PrevPositionBinding);
	AttributeBindings.Add(&PrevRibbonWidthBinding);
	AttributeBindings.Add(&PrevRibbonFacingBinding);
	AttributeBindings.Add(&PrevRibbonTwistBinding);
}

FNiagaraRenderer* UNiagaraRibbonRendererProperties::CreateEmitterRenderer(ERHIFeatureLevel::Type FeatureLevel, const FNiagaraEmitterInstance* Emitter, const FNiagaraSystemInstanceController& InController)
{
	FNiagaraRenderer* NewRenderer = new FNiagaraRendererRibbons(FeatureLevel, this, Emitter);
	NewRenderer->Initialize(this, Emitter, InController);
	return NewRenderer;
}

#if WITH_EDITORONLY_DATA
void UpgradeUVSettings(FNiagaraRibbonUVSettings& UVSettings, float TilingDistance, const FVector2D& Offset, const FVector2D& Scale)
{
	if (TilingDistance == 0)
	{
		UVSettings.LeadingEdgeMode = ENiagaraRibbonUVEdgeMode::SmoothTransition;
		UVSettings.TrailingEdgeMode = ENiagaraRibbonUVEdgeMode::SmoothTransition;
		UVSettings.DistributionMode = ENiagaraRibbonUVDistributionMode::ScaledUniformly;
	}
	else
	{
		UVSettings.LeadingEdgeMode = ENiagaraRibbonUVEdgeMode::Locked;
		UVSettings.TrailingEdgeMode = ENiagaraRibbonUVEdgeMode::Locked;
		UVSettings.DistributionMode = ENiagaraRibbonUVDistributionMode::TiledOverRibbonLength;
		UVSettings.TilingLength = TilingDistance;
	}
	UVSettings.Offset = Offset;
	UVSettings.Scale = Scale;
}
#endif

void UNiagaraRibbonRendererProperties::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	if (MaterialUserParamBinding.Parameter.GetType().GetClass() != UMaterialInterface::StaticClass())
	{
		FNiagaraTypeDefinition MaterialDef(UMaterialInterface::StaticClass());
		MaterialUserParamBinding.Parameter.SetType(MaterialDef);
	}

	const int32 NiagaraVer = GetLinkerCustomVersion(FNiagaraCustomVersion::GUID);
	if (NiagaraVer < FNiagaraCustomVersion::RibbonRendererUVRefactor)
	{
		UpgradeUVSettings(UV0Settings, UV0TilingDistance_DEPRECATED, UV0Offset_DEPRECATED, UV0Scale_DEPRECATED);
		UpgradeUVSettings(UV1Settings, UV1TilingDistance_DEPRECATED, UV1Offset_DEPRECATED, UV1Scale_DEPRECATED);
	}

	ChangeToPositionBinding(PositionBinding);
#endif
	
	PostLoadBindings(ENiagaraRendererSourceDataMode::Particles);

	if ( Material )
	{
		Material->ConditionalPostLoad();
	}

#if WITH_EDITORONLY_DATA
	if (MaterialParameterBindings_DEPRECATED.Num() > 0)
	{
		MaterialParameters.AttributeBindings = MaterialParameterBindings_DEPRECATED;
		MaterialParameterBindings_DEPRECATED.Empty();
	}
#endif
}

FNiagaraBoundsCalculator* UNiagaraRibbonRendererProperties::CreateBoundsCalculator()
{
	return new FNiagaraBoundsCalculatorHelper<false, false, true>();
}

void UNiagaraRibbonRendererProperties::GetUsedMaterials(const FNiagaraEmitterInstance* InEmitter, TArray<UMaterialInterface*>& OutMaterials) const
{
	UMaterialInterface* MaterialInterface = nullptr;
	if (InEmitter != nullptr)
	{
		MaterialInterface = Cast<UMaterialInterface>(InEmitter->FindBinding(MaterialUserParamBinding.Parameter));
	}

	OutMaterials.Add(MaterialInterface ? MaterialInterface : ToRawPtr(Material));
}

const FVertexFactoryType* UNiagaraRibbonRendererProperties::GetVertexFactoryType() const
{
	return &FNiagaraRibbonVertexFactory::StaticType;
}

bool UNiagaraRibbonRendererProperties::IsBackfaceCullingDisabled() const
{
	if (Shape == ENiagaraRibbonShapeMode::MultiPlane)
	{
		return !bEnableAccurateGeometry;
	}
	else
	{
		return true;
	}
}

#if WITH_EDITORONLY_DATA
TArray<FNiagaraVariable> UNiagaraRibbonRendererProperties::GetBoundAttributes() const 
{
	TArray<FNiagaraVariable> BoundAttributes = Super::GetBoundAttributes();
	BoundAttributes.Reserve(BoundAttributes.Num() + MaterialParameters.AttributeBindings.Num());

	for (const FNiagaraMaterialAttributeBinding& MaterialParamBinding : MaterialParameters.AttributeBindings)
	{
		BoundAttributes.AddUnique(MaterialParamBinding.GetParamMapBindableVariable());
	}
	return BoundAttributes;
}
#endif

bool UNiagaraRibbonRendererProperties::PopulateRequiredBindings(FNiagaraParameterStore& InParameterStore)
{
	bool bAnyAdded = Super::PopulateRequiredBindings(InParameterStore);

	for (const FNiagaraVariableAttributeBinding* Binding : AttributeBindings)
	{
		if (Binding && Binding->CanBindToHostParameterMap())
		{
			InParameterStore.AddParameter(Binding->GetParamMapBindableVariable(), false);
			bAnyAdded = true;
		}
	}

	for (FNiagaraMaterialAttributeBinding& MaterialParamBinding : MaterialParameters.AttributeBindings)
	{
		InParameterStore.AddParameter(MaterialParamBinding.GetParamMapBindableVariable(), false);
		bAnyAdded = true;
	}

	return bAnyAdded;
}

void UNiagaraRibbonRendererProperties::UpdateSourceModeDerivates(ENiagaraRendererSourceDataMode InSourceMode, bool bFromPropertyEdit)
{
	UNiagaraEmitter* SrcEmitter = GetTypedOuter<UNiagaraEmitter>();
	if (SrcEmitter)
	{
		for (FNiagaraMaterialAttributeBinding& MaterialParamBinding : MaterialParameters.AttributeBindings)
		{
			MaterialParamBinding.CacheValues(SrcEmitter);
		}
	}

	Super::UpdateSourceModeDerivates(InSourceMode, bFromPropertyEdit);
}

void UNiagaraRibbonRendererProperties::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		// We can end up hitting PostInitProperties before the Niagara Module has initialized bindings this needs, mark this object for deferred init and early out.
		if (FModuleManager::Get().IsModuleLoaded("Niagara") == false)
		{
			RibbonRendererPropertiesToDeferredInit.Add(this);
			return;
		}
		InitBindings();
	}
}

/** The bindings depend on variables that are created during the NiagaraModule startup. However, the CDO's are build prior to this being initialized, so we defer setting these values until later.*/
void UNiagaraRibbonRendererProperties::InitCDOPropertiesAfterModuleStartup()
{
	UNiagaraRibbonRendererProperties* CDO = CastChecked<UNiagaraRibbonRendererProperties>(UNiagaraRibbonRendererProperties::StaticClass()->GetDefaultObject());
	CDO->InitBindings();

	for (TWeakObjectPtr<UNiagaraRibbonRendererProperties>& WeakRibbonRendererProperties : RibbonRendererPropertiesToDeferredInit)
	{
		if (WeakRibbonRendererProperties.Get())
		{
			WeakRibbonRendererProperties->InitBindings();
		}
	}
}

void UNiagaraRibbonRendererProperties::InitBindings()
{
	if (!PositionBinding.IsValid())
	{
		PositionBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_POSITION);
		ColorBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_COLOR);
		VelocityBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_VELOCITY);
		DynamicMaterialBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM);
		DynamicMaterial1Binding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_1);
		DynamicMaterial2Binding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_2);
		DynamicMaterial3Binding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_3);
		NormalizedAgeBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_NORMALIZED_AGE);
		RibbonTwistBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_RIBBONTWIST);
		RibbonWidthBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_RIBBONWIDTH);
		RibbonFacingBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_RIBBONFACING);
		RibbonIdBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_RIBBONID);		
		RibbonLinkOrderBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_RIBBONLINKORDER);		
		MaterialRandomBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_MATERIAL_RANDOM);
		RibbonUVDistance = FNiagaraConstants::GetAttributeDefaultBinding(RIBBONUVDISTANCE);
		U0OverrideBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_RIBBONU0OVERRIDE);
		V0RangeOverrideBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_RIBBONV0RANGEOVERRIDE);
		U1OverrideBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_RIBBONU1OVERRIDE);
		V1RangeOverrideBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_RIBBONV1RANGEOVERRIDE);
	}

	SetPreviousBindings(FVersionedNiagaraEmitter());
}

void UNiagaraRibbonRendererProperties::SetPreviousBindings(const FVersionedNiagaraEmitter& SrcEmitter)
{
	PrevPositionBinding.SetAsPreviousValue(PositionBinding, SrcEmitter, ENiagaraRendererSourceDataMode::Particles);
	PrevRibbonWidthBinding.SetAsPreviousValue(RibbonWidthBinding, SrcEmitter, ENiagaraRendererSourceDataMode::Particles);
	PrevRibbonFacingBinding.SetAsPreviousValue(RibbonFacingBinding, SrcEmitter, ENiagaraRendererSourceDataMode::Particles);
	PrevRibbonTwistBinding.SetAsPreviousValue(RibbonTwistBinding, SrcEmitter, ENiagaraRendererSourceDataMode::Particles);
}

void UNiagaraRibbonRendererProperties::CacheFromCompiledData(const FNiagaraDataSetCompiledData* CompiledData)
{
	// Initialize accessors
	bSortKeyDataSetAccessorIsAge = false;
	SortKeyDataSetAccessor.Init(CompiledData, RibbonLinkOrderBinding.GetDataSetBindableVariable().GetName());
	if (!SortKeyDataSetAccessor.IsValid())
	{
		bSortKeyDataSetAccessorIsAge = true;
		SortKeyDataSetAccessor.Init(CompiledData, NormalizedAgeBinding.GetDataSetBindableVariable().GetName());
	}

	NormalizedAgeAccessor.Init(CompiledData, NormalizedAgeBinding.GetDataSetBindableVariable().GetName());

	PositionDataSetAccessor.Init(CompiledData, PositionBinding.GetDataSetBindableVariable().GetName());
	SizeDataSetAccessor.Init(CompiledData, RibbonWidthBinding.GetDataSetBindableVariable().GetName());
	TwistDataSetAccessor.Init(CompiledData, RibbonTwistBinding.GetDataSetBindableVariable().GetName());
	FacingDataSetAccessor.Init(CompiledData, RibbonFacingBinding.GetDataSetBindableVariable().GetName());

	MaterialParam0DataSetAccessor.Init(CompiledData, DynamicMaterialBinding.GetDataSetBindableVariable().GetName());
	MaterialParam1DataSetAccessor.Init(CompiledData, DynamicMaterial1Binding.GetDataSetBindableVariable().GetName());
	MaterialParam2DataSetAccessor.Init(CompiledData, DynamicMaterial2Binding.GetDataSetBindableVariable().GetName());
	MaterialParam3DataSetAccessor.Init(CompiledData, DynamicMaterial3Binding.GetDataSetBindableVariable().GetName());

	FNiagaraDataSetAccessor<float> RibbonUVDistanceAccessor;
	RibbonUVDistanceAccessor.Init(CompiledData, RibbonUVDistance.GetDataSetBindableVariable().GetName());
	DistanceFromStartIsBound = RibbonUVDistanceAccessor.IsValid();

	FNiagaraDataSetAccessor<float> U0OverrideDataSetAccessor;
	U0OverrideDataSetAccessor.Init(CompiledData, U0OverrideBinding.GetDataSetBindableVariable().GetName());
	U0OverrideIsBound = U0OverrideDataSetAccessor.IsValid();
	FNiagaraDataSetAccessor<float> U1OverrideDataSetAccessor;
	U1OverrideDataSetAccessor.Init(CompiledData, U1OverrideBinding.GetDataSetBindableVariable().GetName());
	U1OverrideIsBound = U1OverrideDataSetAccessor.IsValid();

	if (RibbonIdBinding.GetDataSetBindableVariable().GetType() == FNiagaraTypeDefinition::GetIDDef())
	{
		RibbonFullIDDataSetAccessor.Init(CompiledData, RibbonIdBinding.GetDataSetBindableVariable().GetName());
	}
	else
	{
		RibbonIdDataSetAccessor.Init(CompiledData, RibbonIdBinding.GetDataSetBindableVariable().GetName());
	}

	RibbonLinkOrderDataSetAccessor.Init(CompiledData, RibbonLinkOrderBinding.GetDataSetBindableVariable().GetName());

	const bool bShouldDoFacing = FacingMode == ENiagaraRibbonFacingMode::Custom || FacingMode == ENiagaraRibbonFacingMode::CustomSideVector;

	// Initialize layout
	RendererLayout.Initialize(ENiagaraRibbonVFLayout::Num);
	RendererLayout.SetVariableFromBinding(CompiledData, PositionBinding, ENiagaraRibbonVFLayout::Position);
	RendererLayout.SetVariableFromBinding(CompiledData, VelocityBinding, ENiagaraRibbonVFLayout::Velocity);
	RendererLayout.SetVariableFromBinding(CompiledData, ColorBinding, ENiagaraRibbonVFLayout::Color);
	RendererLayout.SetVariableFromBinding(CompiledData, RibbonWidthBinding, ENiagaraRibbonVFLayout::Width);
	RendererLayout.SetVariableFromBinding(CompiledData, RibbonTwistBinding, ENiagaraRibbonVFLayout::Twist);
	if (bShouldDoFacing)
	{
		RendererLayout.SetVariableFromBinding(CompiledData, RibbonFacingBinding, ENiagaraRibbonVFLayout::Facing);
	}
	RendererLayout.SetVariableFromBinding(CompiledData, NormalizedAgeBinding, ENiagaraRibbonVFLayout::NormalizedAge);
	RendererLayout.SetVariableFromBinding(CompiledData, MaterialRandomBinding, ENiagaraRibbonVFLayout::MaterialRandom);
	RendererLayout.SetVariableFromBinding(CompiledData, RibbonUVDistance, ENiagaraRibbonVFLayout::DistanceFromStart);
	RendererLayout.SetVariableFromBinding(CompiledData, U0OverrideBinding, ENiagaraRibbonVFLayout::U0Override);
	RendererLayout.SetVariableFromBinding(CompiledData, V0RangeOverrideBinding, ENiagaraRibbonVFLayout::V0RangeOverride);
	RendererLayout.SetVariableFromBinding(CompiledData, U1OverrideBinding, ENiagaraRibbonVFLayout::U1Override);
	RendererLayout.SetVariableFromBinding(CompiledData, V1RangeOverrideBinding, ENiagaraRibbonVFLayout::V1RangeOverride);
	MaterialParamValidMask  = RendererLayout.SetVariableFromBinding(CompiledData, DynamicMaterialBinding, ENiagaraRibbonVFLayout::MaterialParam0) ? 1 : 0;
	MaterialParamValidMask |= RendererLayout.SetVariableFromBinding(CompiledData, DynamicMaterial1Binding, ENiagaraRibbonVFLayout::MaterialParam1) ? 2 : 0;
	MaterialParamValidMask |= RendererLayout.SetVariableFromBinding(CompiledData, DynamicMaterial2Binding, ENiagaraRibbonVFLayout::MaterialParam2) ? 4 : 0;
	MaterialParamValidMask |= RendererLayout.SetVariableFromBinding(CompiledData, DynamicMaterial3Binding, ENiagaraRibbonVFLayout::MaterialParam3) ? 8 : 0;

	if (NeedsPreciseMotionVectors())
	{
		RendererLayout.SetVariableFromBinding(CompiledData, PrevPositionBinding, ENiagaraRibbonVFLayout::PrevPosition);
		RendererLayout.SetVariableFromBinding(CompiledData, PrevRibbonWidthBinding, ENiagaraRibbonVFLayout::PrevRibbonWidth);
		RendererLayout.SetVariableFromBinding(CompiledData, PrevRibbonFacingBinding, ENiagaraRibbonVFLayout::PrevRibbonFacing);
		RendererLayout.SetVariableFromBinding(CompiledData, PrevRibbonTwistBinding, ENiagaraRibbonVFLayout::PrevRibbonTwist);
	}

	RendererLayout.SetVariableFromBinding(CompiledData, RibbonLinkOrderBinding, ENiagaraRibbonVFLayout::LinkOrder);
	
	RendererLayout.Finalize();
}


#if WITH_EDITORONLY_DATA

bool UNiagaraRibbonRendererProperties::IsSupportedVariableForBinding(const FNiagaraVariableBase& InSourceForBinding, const FName& InTargetBindingName) const
{
	if (InSourceForBinding.IsInNameSpace(FNiagaraConstants::ParticleAttributeNamespaceString) ||
		InSourceForBinding.IsInNameSpace(FNiagaraConstants::UserNamespaceString) ||
		InSourceForBinding.IsInNameSpace(FNiagaraConstants::SystemNamespaceString) ||
		InSourceForBinding.IsInNameSpace(FNiagaraConstants::EmitterNamespaceString))
	{
		return true;
	}
	return false;
}

void UNiagaraRibbonRendererProperties::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UNiagaraRibbonRendererProperties, TessellationAngle))
	{
		if (TessellationAngle > 0.f && TessellationAngle < 1.f)
		{
			TessellationAngle = 1.f;
		}
	}
}

const TArray<FNiagaraVariable>& UNiagaraRibbonRendererProperties::GetOptionalAttributes()
{
	static TArray<FNiagaraVariable> Attrs;

	if (Attrs.Num() == 0)
	{
		Attrs.Add(SYS_PARAM_PARTICLES_POSITION);
		Attrs.Add(SYS_PARAM_PARTICLES_NORMALIZED_AGE);
		Attrs.Add(SYS_PARAM_PARTICLES_COLOR);
		Attrs.Add(SYS_PARAM_PARTICLES_RIBBONID);
		Attrs.Add(SYS_PARAM_PARTICLES_RIBBONTWIST);
		Attrs.Add(SYS_PARAM_PARTICLES_RIBBONWIDTH);
		Attrs.Add(SYS_PARAM_PARTICLES_RIBBONFACING);
		Attrs.Add(SYS_PARAM_PARTICLES_RIBBONLINKORDER);
		Attrs.Add(RIBBONUVDISTANCE);
		Attrs.Add(SYS_PARAM_PARTICLES_RIBBONU0OVERRIDE);
		Attrs.Add(SYS_PARAM_PARTICLES_RIBBONV0RANGEOVERRIDE);
		Attrs.Add(SYS_PARAM_PARTICLES_RIBBONU1OVERRIDE);
		Attrs.Add(SYS_PARAM_PARTICLES_RIBBONV1RANGEOVERRIDE);
	}

	return Attrs;
}

void UNiagaraRibbonRendererProperties::GetAdditionalVariables(TArray<FNiagaraVariableBase>& OutArray) const
{
	if (NeedsPreciseMotionVectors())
	{
		OutArray.Append({
				PrevPositionBinding.GetParamMapBindableVariable(),
				PrevRibbonWidthBinding.GetParamMapBindableVariable(),
				PrevRibbonFacingBinding.GetParamMapBindableVariable(),
				PrevRibbonTwistBinding.GetParamMapBindableVariable()
		});
	}
}

FNiagaraVariable UNiagaraRibbonRendererProperties::GetBoundAttribute(const FNiagaraVariableAttributeBinding* Binding) const
{
	if (!NeedsPreciseMotionVectors())
	{
		if ((Binding == &PrevPositionBinding)
			|| (Binding == &PrevRibbonWidthBinding)
			|| (Binding == &PrevRibbonFacingBinding)
			|| (Binding == &PrevRibbonTwistBinding))
		{
			return FNiagaraVariable();
		}
	}

	return Super::GetBoundAttribute(Binding);
}

void UNiagaraRibbonRendererProperties::GetRendererWidgets(const FNiagaraEmitterInstance* InEmitter, TArray<TSharedPtr<SWidget>>& OutWidgets, TSharedPtr<FAssetThumbnailPool> InThumbnailPool) const
{
	TSharedRef<SWidget> ThumbnailWidget = SNullWidget::NullWidget;
	int32 ThumbnailSize = 32;
	TArray<UMaterialInterface*> Materials;
	GetUsedMaterials(InEmitter, Materials);
	for (UMaterialInterface* PreviewedMaterial : Materials)
	{
		TSharedPtr<FAssetThumbnail> AssetThumbnail = MakeShareable(new FAssetThumbnail(PreviewedMaterial, ThumbnailSize, ThumbnailSize, InThumbnailPool));
		if (AssetThumbnail)
		{
			ThumbnailWidget = AssetThumbnail->MakeThumbnailWidget();
		}
		OutWidgets.Add(ThumbnailWidget);
	}

	if (Materials.Num() == 0)
	{
		TSharedRef<SWidget> SpriteWidget = SNew(SImage)
			.Image(FSlateIconFinder::FindIconBrushForClass(GetClass()));
		OutWidgets.Add(SpriteWidget);
	}
}

void UNiagaraRibbonRendererProperties::GetRendererTooltipWidgets(const FNiagaraEmitterInstance* InEmitter, TArray<TSharedPtr<SWidget>>& OutWidgets, TSharedPtr<FAssetThumbnailPool> InThumbnailPool) const
{
	TArray<UMaterialInterface*> Materials;
	GetUsedMaterials(InEmitter, Materials);
	if (Materials.Num() > 0)
	{
		GetRendererWidgets(InEmitter, OutWidgets, InThumbnailPool);
	}
	else
	{
		TSharedRef<SWidget> RibbonTooltip = SNew(STextBlock)
			.Text(LOCTEXT("RibbonRendererNoMat", "Ribbon Renderer (No Material Set)"));
		OutWidgets.Add(RibbonTooltip);
	}
}


void UNiagaraRibbonRendererProperties::GetRendererFeedback(const FVersionedNiagaraEmitter& InEmitter, TArray<FNiagaraRendererFeedback>& OutErrors, TArray<FNiagaraRendererFeedback>& OutWarnings, TArray<FNiagaraRendererFeedback>& OutInfo) const
{
	Super::GetRendererFeedback(InEmitter, OutErrors, OutWarnings, OutInfo);

	const FVersionedNiagaraEmitterData* EmitterData = InEmitter.GetEmitterData();


	// If we're in a gpu sim, then uv mode uniform by segment can cause some visual oddity due to non-existent
	// culling of near particles like the cpu initialization pipeline runs
	if (EmitterData && EmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim)
	{
		const auto CheckUVSettingsForChannel = [&](const FNiagaraRibbonUVSettings& UVSettings, int32 Index)
		{
			if (UVSettings.DistributionMode == ENiagaraRibbonUVDistributionMode::ScaledUniformly)
			{
				const FText ErrorDescription = FText::Format(LOCTEXT("NiagaraRibbonRendererUVBySegmentGPUDesc", "The specified UV Distribution for Channel {0} on GPU may result in different visual look than a CPU sim due to increased particle density in GPU sim."), FText::AsNumber(Index));
				const FText ErrorSummary = FText::Format(LOCTEXT("NiagaraRibbonRendererUVBySegmentGPUSummary", "The specified UV Settings on Channel {0} on GPU may result in undesirable look."), FText::AsNumber(Index));
				OutWarnings.Add(FNiagaraRendererFeedback(ErrorDescription, ErrorSummary, FText(), FNiagaraRendererFeedbackFix(), true));				
			}
		};

		CheckUVSettingsForChannel(UV0Settings, 0);
		CheckUVSettingsForChannel(UV1Settings, 1);
	}


	// If we're in multiplane shape, and multiplane count is even while we're in camera facing mode then one
	// slice out of the set will be invisible because the camera will be coplanar to it
	if (FacingMode == ENiagaraRibbonFacingMode::Screen && Shape == ENiagaraRibbonShapeMode::MultiPlane && MultiPlaneCount % 2 == 0)
	{
		const FText ErrorDescription = LOCTEXT("NiagaraRibbonRendererMultiPlaneInvisibleFaceDesc", "The specified MultiPlaneCount (Even Count) with ScreenFacing will result in a hidden face due to the camera being coplanar to one face.");
		const FText ErrorSummary = LOCTEXT("NiagaraRibbonRendererMultiPlaneInvisibleFaceSummary", "The specified MultiPlaneCount+ScreenFacing will result in a hidden face.");
		const FText ErrorFix = LOCTEXT("NiagaraRibbonRendererMultiPlaneInvisibleFaceFix", "Fix by decreasing MultiPlane count by 1.");
		const FNiagaraRendererFeedbackFix MultiPlaneFix = FNiagaraRendererFeedbackFix::CreateLambda([this]() { const_cast<UNiagaraRibbonRendererProperties*>(this)->MultiPlaneCount = FMath::Clamp(this->MultiPlaneCount - 1, 1, 16); });
		OutWarnings.Add(FNiagaraRendererFeedback(ErrorDescription, ErrorSummary, ErrorFix, MultiPlaneFix, true));	
	}	


	if (MaterialParameters.HasAnyBindings())
	{
		TArray<UMaterialInterface*> Materials;
		GetUsedMaterials(nullptr, Materials);
		MaterialParameters.GetFeedback(Materials, OutWarnings);
	}
}


bool UNiagaraRibbonRendererProperties::CanEditChange(const FProperty* InProperty) const
{

	if (InProperty->HasMetaData(TEXT("Category")) && InProperty->GetMetaData(TEXT("Category")).Contains("Tessellation"))
	{
		FName PropertyName = InProperty->GetFName();
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UNiagaraRibbonRendererProperties, CurveTension))
		{
			return TessellationMode != ENiagaraRibbonTessellationMode::Disabled;
		}
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UNiagaraRibbonRendererProperties, TessellationFactor))
		{
			return TessellationMode == ENiagaraRibbonTessellationMode::Custom;
		}
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UNiagaraRibbonRendererProperties, TessellationMode))
		{
			return Super::CanEditChange(InProperty);
		}
		return TessellationMode == ENiagaraRibbonTessellationMode::Custom;
	}
	return Super::CanEditChange(InProperty);
}

void UNiagaraRibbonRendererProperties::RenameVariable(const FNiagaraVariableBase& OldVariable, const FNiagaraVariableBase& NewVariable, const FVersionedNiagaraEmitter& InEmitter)
{
	Super::RenameVariable(OldVariable, NewVariable, InEmitter);

	// Handle renaming material bindings
	for (FNiagaraMaterialAttributeBinding& Binding : MaterialParameters.AttributeBindings)
	{
		Binding.RenameVariableIfMatching(OldVariable, NewVariable, InEmitter.Emitter, GetCurrentSourceMode());
	}
}

void UNiagaraRibbonRendererProperties::RemoveVariable(const FNiagaraVariableBase& OldVariable, const FVersionedNiagaraEmitter& InEmitter)
{
	Super::RemoveVariable(OldVariable, InEmitter);

	// Handle resetting material bindings to defaults
	for (FNiagaraMaterialAttributeBinding& Binding : MaterialParameters.AttributeBindings)
	{
		if (Binding.Matches(OldVariable, InEmitter.Emitter, GetCurrentSourceMode()))
		{
			Binding.NiagaraVariable = FNiagaraVariable();
			Binding.CacheValues(InEmitter.Emitter);
		}
	}
}

#endif // WITH_EDITORONLY_DATA
#undef LOCTEXT_NAMESPACE

