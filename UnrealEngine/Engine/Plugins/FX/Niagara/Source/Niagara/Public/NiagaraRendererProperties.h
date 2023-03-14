// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "RHIDefinitions.h"
#include "NiagaraTypes.h"
#include "NiagaraCommon.h"
#include "NiagaraMergeable.h"
#include "NiagaraPlatformSet.h"
#include "NiagaraRendererProperties.generated.h"

struct FVersionedNiagaraEmitterData;
class FNiagaraRenderer;
class FNiagaraSystemInstanceController;
class UMaterial;
class UMaterialInterface;
class FNiagaraEmitterInstance;
class SWidget;
class FAssetThumbnailPool;
struct FNiagaraDataSetCompiledData;
struct FSlateBrush;

#if WITH_EDITOR
// Helper class for GUI error handling
DECLARE_DELEGATE(FNiagaraRendererFeedbackFix);
class FNiagaraRendererFeedback
{
public:
	FNiagaraRendererFeedback(FText InDescriptionText, FText InSummaryText, FText InFixDescription = FText(), FNiagaraRendererFeedbackFix InFix = FNiagaraRendererFeedbackFix(), bool InDismissable = false)
		: DescriptionText(InDescriptionText)
		  , SummaryText(InSummaryText)
		  , FixDescription(InFixDescription)
		  , Fix(InFix)
		  , Dismissable(InDismissable)
	{}

	FNiagaraRendererFeedback(FText InSummaryText)
        : DescriptionText(FText())
          , SummaryText(InSummaryText)
          , FixDescription(FText())
          , Fix(FNiagaraRendererFeedbackFix())
	{}

	FNiagaraRendererFeedback()
	{}

	/** Returns true if the problem can be fixed automatically. */
	bool IsFixable() const
	{
		return Fix.IsBound();
	}

	/** Applies the fix if a delegate is bound for it.*/
	void TryFix() const
	{
		if (Fix.IsBound())
		{
			Fix.Execute();
		}
	}

	/** Full description text */
	FText GetDescriptionText() const
	{
		return DescriptionText;
	}

	/** Shortened error description text*/
	FText GetSummaryText() const
	{
		return SummaryText;
	}

	/** Full description text */
	FText GetFixDescriptionText() const
	{
		return FixDescription;
	}

	bool IsDismissable() const
	{
		return Dismissable;
	}

private:
	FText DescriptionText;
	FText SummaryText;
	FText FixDescription;
	FNiagaraRendererFeedbackFix Fix;
	bool Dismissable = false;
};
#endif

/** Mapping between a variable in the source dataset and the location we place it in the GPU buffer passed to the VF. */
struct FNiagaraRendererVariableInfo
{
	FNiagaraRendererVariableInfo() {}
	FNiagaraRendererVariableInfo(int32 InDataOffset, int32 InGPUBufferOffset, int32 InNumComponents, bool bInUpload, bool bInHalfType)
		: DatasetOffset(InDataOffset)
		, GPUBufferOffset(InGPUBufferOffset)
		, NumComponents(InNumComponents)
		, bUpload(bInUpload)
		, bHalfType(bInHalfType)
	{
	}

	FORCEINLINE int32 GetGPUOffset() const
	{
		int32 Offset = GPUBufferOffset;
		if (bHalfType)
		{
			Offset |= 1 << 31;
		}
		return Offset;
	}

	FORCEINLINE int32 GetEncodedDatasetOffset() const
	{
		return DatasetOffset | (bHalfType ? (1<<31) : 0);
	}

	int32 DatasetOffset = INDEX_NONE;
	int32 GPUBufferOffset = INDEX_NONE;
	int32 NumComponents = 0;
	bool bUpload = false;
	bool bHalfType = false;
};

/** Used for building renderer layouts for vertex factories */
struct NIAGARA_API FNiagaraRendererLayout
{
	void Initialize(int32 NumVariables);
	bool SetVariable(const FNiagaraDataSetCompiledData* CompiledData, const FNiagaraVariableBase& Variable, int32 VFVarOffset);
	bool SetVariableFromBinding(const FNiagaraDataSetCompiledData* CompiledData, const FNiagaraVariableAttributeBinding& VariableBinding, int32 VFVarOffset);
	void Finalize();

	TConstArrayView<FNiagaraRendererVariableInfo> GetVFVariables_RenderThread() const { check(IsInRenderingThread()); return MakeArrayView(VFVariables_RT); }
	int32 GetTotalFloatComponents_RenderThread() const { check(IsInRenderingThread()); return TotalFloatComponents_RT; }
	int32 GetTotalHalfComponents_RenderThread() const { check(IsInRenderingThread()); return TotalHalfComponents_RT; }

private:
	TArray<FNiagaraRendererVariableInfo> VFVariables_GT;
	int32 TotalFloatComponents_GT;
	int32 TotalHalfComponents_GT;

	TArray<FNiagaraRendererVariableInfo> VFVariables_RT;
	int32 TotalFloatComponents_RT;
	int32 TotalHalfComponents_RT;
};

UENUM()
enum class ENiagaraRendererSortPrecision : uint8
{
	/** Uses the project settings value. */
	Default,
	/** Low precision sorting, half float (fp16) precision, faster and adequate for most cases. */
	Low,
	/** High precision sorting, float (fp32) precision, slower but may fix sorting artifacts. */
	High,
};

UENUM()
enum class ENiagaraRendererGpuTranslucentLatency : uint8
{
	/** Uses the project default value. */
	ProjectDefault,
	/** Gpu simulations will always read this frames data for translucent materials. */
	Immediate,
	/** Gpu simulations will read the previous frames data if the simulation has to run in PostRenderOpaque. */
	Latent,
};

USTRUCT()
struct FNiagaraRendererMaterialScalarParameter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Material")
	FName MaterialParameterName;

	UPROPERTY(EditAnywhere, Category = "Material")
	float Value = 0.0f;
};

USTRUCT()
struct FNiagaraRendererMaterialVectorParameter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Material")
	FName MaterialParameterName;

	UPROPERTY(EditAnywhere, Category = "Material")
	FLinearColor Value = FLinearColor::Black;
};

USTRUCT()
struct FNiagaraRendererMaterialTextureParameter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Material")
	FName MaterialParameterName;

	UPROPERTY(EditAnywhere, Category = "Material")
	TObjectPtr<UTexture> Texture;
};

/**
* Parameters to apply to the material, these are both constant and dynamic bindings
* Having any bindings set will cause a MID to be generated
*/
USTRUCT()
struct FNiagaraRendererMaterialParameters
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Material")
	TArray<FNiagaraMaterialAttributeBinding> AttributeBindings;

	UPROPERTY(EditAnywhere, Category = "Material")
	TArray<FNiagaraRendererMaterialScalarParameter> ScalarParameters;

	UPROPERTY(EditAnywhere, Category = "Material")
	TArray<FNiagaraRendererMaterialVectorParameter> VectorParameters;

	UPROPERTY(EditAnywhere, Category = "Material")
	TArray<FNiagaraRendererMaterialTextureParameter> TextureParameters;

#if WITH_EDITORONLY_DATA
	void GetFeedback(TArrayView<UMaterialInterface*> Materials, TArray<FNiagaraRendererFeedback>& OutWarnings) const;
#endif

	bool HasAnyBindings() const
	{
		return AttributeBindings.Num() > 0 || ScalarParameters.Num() > 0 || VectorParameters.Num() > 0 || TextureParameters.Num() > 0;
	}
};

/**
* Emitter properties base class
* Each EmitterRenderer derives from this with its own class, and returns it in GetProperties; a copy
* of those specific properties is stored on UNiagaraEmitter (on the System) for serialization
* and handed back to the System renderer on load.
*/
UCLASS(ABSTRACT)
class NIAGARA_API UNiagaraRendererProperties : public UNiagaraMergeable
{
	GENERATED_BODY()
	
public:
#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE(FOnPropertiesChanged);
#endif
public:
	UNiagaraRendererProperties()		
		: bIsEnabled(true)
		, bAllowInCullProxies(true)
		, bMotionBlurEnabled_DEPRECATED(true)
	{
	}

	//UObject Interface Begin
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
#if WITH_EDITORONLY_DATA
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	//UObject Interface End
	
	virtual FNiagaraRenderer* CreateEmitterRenderer(ERHIFeatureLevel::Type FeatureLevel, const FNiagaraEmitterInstance* Emitter, const FNiagaraSystemInstanceController& InController) PURE_VIRTUAL ( UNiagaraRendererProperties::CreateEmitterRenderer, return nullptr;);
	virtual class FNiagaraBoundsCalculator* CreateBoundsCalculator() PURE_VIRTUAL(UNiagaraRendererProperties::CreateBoundsCalculator, return nullptr;);
	virtual void GetUsedMaterials(const FNiagaraEmitterInstance* InEmitter, TArray<UMaterialInterface*>& OutMaterials) const PURE_VIRTUAL(UNiagaraRendererProperties::GetUsedMaterials,);
	virtual const FVertexFactoryType* GetVertexFactoryType() const { return nullptr; }
	virtual bool IsBackfaceCullingDisabled() const { return false; }

	virtual void GetStreamingMeshInfo(const FBoxSphereBounds& OwnerBounds, const FNiagaraEmitterInstance* InEmitter, TArray<FStreamingRenderAssetPrimitiveInfo>& OutStreamingRenderAssets) const {}

	virtual bool IsSimTargetSupported(ENiagaraSimTarget InSimTarget) const { return false; };

	const TArray<const FNiagaraVariableAttributeBinding*>& GetAttributeBindings() const { return AttributeBindings; }
	uint32 ComputeMaxUsedComponents(const FNiagaraDataSetCompiledData* CompiledDataSetData) const;

	virtual bool NeedsLoadForTargetPlatform(const class ITargetPlatform* TargetPlatform) const override;

	/** Method to add asset tags that are specific to this renderer. By default we add in how many instances of this class exist in the list.*/
	virtual void GetAssetTagsForContext(const UObject* InAsset, FGuid AssetVersion, const TArray<const UNiagaraRendererProperties*>& InProperties, TMap<FName, uint32>& NumericKeys, TMap<FName, FString>& StringKeys) const;

	/** In the case that we need parameters bound in that aren't Particle variables, these should be set up here so that the data is appropriately populated after the simulation.*/
	virtual bool PopulateRequiredBindings(FNiagaraParameterStore& InParameterStore);

#if WITH_EDITORONLY_DATA

	virtual bool IsSupportedVariableForBinding(const FNiagaraVariableBase& InSourceForBinding, const FName& InTargetBindingName) const;

	/** Internal handling of any emitter variable renames. Note that this doesn't modify the renderer, the caller will need to do that if it is desired.*/
	virtual void RenameEmitter(const FName& InOldName, const UNiagaraEmitter* InRenamedEmitter);
	virtual void RenameVariable(const FNiagaraVariableBase& OldVariable, const FNiagaraVariableBase& NewVariable, const FVersionedNiagaraEmitter& InEmitter);
	virtual void RemoveVariable(const FNiagaraVariableBase& OldVariable, const FVersionedNiagaraEmitter& InEmitter);
	virtual bool IsMaterialValidForRenderer(UMaterial* Material, FText& InvalidMessage) { return true; }

	virtual void FixMaterial(UMaterial* Material) { }

	virtual TArray<FNiagaraVariable> GetBoundAttributes() const;

	virtual const TArray<FNiagaraVariable>& GetRequiredAttributes() { static TArray<FNiagaraVariable> Vars; return Vars; };
	virtual const TArray<FNiagaraVariable>& GetOptionalAttributes() { static TArray<FNiagaraVariable> Vars; return Vars; };

	virtual void GetAdditionalVariables(TArray<FNiagaraVariableBase>& OutArray) const { };

	UNiagaraRendererProperties* StaticDuplicateWithNewMergeId(UObject* InOuter) const
	{
		return CastChecked<UNiagaraRendererProperties>(Super::StaticDuplicateWithNewMergeIdInternal(InOuter));
	}

	virtual void GetRendererWidgets(const FNiagaraEmitterInstance* InEmitter, TArray<TSharedPtr<SWidget>>& OutWidgets, TSharedPtr<FAssetThumbnailPool> InThumbnailPool) const PURE_VIRTUAL(UNiagaraRendererProperties::GetRendererWidgets, );
	virtual void GetRendererTooltipWidgets(const FNiagaraEmitterInstance* InEmitter, TArray<TSharedPtr<SWidget>>& OutWidgets, TSharedPtr<FAssetThumbnailPool> InThumbnailPool) const PURE_VIRTUAL(UNiagaraRendererProperties::GetRendererTooltipWidgets, );
	virtual void GetRendererFeedback(const FVersionedNiagaraEmitter& InEmitter, TArray<FText>& OutErrors, TArray<FText>& OutWarnings, TArray<FText>& OutInfo) const {};
	virtual void GetRendererFeedback(const FVersionedNiagaraEmitter& InEmitter, TArray<FNiagaraRendererFeedback>& OutErrors, TArray<FNiagaraRendererFeedback>& OutWarnings, TArray<FNiagaraRendererFeedback>& OutInfo) const;

	// The icon to display in the niagara stack widget under the renderer section
	virtual const FSlateBrush* GetStackIcon() const;

	// The text to display in the niagara stack widget under the renderer section
	virtual FText GetWidgetDisplayName() const;
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	FOnPropertiesChanged& OnPropertiesChanged();
#endif
	virtual ENiagaraRendererSourceDataMode GetCurrentSourceMode() const {	return ENiagaraRendererSourceDataMode::Particles;}

	virtual bool GetIsActive() const;
	virtual bool GetIsEnabled() const { return bIsEnabled; }
	virtual void SetIsEnabled(bool bInIsEnabled);

	virtual void CacheFromCompiledData(const FNiagaraDataSetCompiledData* CompiledData) {}

	virtual bool NeedsMIDsForMaterials() const { return false; }

	/** When true, the renderer will be opted in to get its PostSystemTick_GameThread overload called */
	virtual bool NeedsSystemPostTick() const { return false; }
	/** When true, the renderer will be opted in to get its OnSystemComplete_GameThread overload called */
	virtual bool NeedsSystemCompletion() const { return false; }

	bool NeedsPreciseMotionVectors() const;

	static bool IsSortHighPrecision(ENiagaraRendererSortPrecision SortPrecision);

	static bool IsGpuTranslucentThisFrame(ENiagaraRendererGpuTranslucentLatency Latency);

	template<typename TAction>
	void ForEachPlatformSet(TAction Func);

	FVersionedNiagaraEmitterData* GetEmitterData() const;
	FVersionedNiagaraEmitter GetOuterEmitter() const;

	/** Platforms on which this renderer is enabled. */
	UPROPERTY(EditAnywhere, Category = "Scalability", meta=(DisplayInScalabilityContext))
	FNiagaraPlatformSet Platforms;

	/** By default, emitters are drawn in the order that they are added to the system. This value will allow you to control the order in a more fine-grained manner.
	Materials of the same type (i.e. Transparent) will draw in order from lowest to highest within the system. The default value is 0.*/
	UPROPERTY(EditAnywhere, Category = "Sort Order")
	int32 SortOrderHint;

	/** Hint about how to generate motion (velocity) vectors for this renderer. */
	UPROPERTY(EditAnywhere, Category = "Motion Blur")
	ENiagaraRendererMotionVectorSetting MotionVectorSetting;

	/** Optional bool binding to dynamically enable / disable the renderer. */
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding RendererEnabledBinding;

	UPROPERTY()
	bool bIsEnabled;

	UPROPERTY(EditAnywhere, Category = "Scalability")
	bool bAllowInCullProxies;

	UPROPERTY()
	FGuid OuterEmitterVersion;
	
protected:
	UPROPERTY()
	bool bMotionBlurEnabled_DEPRECATED; // This has been rolled into MotionVectorSetting

	TArray<const FNiagaraVariableAttributeBinding*> AttributeBindings;

	virtual void PostLoadBindings(ENiagaraRendererSourceDataMode InSourceMode);
	virtual void UpdateSourceModeDerivates(ENiagaraRendererSourceDataMode InSourceMode, bool bFromPropertyEdit = false);

#if WITH_EDITORONLY_DATA
	/** returns the variable associated with the supplied binding if it should be bound given the current settings of the RendererProperties. */
	virtual FNiagaraVariable GetBoundAttribute(const FNiagaraVariableAttributeBinding* Binding) const;

	/** utility function that can be used to fix up old vec3 bindings into position bindings. */
	static void ChangeToPositionBinding(FNiagaraVariableAttributeBinding& Binding);
#endif

#if WITH_EDITOR
	FOnPropertiesChanged OnPropertiesChangedDelegate;
#endif
};

template<typename TAction>
void UNiagaraRendererProperties::ForEachPlatformSet(TAction Func)
{
	Func(Platforms);
}
