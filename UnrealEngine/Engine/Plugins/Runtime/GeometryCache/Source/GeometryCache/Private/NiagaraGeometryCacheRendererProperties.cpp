// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraGeometryCacheRendererProperties.h"
#include "NiagaraRenderer.h"
#include "NiagaraConstants.h"
#include "NiagaraRendererGeometryCache.h"
#include "NiagaraComponent.h"
#include "Modules/ModuleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraGeometryCacheRendererProperties)

#if WITH_EDITOR
#include "Widgets/Images/SImage.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/SWidget.h"
#include "Styling/SlateBrush.h"
#include "AssetThumbnail.h"
#endif

#define LOCTEXT_NAMESPACE "UNiagaraGeometryCacheRendererProperties"

TArray<TWeakObjectPtr<UNiagaraGeometryCacheRendererProperties>> UNiagaraGeometryCacheRendererProperties::GeometryCacheRendererPropertiesToDeferredInit;
FNiagaraVariable UNiagaraGeometryCacheRendererProperties::Particles_GeoCacheRotation;
FNiagaraVariable UNiagaraGeometryCacheRendererProperties::Particles_Age;
FNiagaraVariable UNiagaraGeometryCacheRendererProperties::Particles_GeoCacheIsEnabled;
FNiagaraVariable UNiagaraGeometryCacheRendererProperties::Particles_ArrayIndex;

FNiagaraGeometryCacheReference::FNiagaraGeometryCacheReference()
	: GeometryCache(nullptr)
	, GeometryCacheUserParamBinding(FNiagaraTypeDefinition(UObject::StaticClass()))
{
}

UNiagaraGeometryCacheRendererProperties::UNiagaraGeometryCacheRendererProperties()
{
	AttributeBindings.Reserve(6);
	AttributeBindings.Add(&PositionBinding);
	AttributeBindings.Add(&RotationBinding);
	AttributeBindings.Add(&ScaleBinding);
	AttributeBindings.Add(&ElapsedTimeBinding);
	AttributeBindings.Add(&EnabledBinding);
	AttributeBindings.Add(&RendererVisibilityTagBinding);
	AttributeBindings.Add(&ArrayIndexBinding);

	if (GeometryCaches.Num() == 0)
	{
		GeometryCaches.AddDefaulted();
	}
	GeometryCaches.Shrink();
}

void UNiagaraGeometryCacheRendererProperties::PostLoad()
{
	Super::PostLoad();
	PostLoadBindings(ENiagaraRendererSourceDataMode::Particles);
}

template<typename T>
FNiagaraVariableAttributeBinding CreateDefaultBinding(FNiagaraVariable DefaultValue, const T& DefaultValueData)
{
	DefaultValue.SetValue(DefaultValueData);
	FNiagaraVariableAttributeBinding Binding;
	Binding.Setup(DefaultValue, DefaultValue);
	return Binding;
}

void UNiagaraGeometryCacheRendererProperties::InitBindings()
{
	if (!PositionBinding.IsValid())
	{
		PositionBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_POSITION);
		ScaleBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_SCALE);
		RendererVisibilityTagBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_VISIBILITY_TAG);
		ArrayIndexBinding = CreateDefaultBinding(Particles_ArrayIndex, 0);
		RotationBinding = CreateDefaultBinding(Particles_GeoCacheRotation, FVector3f(0, 0, 0));
		ElapsedTimeBinding = CreateDefaultBinding(Particles_Age, 0.0f);
		EnabledBinding = CreateDefaultBinding(Particles_GeoCacheIsEnabled, false);
	}
}

void UNiagaraGeometryCacheRendererProperties::InitDefaultAttributes()
{
	if (!Particles_GeoCacheRotation.IsValid())
	{
		Particles_GeoCacheRotation = FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Particles.GeoCacheRotation"));
	}
	if (!Particles_Age.IsValid())
	{
		Particles_Age = FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Particles.Age"));
	}
	if (!Particles_GeoCacheIsEnabled.IsValid())
	{
		Particles_GeoCacheIsEnabled = FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Particles.GeoCacheIsEnabled"));
	}
	if (!Particles_ArrayIndex.IsValid())
	{
		Particles_ArrayIndex = FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Particles.GeoCacheArrayIndex"));
	}
}

void UNiagaraGeometryCacheRendererProperties::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		// We can end up hitting PostInitProperties before the Niagara Module has initialized bindings this needs, mark this object for deferred init and early out.
		if (FModuleManager::Get().IsModuleLoaded("Niagara") == false)
		{
			GeometryCacheRendererPropertiesToDeferredInit.Add(this);
		}
		else
		{
			InitDefaultAttributes();
			
			if (!PositionBinding.IsValid())
			{
				PositionBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_POSITION);
			}
			if (!ScaleBinding.IsValid())
			{
				ScaleBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_SCALE);
			}
			if (!RendererVisibilityTagBinding.IsValid())
			{
				RendererVisibilityTagBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_VISIBILITY_TAG);
			}
			if (!RotationBinding.IsValid())
			{
				RotationBinding = CreateDefaultBinding(Particles_GeoCacheRotation, FVector3f(0, 0, 0));
			}
			if (!ElapsedTimeBinding.IsValid())
			{
				ElapsedTimeBinding = CreateDefaultBinding(Particles_Age, 0.0f);
			}
			if (!EnabledBinding.IsValid())
			{
				EnabledBinding = CreateDefaultBinding(Particles_GeoCacheIsEnabled, false);
			}
			if (!ArrayIndexBinding.IsValid())
			{
				ArrayIndexBinding = CreateDefaultBinding(Particles_ArrayIndex, 0);
			}
		}
	}
}

void UNiagaraGeometryCacheRendererProperties::InitCDOPropertiesAfterModuleStartup()
{
	InitDefaultAttributes();
	
	UNiagaraGeometryCacheRendererProperties* CDO = CastChecked<UNiagaraGeometryCacheRendererProperties>(StaticClass()->GetDefaultObject());
	CDO->PositionBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_POSITION);
	CDO->ScaleBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_SCALE);
	CDO->RendererVisibilityTagBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_VISIBILITY_TAG);
	CDO->RotationBinding = CreateDefaultBinding(Particles_GeoCacheRotation, FVector3f(0, 0, 0));
	CDO->ElapsedTimeBinding = CreateDefaultBinding(Particles_Age, 0.0f);
	CDO->EnabledBinding = CreateDefaultBinding(Particles_GeoCacheIsEnabled, false);
	CDO->ArrayIndexBinding = CreateDefaultBinding(Particles_ArrayIndex, 0);

	for (TWeakObjectPtr<UNiagaraGeometryCacheRendererProperties>& WeakGeometryCacheRendererProperties : GeometryCacheRendererPropertiesToDeferredInit)
	{
		if (WeakGeometryCacheRendererProperties.Get())
		{
			if (!WeakGeometryCacheRendererProperties->PositionBinding.IsValid())
			{
				WeakGeometryCacheRendererProperties->PositionBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_POSITION);
			}
			if (!WeakGeometryCacheRendererProperties->ScaleBinding.IsValid())
			{
				WeakGeometryCacheRendererProperties->ScaleBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_SCALE);
			}
			if (!WeakGeometryCacheRendererProperties->RendererVisibilityTagBinding.IsValid())
			{
				WeakGeometryCacheRendererProperties->RendererVisibilityTagBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_VISIBILITY_TAG);
			}
			if (!WeakGeometryCacheRendererProperties->RotationBinding.IsValid())
			{
				WeakGeometryCacheRendererProperties->RotationBinding = CreateDefaultBinding(Particles_GeoCacheRotation, FVector3f(0, 0, 0));
			}
			if (!WeakGeometryCacheRendererProperties->ElapsedTimeBinding.IsValid())
			{
				WeakGeometryCacheRendererProperties->ElapsedTimeBinding = CreateDefaultBinding(Particles_Age, 0.0f);
			}
			if (!WeakGeometryCacheRendererProperties->EnabledBinding.IsValid())
			{
				WeakGeometryCacheRendererProperties->EnabledBinding = CreateDefaultBinding(Particles_GeoCacheIsEnabled, false);
			}
			if (!WeakGeometryCacheRendererProperties->ArrayIndexBinding.IsValid())
			{
				WeakGeometryCacheRendererProperties->ArrayIndexBinding = CreateDefaultBinding(Particles_ArrayIndex, 0);
			}
		}
	}
}

FNiagaraRenderer* UNiagaraGeometryCacheRendererProperties::CreateEmitterRenderer(ERHIFeatureLevel::Type FeatureLevel, const FNiagaraEmitterInstance* Emitter, const FNiagaraSystemInstanceController& InController)
{
	FNiagaraRenderer* NewRenderer = new FNiagaraRendererGeometryCache(FeatureLevel, this, Emitter);
	NewRenderer->Initialize(this, Emitter, InController);
	return NewRenderer;
}

void UNiagaraGeometryCacheRendererProperties::GetUsedMaterials(const FNiagaraEmitterInstance* InEmitter, TArray<UMaterialInterface*>& MaterialInterfaces) const
{
	for (const FNiagaraGeometryCacheReference& Entry : GeometryCaches)
	{
		if (Entry.GeometryCache)
		{
			int32 MaxIndex = FMath::Max(Entry.GeometryCache->Materials.Num(), Entry.OverrideMaterials.Num());
			for (int i = 0; i < MaxIndex; i++)
			{
				if (Entry.OverrideMaterials.IsValidIndex(i) && Entry.OverrideMaterials[i])
				{
					MaterialInterfaces.Add(Entry.OverrideMaterials[i]);
				}
				else if (Entry.GeometryCache->Materials.IsValidIndex(i))
				{
					MaterialInterfaces.Add(Entry.GeometryCache->Materials[i]);
				}
			}
		}
	}
}

bool UNiagaraGeometryCacheRendererProperties::PopulateRequiredBindings(FNiagaraParameterStore& InParameterStore)
{
	bool bAnyAdded = false;

	for (const FNiagaraVariableAttributeBinding* Binding : AttributeBindings)
	{
		if (Binding && Binding->CanBindToHostParameterMap())
		{
			InParameterStore.AddParameter(Binding->GetParamMapBindableVariable(), false);
			bAnyAdded = true;
		}
	}

	for (const FNiagaraGeometryCacheReference& Entry : GeometryCaches)
	{
		FNiagaraVariable Variable = Entry.GeometryCacheUserParamBinding.Parameter;
		if (Variable.IsValid())
		{
			InParameterStore.AddParameter(Variable, false);
			bAnyAdded = true;
		}
	}

	return bAnyAdded;
}

#if WITH_EDITORONLY_DATA

const FSlateBrush* UNiagaraGeometryCacheRendererProperties::GetStackIcon() const
{
	return FSlateIconFinder::FindIconBrushForClass(GetClass());
}

void UNiagaraGeometryCacheRendererProperties::GetRendererWidgets(const FNiagaraEmitterInstance* InEmitter, TArray<TSharedPtr<SWidget>>& OutWidgets, TSharedPtr<FAssetThumbnailPool> InThumbnailPool) const
{
	TSharedRef<SWidget> DefaultThumbnailWidget = SNew(SImage)
		.Image(FSlateIconFinder::FindIconBrushForClass(StaticClass()));

	int32 ThumbnailSize = 32;
	for(const FNiagaraGeometryCacheReference& Entry : GeometryCaches)
	{
		TSharedPtr<SWidget> ThumbnailWidget = DefaultThumbnailWidget;

		UGeometryCache* GeometryCache = Entry.GeometryCache;
		if (GeometryCache)
		{
			TSharedPtr<FAssetThumbnail> AssetThumbnail = MakeShareable(new FAssetThumbnail(GeometryCache, ThumbnailSize, ThumbnailSize, InThumbnailPool));
			ThumbnailWidget = AssetThumbnail->MakeThumbnailWidget();
		}
		
		OutWidgets.Add(ThumbnailWidget);
	}

	if (GeometryCaches.Num() == 0)
	{
		OutWidgets.Add(DefaultThumbnailWidget);
	}
}

void UNiagaraGeometryCacheRendererProperties::GetRendererTooltipWidgets(const FNiagaraEmitterInstance* InEmitter, TArray<TSharedPtr<SWidget>>& OutWidgets, TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool) const
{
	TSharedRef<SWidget> DefaultGeoCacheTooltip = SNew(STextBlock)
		.Text(LOCTEXT("GeoCacheRenderer", "Geometry Cache Renderer"));
	
	TArray<TSharedPtr<SWidget>> RendererWidgets;
	if (GeometryCaches.Num() > 0)
	{
		GetRendererWidgets(InEmitter, RendererWidgets, AssetThumbnailPool);
	}
	
	for(int32 Index = 0; Index < GeometryCaches.Num(); Index++)
	{
		const FNiagaraGeometryCacheReference& Entry = GeometryCaches[Index];
		
		TSharedPtr<SWidget> TooltipWidget = DefaultGeoCacheTooltip;		
		// we make sure to reuse the asset widget as a thumbnail if the geometry cache is valid
		if(Entry.GeometryCache)
		{
			TooltipWidget = RendererWidgets[Index];
		}

		// we override the previous thumbnail tooltip with a text indicating user parameter binding, if it exists
		if(Entry.GeometryCacheUserParamBinding.Parameter.IsValid())
		{
			TooltipWidget = SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("GeoCacheBoundTooltip", "Geometry cache slot is bound to user parameter {0}"), FText::FromName(Entry.GeometryCacheUserParamBinding.Parameter.GetName())));
		}
		
		OutWidgets.Add(TooltipWidget);
	}

	if (GeometryCaches.Num() == 0)
	{
		OutWidgets.Add(DefaultGeoCacheTooltip);
	}
}

const TArray<FNiagaraVariable>& UNiagaraGeometryCacheRendererProperties::GetOptionalAttributes()
{
	static TArray<FNiagaraVariable> Attrs;
	if (Attrs.Num() == 0)
	{
		Attrs.Add(SYS_PARAM_PARTICLES_POSITION);
		Attrs.Add(SYS_PARAM_PARTICLES_SCALE);
		Attrs.Add(SYS_PARAM_PARTICLES_VISIBILITY_TAG);
		Attrs.Add(Particles_GeoCacheRotation);
		Attrs.Add(Particles_Age);
		Attrs.Add(Particles_GeoCacheIsEnabled);
		Attrs.Add(Particles_ArrayIndex);
	}
	return Attrs;
}

void UNiagaraGeometryCacheRendererProperties::GetRendererFeedback(const FVersionedNiagaraEmitter& InEmitter, TArray<FText>& OutErrors, TArray<FText>& OutWarnings, TArray<FText>& OutInfo) const
{
	OutInfo.Add(FText::FromString(TEXT("The geometry cache renderer is still an experimental feature.")));
}

#endif

#undef LOCTEXT_NAMESPACE
