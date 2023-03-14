// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraRenderer.h"
#include "Internationalization/Internationalization.h"
#include "NiagaraConstants.h"
#include "NiagaraRendererSprites.h"
#include "NiagaraBoundsCalculatorHelper.h"
#include "NiagaraCustomVersion.h"
#include "NiagaraEmitterInstance.h"
#include "Modules/ModuleManager.h"
#include "NiagaraEmitter.h"
#include "NiagaraScriptSourceBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraSpriteRendererProperties)

#if WITH_EDITOR
#include "DerivedDataCacheInterface.h"
#include "Widgets/Images/SImage.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/SWidget.h"
#include "AssetThumbnail.h"
#include "Widgets/Text/STextBlock.h"
#endif

#define LOCTEXT_NAMESPACE "UNiagaraSpriteRendererProperties"

TArray<TWeakObjectPtr<UNiagaraSpriteRendererProperties>> UNiagaraSpriteRendererProperties::SpriteRendererPropertiesToDeferredInit;

#if ENABLE_COOK_STATS
class NiagaraCutoutCookStats
{
public:
	static FCookStats::FDDCResourceUsageStats UsageStats;
	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats;
};

FCookStats::FDDCResourceUsageStats NiagaraCutoutCookStats::UsageStats;
FCookStatsManager::FAutoRegisterCallback NiagaraCutoutCookStats::RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
{
	UsageStats.LogStats(AddStat, TEXT("NiagaraCutout.Usage"), TEXT(""));
});
#endif // ENABLE_COOK_STATS

UNiagaraSpriteRendererProperties::UNiagaraSpriteRendererProperties()
	: Material(nullptr)
	, SourceMode(ENiagaraRendererSourceDataMode::Particles)
	, MaterialUserParamBinding(FNiagaraTypeDefinition(UMaterialInterface::StaticClass()))
	, Alignment(ENiagaraSpriteAlignment::Unaligned)
	, FacingMode(ENiagaraSpriteFacingMode::FaceCamera)
	, PivotInUVSpace(0.5f, 0.5f)
	, SortMode(ENiagaraSortMode::None)
	, SubImageSize(1.0f, 1.0f)
	, bSubImageBlend(false)
	, bRemoveHMDRollInVR(false)
	, bSortOnlyWhenTranslucent(true)
	, MinFacingCameraBlendDistance(0.0f)
	, MaxFacingCameraBlendDistance(0.0f)
#if WITH_EDITORONLY_DATA
	, BoundingMode(BVC_EightVertices)
	, AlphaThreshold(0.1f)
#endif // WITH_EDITORONLY_DATA
{
	AttributeBindings.Reserve(27);

	// NOTE: These bindings' indices have to align to their counterpart in ENiagaraSpriteVFLayout
	AttributeBindings.Add(&PositionBinding);
	AttributeBindings.Add(&ColorBinding);
	AttributeBindings.Add(&VelocityBinding);
	AttributeBindings.Add(&SpriteRotationBinding);
	AttributeBindings.Add(&SpriteSizeBinding);
	AttributeBindings.Add(&SpriteFacingBinding);
	AttributeBindings.Add(&SpriteAlignmentBinding);
	AttributeBindings.Add(&SubImageIndexBinding);
	AttributeBindings.Add(&DynamicMaterialBinding);
	AttributeBindings.Add(&DynamicMaterial1Binding);
	AttributeBindings.Add(&DynamicMaterial2Binding);
	AttributeBindings.Add(&DynamicMaterial3Binding);
	AttributeBindings.Add(&CameraOffsetBinding);
	AttributeBindings.Add(&UVScaleBinding);
	AttributeBindings.Add(&PivotOffsetBinding);
	AttributeBindings.Add(&MaterialRandomBinding);
	AttributeBindings.Add(&CustomSortingBinding);
	AttributeBindings.Add(&NormalizedAgeBinding);

	// These bindings are only actually used with accurate motion vectors (indices still need to align)
	AttributeBindings.Add(&PrevPositionBinding);
	AttributeBindings.Add(&PrevVelocityBinding);
	AttributeBindings.Add(&PrevSpriteRotationBinding);
	AttributeBindings.Add(&PrevSpriteSizeBinding);
	AttributeBindings.Add(&PrevSpriteFacingBinding);
	AttributeBindings.Add(&PrevSpriteAlignmentBinding);
	AttributeBindings.Add(&PrevCameraOffsetBinding);
	AttributeBindings.Add(&PrevPivotOffsetBinding);

	// The remaining bindings are not associated with attributes in the VF layout
	AttributeBindings.Add(&RendererVisibilityTagBinding);
}

FNiagaraRenderer* UNiagaraSpriteRendererProperties::CreateEmitterRenderer(ERHIFeatureLevel::Type FeatureLevel, const FNiagaraEmitterInstance* Emitter, const FNiagaraSystemInstanceController& InController)
{
	FNiagaraRenderer* NewRenderer = new FNiagaraRendererSprites(FeatureLevel, this, Emitter);
	NewRenderer->Initialize(this, Emitter, InController);
	return NewRenderer;
}

FNiagaraBoundsCalculator* UNiagaraSpriteRendererProperties::CreateBoundsCalculator()
{
	if (GetCurrentSourceMode() == ENiagaraRendererSourceDataMode::Emitter)
	{
		return nullptr;
	}

	return new FNiagaraBoundsCalculatorHelper<true, false, false>();
}

void UNiagaraSpriteRendererProperties::GetUsedMaterials(const FNiagaraEmitterInstance* InEmitter, TArray<UMaterialInterface*>& OutMaterials) const
{
	UMaterialInterface* MaterialInterface = nullptr;
	if (InEmitter != nullptr)
	{
		MaterialInterface = Cast<UMaterialInterface>(InEmitter->FindBinding(MaterialUserParamBinding.Parameter));
	}

	OutMaterials.Add(MaterialInterface ? MaterialInterface : ToRawPtr(Material));
}

const FVertexFactoryType* UNiagaraSpriteRendererProperties::GetVertexFactoryType() const
{
	return &FNiagaraSpriteVertexFactory::StaticType;
}

void UNiagaraSpriteRendererProperties::PostLoad()
{
	Super::PostLoad();

	if ( Material )
	{
		Material->ConditionalPostLoad();
	}

#if WITH_EDITORONLY_DATA
	if (MaterialUserParamBinding.Parameter.GetType().GetClass() != UMaterialInterface::StaticClass())
	{
		FNiagaraTypeDefinition MaterialDef(UMaterialInterface::StaticClass());
		MaterialUserParamBinding.Parameter.SetType(MaterialDef);
	}

	if (!FPlatformProperties::RequiresCookedData())
	{
		if (CutoutTexture)
		{	// Here we don't call UpdateCutoutTexture() to avoid issue with the material postload.
			CutoutTexture->ConditionalPostLoad();
		}
		CacheDerivedData();
	}
	ChangeToPositionBinding(PositionBinding);
	ChangeToPositionBinding(PrevPositionBinding);
	PostLoadBindings(SourceMode);

	// Fix up these bindings from their loaded source bindings
	SetPreviousBindings(FVersionedNiagaraEmitter(), SourceMode);

	if (MaterialParameterBindings_DEPRECATED.Num() > 0)
	{
		MaterialParameters.AttributeBindings = MaterialParameterBindings_DEPRECATED;
		MaterialParameterBindings_DEPRECATED.Empty();
	}
#endif
}

void UNiagaraSpriteRendererProperties::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		// We can end up hitting PostInitProperties before the Niagara Module has initialized bindings this needs, mark this object for deferred init and early out.
		if (FModuleManager::Get().IsModuleLoaded("Niagara") == false)
		{
			SpriteRendererPropertiesToDeferredInit.Add(this);
			return;
		}
		InitBindings();
	}
}

void UNiagaraSpriteRendererProperties::Serialize(FStructuredArchive::FRecord Record)
{
	FArchive& Ar = Record.GetUnderlyingArchive();
	Ar.UsingCustomVersion(FNiagaraCustomVersion::GUID);
	const int32 NiagaraVersion = Ar.CustomVer(FNiagaraCustomVersion::GUID);

	if (Ar.IsLoading() && (NiagaraVersion < FNiagaraCustomVersion::DisableSortingByDefault))
	{
		SortMode = ENiagaraSortMode::ViewDistance;
	}

	Super::Serialize(Record);

	bool bIsCookedForEditor = false;
#if WITH_EDITORONLY_DATA
	bIsCookedForEditor = ((Ar.GetPortFlags() & PPF_Duplicate) == 0) && GetPackage()->HasAnyPackageFlags(PKG_Cooked);
#endif // WITH_EDITORONLY_DATA

	FArchive& UnderlyingArchive = Record.GetUnderlyingArchive();
	if (UnderlyingArchive.IsCooking() || (FPlatformProperties::RequiresCookedData() && UnderlyingArchive.IsLoading()) || bIsCookedForEditor)
	{
		DerivedData.Serialize(Record.EnterField(TEXT("DerivedData")));
	}
}

/** The bindings depend on variables that are created during the NiagaraModule startup. However, the CDO's are build prior to this being initialized, so we defer setting these values until later.*/
void UNiagaraSpriteRendererProperties::InitCDOPropertiesAfterModuleStartup()
{
	UNiagaraSpriteRendererProperties* CDO = CastChecked<UNiagaraSpriteRendererProperties>(UNiagaraSpriteRendererProperties::StaticClass()->GetDefaultObject());
	CDO->InitBindings();

	for (TWeakObjectPtr<UNiagaraSpriteRendererProperties>& WeakSpriteRendererProperties : SpriteRendererPropertiesToDeferredInit)
	{
		if (WeakSpriteRendererProperties.Get())
		{
			WeakSpriteRendererProperties->InitBindings();
		}
	}
}

void UNiagaraSpriteRendererProperties::InitBindings()
{
	if (PositionBinding.GetParamMapBindableVariable().GetName() == NAME_None)
	{
		PositionBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_POSITION);
		ColorBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_COLOR);
		VelocityBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_VELOCITY);
		SpriteRotationBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_SPRITE_ROTATION);
		SpriteSizeBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_SPRITE_SIZE);
		SpriteFacingBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_SPRITE_FACING);
		SpriteAlignmentBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_SPRITE_ALIGNMENT);
		SubImageIndexBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_SUB_IMAGE_INDEX);
		DynamicMaterialBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM);
		DynamicMaterial1Binding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_1);
		DynamicMaterial2Binding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_2);
		DynamicMaterial3Binding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_3);
		CameraOffsetBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_CAMERA_OFFSET);
		UVScaleBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_UV_SCALE);
		PivotOffsetBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_PIVOT_OFFSET);
		MaterialRandomBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_MATERIAL_RANDOM);
		NormalizedAgeBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_NORMALIZED_AGE);
		RendererVisibilityTagBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_VISIBILITY_TAG);

		//Default custom sorting to age
		CustomSortingBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_NORMALIZED_AGE);
	}

	SetPreviousBindings(FVersionedNiagaraEmitter(), SourceMode);
}

void UNiagaraSpriteRendererProperties::SetPreviousBindings(const FVersionedNiagaraEmitter& SrcEmitter, ENiagaraRendererSourceDataMode InSourceMode)
{
	PrevPositionBinding.SetAsPreviousValue(PositionBinding, SrcEmitter, InSourceMode);
	PrevVelocityBinding.SetAsPreviousValue(VelocityBinding, SrcEmitter, InSourceMode);
	PrevSpriteRotationBinding.SetAsPreviousValue(SpriteRotationBinding, SrcEmitter, InSourceMode);
	PrevSpriteSizeBinding.SetAsPreviousValue(SpriteSizeBinding, SrcEmitter, InSourceMode);
	PrevSpriteFacingBinding.SetAsPreviousValue(SpriteFacingBinding, SrcEmitter, InSourceMode);
	PrevSpriteAlignmentBinding.SetAsPreviousValue(SpriteAlignmentBinding, SrcEmitter, InSourceMode);
	PrevCameraOffsetBinding.SetAsPreviousValue(CameraOffsetBinding, SrcEmitter, InSourceMode);
	PrevPivotOffsetBinding.SetAsPreviousValue(PivotOffsetBinding, SrcEmitter, InSourceMode);
}

void UNiagaraSpriteRendererProperties::CacheFromCompiledData(const FNiagaraDataSetCompiledData* CompiledData)
{
	UpdateSourceModeDerivates(SourceMode);

	const int32 NumLayoutVars = NeedsPreciseMotionVectors() ? ENiagaraSpriteVFLayout::Num_Max : ENiagaraSpriteVFLayout::Num_Default;
	RendererLayoutWithCustomSort.Initialize(NumLayoutVars);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, PositionBinding, ENiagaraSpriteVFLayout::Position);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, VelocityBinding, ENiagaraSpriteVFLayout::Velocity);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, ColorBinding, ENiagaraSpriteVFLayout::Color);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, SpriteRotationBinding, ENiagaraSpriteVFLayout::Rotation);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, SpriteSizeBinding, ENiagaraSpriteVFLayout::Size);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, SpriteFacingBinding, ENiagaraSpriteVFLayout::Facing);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, SpriteAlignmentBinding, ENiagaraSpriteVFLayout::Alignment);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, SubImageIndexBinding, ENiagaraSpriteVFLayout::SubImage);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, CameraOffsetBinding, ENiagaraSpriteVFLayout::CameraOffset);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, UVScaleBinding, ENiagaraSpriteVFLayout::UVScale);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, PivotOffsetBinding, ENiagaraSpriteVFLayout::PivotOffset);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, NormalizedAgeBinding, ENiagaraSpriteVFLayout::NormalizedAge);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, MaterialRandomBinding, ENiagaraSpriteVFLayout::MaterialRandom);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, CustomSortingBinding, ENiagaraSpriteVFLayout::CustomSorting);
	MaterialParamValidMask  = RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, DynamicMaterialBinding, ENiagaraSpriteVFLayout::MaterialParam0) ? 0x1 : 0;
	MaterialParamValidMask |= RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, DynamicMaterial1Binding, ENiagaraSpriteVFLayout::MaterialParam1) ? 0x2 : 0;
	MaterialParamValidMask |= RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, DynamicMaterial2Binding, ENiagaraSpriteVFLayout::MaterialParam2) ? 0x4 : 0;
	MaterialParamValidMask |= RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, DynamicMaterial3Binding, ENiagaraSpriteVFLayout::MaterialParam3) ? 0x8 : 0;
	if (NeedsPreciseMotionVectors())
	{
		RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, PrevPositionBinding, ENiagaraSpriteVFLayout::PrevPosition);
		RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, PrevVelocityBinding, ENiagaraSpriteVFLayout::PrevVelocity);
		RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, PrevSpriteRotationBinding, ENiagaraSpriteVFLayout::PrevRotation);
		RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, PrevSpriteSizeBinding, ENiagaraSpriteVFLayout::PrevSize);
		RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, PrevSpriteFacingBinding, ENiagaraSpriteVFLayout::PrevFacing);
		RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, PrevSpriteAlignmentBinding, ENiagaraSpriteVFLayout::PrevAlignment);
		RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, PrevCameraOffsetBinding, ENiagaraSpriteVFLayout::PrevCameraOffset);
		RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, PrevPivotOffsetBinding, ENiagaraSpriteVFLayout::PrevPivotOffset);
	}
	RendererLayoutWithCustomSort.Finalize();

	RendererLayoutWithoutCustomSort.Initialize(NumLayoutVars);
	RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, PositionBinding, ENiagaraSpriteVFLayout::Position);
	RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, VelocityBinding, ENiagaraSpriteVFLayout::Velocity);
	RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, ColorBinding, ENiagaraSpriteVFLayout::Color);
	RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, SpriteRotationBinding, ENiagaraSpriteVFLayout::Rotation);
	RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, SpriteSizeBinding, ENiagaraSpriteVFLayout::Size);
	RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, SpriteFacingBinding, ENiagaraSpriteVFLayout::Facing);
	RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, SpriteAlignmentBinding, ENiagaraSpriteVFLayout::Alignment);
	RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, SubImageIndexBinding, ENiagaraSpriteVFLayout::SubImage);
	RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, CameraOffsetBinding, ENiagaraSpriteVFLayout::CameraOffset);
	RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, UVScaleBinding, ENiagaraSpriteVFLayout::UVScale);
	RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, PivotOffsetBinding, ENiagaraSpriteVFLayout::PivotOffset);
	RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, NormalizedAgeBinding, ENiagaraSpriteVFLayout::NormalizedAge);
	RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, MaterialRandomBinding, ENiagaraSpriteVFLayout::MaterialRandom);
	MaterialParamValidMask = RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, DynamicMaterialBinding, ENiagaraSpriteVFLayout::MaterialParam0) ? 0x1 : 0;
	MaterialParamValidMask |= RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, DynamicMaterial1Binding, ENiagaraSpriteVFLayout::MaterialParam1) ? 0x2 : 0;
	MaterialParamValidMask |= RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, DynamicMaterial2Binding, ENiagaraSpriteVFLayout::MaterialParam2) ? 0x4 : 0;
	MaterialParamValidMask |= RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, DynamicMaterial3Binding, ENiagaraSpriteVFLayout::MaterialParam3) ? 0x8 : 0;
	if (NeedsPreciseMotionVectors())
	{
		RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, PrevPositionBinding, ENiagaraSpriteVFLayout::PrevPosition);
		RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, PrevVelocityBinding, ENiagaraSpriteVFLayout::PrevVelocity);
		RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, PrevSpriteRotationBinding, ENiagaraSpriteVFLayout::PrevRotation);
		RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, PrevSpriteSizeBinding, ENiagaraSpriteVFLayout::PrevSize);
		RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, PrevSpriteFacingBinding, ENiagaraSpriteVFLayout::PrevFacing);
		RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, PrevSpriteAlignmentBinding, ENiagaraSpriteVFLayout::PrevAlignment);
		RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, PrevCameraOffsetBinding, ENiagaraSpriteVFLayout::PrevCameraOffset);
		RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, PrevPivotOffsetBinding, ENiagaraSpriteVFLayout::PrevPivotOffset);
	}
	RendererLayoutWithoutCustomSort.Finalize();

}

#if WITH_EDITORONLY_DATA
TArray<FNiagaraVariable> UNiagaraSpriteRendererProperties::GetBoundAttributes() const
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

bool UNiagaraSpriteRendererProperties::PopulateRequiredBindings(FNiagaraParameterStore& InParameterStore)
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

void UNiagaraSpriteRendererProperties::UpdateSourceModeDerivates(ENiagaraRendererSourceDataMode InSourceMode, bool bFromPropertyEdit)
{
	Super::UpdateSourceModeDerivates(InSourceMode, bFromPropertyEdit);
	
	UNiagaraEmitter* SrcEmitter = GetTypedOuter<UNiagaraEmitter>();
	if (SrcEmitter)
	{
		for (FNiagaraMaterialAttributeBinding& MaterialParamBinding : MaterialParameters.AttributeBindings)
		{
			MaterialParamBinding.CacheValues(SrcEmitter);
		}

		SetPreviousBindings(FVersionedNiagaraEmitter(), InSourceMode);
	}
}

#if WITH_EDITORONLY_DATA

bool UNiagaraSpriteRendererProperties::IsSupportedVariableForBinding(const FNiagaraVariableBase& InSourceForBinding, const FName& InTargetBindingName) const
{
	if ((SourceMode == ENiagaraRendererSourceDataMode::Particles && InSourceForBinding.IsInNameSpace(FNiagaraConstants::ParticleAttributeNamespaceString)) ||
		InSourceForBinding.IsInNameSpace(FNiagaraConstants::UserNamespaceString) ||
		InSourceForBinding.IsInNameSpace(FNiagaraConstants::SystemNamespaceString) ||
		InSourceForBinding.IsInNameSpace(FNiagaraConstants::EmitterNamespaceString))
	{
		return true;
	}
	return false;
}


//#if WITH_EDITOR
//void UNiagaraSpriteRendererProperties::PostEditUndo()
//{
//	Super::PostEditUndo();
//	UpdateSourceModeDerivates(SourceMode);
//}
//
//void UNiagaraSpriteRendererProperties::PostEditUndo(TSharedPtr<ITransactionObjectAnnotation> TransactionAnnotation)
//{
//	Super::PostEditUndo(TransactionAnnotation);
//	UpdateSourceModeDerivates(SourceMode);
//}
//#endif

void UNiagaraSpriteRendererProperties::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	SubImageSize.X = FMath::Max<float>(SubImageSize.X, 1.f);
	SubImageSize.Y = FMath::Max<float>(SubImageSize.Y, 1.f);

	// DerivedData.BoundingGeometry in case we cleared the CutoutTexture
	if (bUseMaterialCutoutTexture || CutoutTexture || DerivedData.BoundingGeometry.Num())
	{
		const bool bUpdateCutoutDDC =
			PropertyChangedEvent.GetPropertyName() == TEXT("bUseMaterialCutoutTexture") ||
			PropertyChangedEvent.GetPropertyName() == TEXT("CutoutTexture") ||
			PropertyChangedEvent.GetPropertyName() == TEXT("BoundingMode") ||
			PropertyChangedEvent.GetPropertyName() == TEXT("OpacitySourceMode") ||
			PropertyChangedEvent.GetPropertyName() == TEXT("AlphaThreshold") ||
			(bUseMaterialCutoutTexture && PropertyChangedEvent.GetPropertyName() == TEXT("Material"));

		if (bUpdateCutoutDDC)
		{
			UpdateCutoutTexture();
			CacheDerivedData();
		}
	}

	// If changing the source mode, we may need to update many of our values.
	if (PropertyChangedEvent.GetPropertyName() == TEXT("SourceMode"))
	{
		UpdateSourceModeDerivates(SourceMode, true);
	}
	else if (FStructProperty* StructProp = CastField<FStructProperty>(PropertyChangedEvent.Property))
	{
		if (StructProp->Struct == FNiagaraVariableAttributeBinding::StaticStruct())
		{
			UpdateSourceModeDerivates(SourceMode, true);
		}
	}
	else if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(PropertyChangedEvent.Property))
	{
		if (ArrayProp->Inner)
		{
			FStructProperty* ChildStructProp = CastField<FStructProperty>(ArrayProp->Inner);
			if (ChildStructProp->Struct == FNiagaraMaterialAttributeBinding::StaticStruct())
			{
				UpdateSourceModeDerivates(SourceMode, true);
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);

}


void UNiagaraSpriteRendererProperties::RenameVariable(const FNiagaraVariableBase& OldVariable, const FNiagaraVariableBase& NewVariable, const FVersionedNiagaraEmitter& InEmitter)
{
	Super::RenameVariable(OldVariable, NewVariable, InEmitter);

	// Handle renaming material bindings
	for (FNiagaraMaterialAttributeBinding& Binding : MaterialParameters.AttributeBindings)
	{
		Binding.RenameVariableIfMatching(OldVariable, NewVariable, InEmitter.Emitter, GetCurrentSourceMode());
	}
}

void UNiagaraSpriteRendererProperties::RemoveVariable(const FNiagaraVariableBase& OldVariable, const FVersionedNiagaraEmitter& InEmitter)
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

const TArray<FNiagaraVariable>& UNiagaraSpriteRendererProperties::GetOptionalAttributes()
{
	static TArray<FNiagaraVariable> Attrs;

	if (Attrs.Num() == 0)
	{
		Attrs.Add(SYS_PARAM_PARTICLES_POSITION);
		Attrs.Add(SYS_PARAM_PARTICLES_VELOCITY);
		Attrs.Add(SYS_PARAM_PARTICLES_COLOR);
		Attrs.Add(SYS_PARAM_PARTICLES_SPRITE_ROTATION);
		Attrs.Add(SYS_PARAM_PARTICLES_NORMALIZED_AGE);
		Attrs.Add(SYS_PARAM_PARTICLES_SPRITE_SIZE);
		Attrs.Add(SYS_PARAM_PARTICLES_SPRITE_FACING);
		Attrs.Add(SYS_PARAM_PARTICLES_SPRITE_ALIGNMENT);
		Attrs.Add(SYS_PARAM_PARTICLES_SUB_IMAGE_INDEX);
		Attrs.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM);
		Attrs.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_1);
		Attrs.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_2);
		Attrs.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_3);
		Attrs.Add(SYS_PARAM_PARTICLES_CAMERA_OFFSET);
		Attrs.Add(SYS_PARAM_PARTICLES_UV_SCALE);
		Attrs.Add(SYS_PARAM_PARTICLES_PIVOT_OFFSET);
		Attrs.Add(SYS_PARAM_PARTICLES_MATERIAL_RANDOM);
	}

	return Attrs;
}

void UNiagaraSpriteRendererProperties::GetAdditionalVariables(TArray<FNiagaraVariableBase>& OutArray) const
{
	if (NeedsPreciseMotionVectors())
	{
		OutArray.Reserve(8);
		OutArray.Add(PrevPositionBinding.GetParamMapBindableVariable());
		OutArray.Add(PrevVelocityBinding.GetParamMapBindableVariable());
		OutArray.Add(PrevSpriteRotationBinding.GetParamMapBindableVariable());
		OutArray.Add(PrevSpriteSizeBinding.GetParamMapBindableVariable());
		OutArray.Add(PrevSpriteFacingBinding.GetParamMapBindableVariable());
		OutArray.Add(PrevSpriteAlignmentBinding.GetParamMapBindableVariable());
		OutArray.Add(PrevCameraOffsetBinding.GetParamMapBindableVariable());
		OutArray.Add(PrevPivotOffsetBinding.GetParamMapBindableVariable());		
	}
}

void UNiagaraSpriteRendererProperties::GetRendererWidgets(const FNiagaraEmitterInstance* InEmitter, TArray<TSharedPtr<SWidget>>& OutWidgets, TSharedPtr<FAssetThumbnailPool> InThumbnailPool) const
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


void UNiagaraSpriteRendererProperties::GetRendererFeedback(const FVersionedNiagaraEmitter& InEmitter, TArray<FText>& OutErrors, TArray<FText>& OutWarnings, TArray<FText>& OutInfo) const
{
	Super::GetRendererFeedback(InEmitter, OutErrors, OutWarnings, OutInfo);
	if (InEmitter.GetEmitterData()->SpawnScriptProps.Script->GetVMExecutableData().IsValid())
	{
		if (bUseMaterialCutoutTexture || CutoutTexture)
		{
			if (UVScaleBinding.DoesBindingExistOnSource())
			{
				OutInfo.Add(LOCTEXT("SpriteRendererUVScaleWithCutout", "Cutouts will not be sized dynamically with UVScale variable. If scaling above 1.0, geometry may clip."));
			}
		}
	}

	if (CutoutTexture)
	{
		DerivedData.GetFeedback(CutoutTexture, (int32)SubImageSize.X, (int32)SubImageSize.Y, BoundingMode, AlphaThreshold, OpacitySourceMode, OutErrors, OutWarnings, OutInfo);
	}
}

void UNiagaraSpriteRendererProperties::GetRendererFeedback(const FVersionedNiagaraEmitter & InEmitter, TArray<FNiagaraRendererFeedback>&OutErrors, TArray<FNiagaraRendererFeedback>&OutWarnings, TArray<FNiagaraRendererFeedback>&OutInfo) const
{
	Super::GetRendererFeedback(InEmitter, OutErrors, OutWarnings, OutInfo);

	if (MaterialParameters.HasAnyBindings())
	{
		TArray<UMaterialInterface*> Materials;
		GetUsedMaterials(nullptr, Materials);
		MaterialParameters.GetFeedback(Materials, OutWarnings);
	}
}

void UNiagaraSpriteRendererProperties::GetRendererTooltipWidgets(const FNiagaraEmitterInstance* InEmitter, TArray<TSharedPtr<SWidget>>& OutWidgets, TSharedPtr<FAssetThumbnailPool> InThumbnailPool) const
{
	TArray<UMaterialInterface*> Materials;
	GetUsedMaterials(InEmitter, Materials);
	if (Materials.Num() > 0)
	{
		GetRendererWidgets(InEmitter, OutWidgets, InThumbnailPool);
	}
	else
	{
		TSharedRef<SWidget> SpriteTooltip = SNew(STextBlock)
			.Text(LOCTEXT("SpriteRendererNoMat", "Sprite Renderer (No Material Set)"));
		OutWidgets.Add(SpriteTooltip);
	}
}

void UNiagaraSpriteRendererProperties::UpdateCutoutTexture()
{
	if (bUseMaterialCutoutTexture)
	{
		CutoutTexture = nullptr;
		if (Material)
		{
			// Try to find an opacity mask texture to default to, if not try to find an opacity texture
			TArray<UTexture*> OpacityMaskTextures;
			Material->GetTexturesInPropertyChain(MP_OpacityMask, OpacityMaskTextures, nullptr, nullptr);
			if (OpacityMaskTextures.Num())
			{
				CutoutTexture = (UTexture2D*)OpacityMaskTextures[0];
			}
			else
			{
				TArray<UTexture*> OpacityTextures;
				Material->GetTexturesInPropertyChain(MP_Opacity, OpacityTextures, nullptr, nullptr);
				if (OpacityTextures.Num())
				{
					CutoutTexture = (UTexture2D*)OpacityTextures[0];
				}
			}
		}
	}
}

void UNiagaraSpriteRendererProperties::CacheDerivedData()
{
	if (CutoutTexture)
	{
		const FString KeyString = FSubUVDerivedData::GetDDCKeyString(CutoutTexture->Source.GetId(), (int32)SubImageSize.X, (int32)SubImageSize.Y, (int32)BoundingMode, AlphaThreshold, (int32)OpacitySourceMode);
		TArray<uint8> Data;

		COOK_STAT(auto Timer = NiagaraCutoutCookStats::UsageStats.TimeSyncWork());
		if (GetDerivedDataCacheRef().GetSynchronous(*KeyString, Data, GetPathName()))
		{
			COOK_STAT(Timer.AddHit(Data.Num()));
			DerivedData.BoundingGeometry.Empty(Data.Num() / sizeof(FVector2f));
			DerivedData.BoundingGeometry.AddUninitialized(Data.Num() / sizeof(FVector2f));
			FPlatformMemory::Memcpy(DerivedData.BoundingGeometry.GetData(), Data.GetData(), Data.Num() * Data.GetTypeSize());
		}
		else
		{
			DerivedData.Build(CutoutTexture, (int32)SubImageSize.X, (int32)SubImageSize.Y, BoundingMode, AlphaThreshold, OpacitySourceMode);

			Data.Empty(DerivedData.BoundingGeometry.Num() * sizeof(FVector2f));
			Data.AddUninitialized(DerivedData.BoundingGeometry.Num() * sizeof(FVector2f));
			FPlatformMemory::Memcpy(Data.GetData(), DerivedData.BoundingGeometry.GetData(), DerivedData.BoundingGeometry.Num() * DerivedData.BoundingGeometry.GetTypeSize());
			GetDerivedDataCacheRef().Put(*KeyString, Data, GetPathName());
			COOK_STAT(Timer.AddMiss(Data.Num()));
		}
	}
	else
	{
		DerivedData.BoundingGeometry.Empty();
	}
}

FNiagaraVariable UNiagaraSpriteRendererProperties::GetBoundAttribute(const FNiagaraVariableAttributeBinding* Binding) const
{
	if (!NeedsPreciseMotionVectors())
	{
		if (Binding == &PrevPositionBinding
			|| Binding == &PrevVelocityBinding
			|| Binding == &PrevSpriteRotationBinding
			|| Binding == &PrevSpriteSizeBinding
			|| Binding == &PrevSpriteFacingBinding
			|| Binding == &PrevSpriteAlignmentBinding
			|| Binding == &PrevCameraOffsetBinding
			|| Binding == &PrevPivotOffsetBinding)
		{
			return FNiagaraVariable();
		}
	}

	return Super::GetBoundAttribute(Binding);
}

#endif // WITH_EDITORONLY_DATA


int32 UNiagaraSpriteRendererProperties::GetNumCutoutVertexPerSubimage() const
{
	if (DerivedData.BoundingGeometry.Num())
	{
		const int32 NumSubImages = FMath::Max<int32>(1, (int32)SubImageSize.X * (int32)SubImageSize.Y);
		const int32 NumCutoutVertexPerSubImage = DerivedData.BoundingGeometry.Num() / NumSubImages;

		// Based on BVC_FourVertices || BVC_EightVertices
		ensure(NumCutoutVertexPerSubImage == 4 || NumCutoutVertexPerSubImage == 8);

		return NumCutoutVertexPerSubImage;
	}
	else
	{
		return 0;
	}
}

uint32 UNiagaraSpriteRendererProperties::GetNumIndicesPerInstance() const
{
	// This is a based on cutout vertices making a triangle strip.
	if (GetNumCutoutVertexPerSubimage() == 8)
	{
		return 18;
	}
	else
	{
		return 6;
	}
}

#undef LOCTEXT_NAMESPACE

