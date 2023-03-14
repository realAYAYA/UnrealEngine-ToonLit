// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraScript.h"
#include "Engine/DeveloperSettings.h"
#include "NiagaraSpawnShortcut.h"
#include "NiagaraEditorSettings.generated.h"

class UCurveFloat;
enum class EScriptSource : uint8;

namespace FNiagaraEditorGuids
{
	const extern FGuid SystemNamespaceMetaDataGuid;
	const extern FGuid EmitterNamespaceMetaDataGuid;
	const extern FGuid ParticleAttributeNamespaceMetaDataGuid;
	const extern FGuid ModuleNamespaceMetaDataGuid;
	const extern FGuid ModuleOutputNamespaceMetaDataGuid;
	const extern FGuid ModuleLocalNamespaceMetaDataGuid;
	const extern FGuid TransientNamespaceMetaDataGuid;
	const extern FGuid StackContextNamespaceMetaDataGuid;
	const extern FGuid EngineNamespaceMetaDataGuid;
	const extern FGuid UserNamespaceMetaDataGuid;
	const extern FGuid ParameterCollectionNamespaceMetaDataGuid;
	const extern FGuid DataInstanceNamespaceMetaDataGuid;
	const extern FGuid StaticSwitchNamespaceMetaDataGuid;
}

USTRUCT()
struct FNiagaraNewAssetDialogConfig
{
	GENERATED_BODY()

	UPROPERTY()
	int32 SelectedOptionIndex;

	UPROPERTY()
	FVector2D WindowSize;

	FNiagaraNewAssetDialogConfig()
	{
		SelectedOptionIndex = 0;
		WindowSize = FVector2D(450, 600);
	}
};

UENUM()
enum class ENiagaraNamespaceMetadataOptions
{
	HideInScript,
	HideInSystem,
	AdvancedInScript,
	AdvancedInSystem,
	PreventEditingNamespace,
	PreventEditingNamespaceModifier,
	PreventEditingName,
	PreventCreatingInSystemEditor,
	HideInDefinitions,
};

USTRUCT()
struct NIAGARAEDITOR_API FNiagaraNamespaceMetadata
{
	GENERATED_BODY()

	FNiagaraNamespaceMetadata();

	FNiagaraNamespaceMetadata(TArray<FName> InNamespaces, FName InRequiredNamespaceModifier = NAME_None);

	//FNiagaraNamespaceMetadata(const FNiagaraNamespaceMetadata& Other);

	bool operator==(const FNiagaraNamespaceMetadata& Other) const
	{
		return
			Namespaces == Other.Namespaces &&
			RequiredNamespaceModifier == Other.RequiredNamespaceModifier &&
			DisplayName.IdenticalTo(Other.DisplayName) &&
			DisplayNameLong.IdenticalTo(Other.DisplayNameLong) &&
			Description.IdenticalTo(Other.Description) &&
			BackgroundColor == Other.BackgroundColor &&
			ForegroundStyle == Other.ForegroundStyle &&
			SortId == Other.SortId &&
			OptionalNamespaceModifiers == Other.OptionalNamespaceModifiers &&
			Options == Other.Options;
	}

	UPROPERTY()
	TArray<FName> Namespaces;

	UPROPERTY()
	FName RequiredNamespaceModifier;

	UPROPERTY()
	FText DisplayName;

	UPROPERTY()
	FText DisplayNameLong;

	UPROPERTY()
	FText Description;

	UPROPERTY()
	FLinearColor BackgroundColor;

	UPROPERTY()
	FName ForegroundStyle;

	UPROPERTY()
	int32 SortId;

	UPROPERTY()
	TArray<FName> OptionalNamespaceModifiers;

	UPROPERTY()
	TArray<ENiagaraNamespaceMetadataOptions> Options;

	FNiagaraNamespaceMetadata& SetDisplayName(FText InDisplayName)
	{
		DisplayName = InDisplayName;
		return *this;
	}

	FNiagaraNamespaceMetadata& SetDisplayNameLong(FText InDisplayNameLong)
	{
		DisplayNameLong = InDisplayNameLong;
		return *this;
	}

	FNiagaraNamespaceMetadata& SetDescription(FText InDescription)
	{
		Description = InDescription;
		return *this;
	}

	FNiagaraNamespaceMetadata& SetBackgroundColor(FLinearColor InBackgroundColor)
	{
		BackgroundColor = InBackgroundColor;
		return *this;
	}

	FNiagaraNamespaceMetadata& SetForegroundStyle(FName InForegroundStyle)
	{
		ForegroundStyle = InForegroundStyle;
		return *this;
	}

	FNiagaraNamespaceMetadata& SetSortId(int32 InSortId)
	{
		SortId = InSortId;
		return *this;
	}

	FNiagaraNamespaceMetadata& AddOptionalNamespaceModifier(FName InOptionalNamespaceModifier)
	{
		OptionalNamespaceModifiers.Add(InOptionalNamespaceModifier);
		return *this;
	}

	FNiagaraNamespaceMetadata& AddOption(ENiagaraNamespaceMetadataOptions Option)
	{
		Options.Add(Option);
		return *this;
	}

	FNiagaraNamespaceMetadata& SetGuid(const FGuid& NewGuid)
	{ 
		Guid = NewGuid; 
		return *this;
	}

	bool IsValid() const { return Namespaces.Num() > 0; }

	const FGuid& GetGuid() const { return Guid; }

private:
	UPROPERTY(Transient)
	FGuid Guid;
};

USTRUCT()
struct NIAGARAEDITOR_API FNiagaraCurveTemplate
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, Category = Curve)
	FString DisplayNameOverride;

	UPROPERTY(EditAnywhere, Category = Curve, meta = (AllowedClasses = "/Script/Engine.CurveFloat"))
	FSoftObjectPath CurveAsset;
};

USTRUCT()
struct NIAGARAEDITOR_API FNiagaraActionColors
{
	GENERATED_BODY()

	FNiagaraActionColors();
	
	UPROPERTY(EditAnywhere, Category = Color)
	FLinearColor NiagaraColor;

	UPROPERTY(EditAnywhere, Category = Color)
	FLinearColor GameColor;

	UPROPERTY(EditAnywhere, Category = Color)
	FLinearColor PluginColor;

	UPROPERTY(EditAnywhere, Category = Color)
	FLinearColor DeveloperColor;
};


USTRUCT()
struct NIAGARAEDITOR_API FNiagaraParameterPanelSectionStorage
{
	GENERATED_BODY()
		
	FNiagaraParameterPanelSectionStorage() {}

	FNiagaraParameterPanelSectionStorage(const FGuid& InGuid) : ParamStorageId(InGuid) 
	{
		check(InGuid.IsValid());
	}

	UPROPERTY()
	FGuid ParamStorageId;

	UPROPERTY()
	TArray<FGuid> ExpandedCategories;
};

FORCEINLINE uint32 GetTypeHash(const FNiagaraNamespaceMetadata& NamespaceMetaData)
{
	return GetTypeHash(NamespaceMetaData.GetGuid());
}

UCLASS(config = Niagara, defaultconfig, meta=(DisplayName="Niagara"))
class NIAGARAEDITOR_API UNiagaraEditorSettings : public UDeveloperSettings
{
public:
	GENERATED_UCLASS_BODY()

	/** Niagara script to duplicate as the base of all new script assets created. */
	UPROPERTY(config, EditAnywhere, Category = Niagara)
	FSoftObjectPath DefaultScript;

	/** Niagara script to duplicate as the base of all new script assets created. */
	UPROPERTY(config, EditAnywhere, Category = Niagara)
	FSoftObjectPath DefaultDynamicInputScript;

	/** Niagara script to duplicate as the base of all new script assets created. */
	UPROPERTY(config, EditAnywhere, Category = Niagara)
	FSoftObjectPath DefaultFunctionScript;

	/** Niagara script to duplicate as the base of all new script assets created. */
	UPROPERTY(config, EditAnywhere, Category = Niagara)
	FSoftObjectPath DefaultModuleScript;

	/** Niagara script which is required in the system update script to control system state. */
	UPROPERTY(config, EditAnywhere, Category = Niagara)
	FSoftObjectPath RequiredSystemUpdateScript;

	/** Shortcut key bindings that if held down while doing a mouse click, will spawn the specified type of Niagara node.*/
	UPROPERTY(config, EditAnywhere, Category = Niagara)
	TArray<FNiagaraSpawnShortcut> GraphCreationShortcuts;

	/** If true then emitter and system nodes will show a simplified representation on low zoom levels. This improves performance and readablity when zoomed out of the system overview graph. */
	UPROPERTY(config, EditAnywhere, Category = Niagara)
	bool bSimplifyStackNodesAtLowResolution = true;

	/** The max number of chars before names on the low resolution nodes are truncated. */
	UPROPERTY(config, EditAnywhere, Category = Niagara, meta=(EditCondition="bSimplifyStackNodesAtLowResolution", UIMin=3))
	int32 LowResolutionNodeMaxNameChars = 7;
	
	/** If true then the system editor will zoom to fit all emitters when opening an asset. */
	UPROPERTY(config, EditAnywhere, Category = Niagara)
	bool bAlwaysZoomToFitSystemGraph = true;

	TArray<float> GetPlaybackSpeeds() const;

	/** Gets whether or not auto-compile is enabled in the editors. */
	bool GetAutoCompile() const;

	/** Sets whether or not auto-compile is enabled in the editors. */
	void SetAutoCompile(bool bInAutoCompile);

	/** Gets whether or not simulations should start playing automatically when the emitter or system editor is opened, or when the data is changed in the editor. */
	bool GetAutoPlay() const;

	/** Sets whether or not simulations should start playing automatically when the emitter or system editor is opened, or when the data is changed in the editor. */
	void SetAutoPlay(bool bInAutoPlay);

	/** Gets whether or not the simulation should reset when a value on the emitter or system is changed. */
	bool GetResetSimulationOnChange() const;

	/** Sets whether or not the simulation should reset when a value on the emitter or system is changed. */
	void SetResetSimulationOnChange(bool bInResetSimulationOnChange);

	/** Gets whether or not to rerun the simulation to the current time when making modifications while paused. */
	bool GetResimulateOnChangeWhilePaused() const;

	/** Sets whether or not to rerun the simulation to the current time when making modifications while paused. */
	void SetResimulateOnChangeWhilePaused(bool bInResimulateOnChangeWhilePaused);

	/** Gets whether or not to reset all components that include the system that is currently being reset */
	bool GetResetDependentSystemsWhenEditingEmitters() const;

	/** Sets whether or not to reset all components that include the system that is currently being reset */
	void SetResetDependentSystemsWhenEditingEmitters(bool bInResetDependentSystemsWhenEditingEmitters);

	/** Gets whether or not to display advanced categories for the parameter panel. */
	bool GetDisplayAdvancedParameterPanelCategories() const;

	/** Sets whether or not to display advanced categories for the parameter panel. */
	void SetDisplayAdvancedParameterPanelCategories(bool bInDisplayAdvancedParameterPanelCategories);

	bool GetDisplayAffectedAssetStats() const;
	int32 GetAssetStatsSearchLimit() const;

	bool IsShowGridInViewport() const;
	void SetShowGridInViewport(bool bShowGridInViewport);
	
	bool IsShowParticleCountsInViewport() const;
	void SetShowParticleCountsInViewport(bool bShowParticleCountsInViewport);
	
	FNiagaraNewAssetDialogConfig GetNewAssetDailogConfig(FName InDialogConfigKey) const;

	void SetNewAssetDialogConfig(FName InDialogConfigKey, const FNiagaraNewAssetDialogConfig& InNewAssetDialogConfig);

	FNiagaraNamespaceMetadata GetDefaultNamespaceMetadata() const;
	FNiagaraNamespaceMetadata GetMetaDataForNamespaces(TArray<FName> Namespaces) const;
	FNiagaraNamespaceMetadata GetMetaDataForId(const FGuid& NamespaceId) const;
	const FGuid& GetIdForUsage(ENiagaraScriptUsage Usage) const;
	const TArray<FNiagaraNamespaceMetadata>& GetAllNamespaceMetadata() const;

	FNiagaraNamespaceMetadata GetDefaultNamespaceModifierMetadata() const;
	FNiagaraNamespaceMetadata GetMetaDataForNamespaceModifier(FName NamespaceModifier) const;
	const TArray<FNiagaraNamespaceMetadata>& GetAllNamespaceModifierMetadata() const;

	const TArray<FNiagaraCurveTemplate>& GetCurveTemplates() const;
	
	// Begin UDeveloperSettings Interface
	virtual FName GetCategoryName() const override;
	virtual FText GetSectionText() const override;
	// END UDeveloperSettings Interface

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnNiagaraEditorSettingsChanged, const FString&, const UNiagaraEditorSettings*);

	/** Gets a multicast delegate which is called whenever one of the parameters in this settings object changes. */
	static FOnNiagaraEditorSettingsChanged& OnSettingsChanged();

	const TMap<FString, FString>& GetHLSLKeywordReplacementsMap()const { return HLSLKeywordReplacements; }

	FLinearColor GetSourceColor(EScriptSource Source) const;

	FNiagaraParameterPanelSectionStorage& FindOrAddParameterPanelSectionStorage(FGuid PanelSectionId, bool& bOutAdded);

	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnIsClassAllowed, const UClass* /*InClass*/);
	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnIsClassPathAllowed, const FTopLevelAssetPath& /*InClassPath*/);
	
	/** Sets a delegate that allows external code to restrict which features can be used within the niagara editor by filtering which classes are allowed. */
	void SetOnIsClassAllowed(const FOnIsClassAllowed& InOnIsClassAllowed);

	/** Sets a delegate that allows external code to restrict which features can be used within the niagara editor by filtering which classes are allowed by class path. */
	void SetOnIsClassPathAllowed(const FOnIsClassPathAllowed& InOnIsClassPathAllowed);

	/** Returns whether or not the supplied class can be used in the current editor context. */
	bool IsAllowedClass(const UClass* InClass) const;

	/** Returns whether or not the class referenced by the supplied class path can be used in the current editor context. */
	bool IsAllowedClassPath(const FTopLevelAssetPath& InClassPath) const;

	/** Returns whether or not the supplied niagara type definition can be used in the current editor context. */
	bool IsAllowedTypeDefinition(const FNiagaraTypeDefinition& InTypeDefinition) const;

private:
	void SetupNamespaceMetadata();
	void BuildCachedPlaybackSpeeds() const;
protected:
	FOnNiagaraEditorSettingsChanged SettingsChangedDelegate;
private:
	/** Whether or not auto-compile is enabled in the editors. */
	UPROPERTY(config, EditAnywhere, Category = Niagara)
	bool bAutoCompile;

	/** Whether or not simulations should start playing automatically when the emitter or system editor is opened, or when the data is changed in the editor. */
	UPROPERTY(config, EditAnywhere, Category = SimulationOptions)
	bool bAutoPlay;

	/** Whether or not the simulation should reset when a value on the emitter or system is changed. */
	UPROPERTY(config, EditAnywhere, Category = SimulationOptions)
	bool bResetSimulationOnChange;

	/** Whether or not to rerun the simulation to the current time when making modifications while paused. */
	UPROPERTY(config, EditAnywhere, Category = SimulationOptions)
	bool bResimulateOnChangeWhilePaused;

	/** Whether or not to reset all components that include the system currently being reset. */
	UPROPERTY(config, EditAnywhere, Category = SimulationOptions)
	bool bResetDependentSystemsWhenEditingEmitters;

	/** Whether or not to display advanced categories for the parameter panel. */
	UPROPERTY(config, EditAnywhere, Category = Niagara)
	bool bDisplayAdvancedParameterPanelCategories;

	/** If true, then the emitter and script editors will show an info message how many downstream asset are affected by a change. Gathering this information for large asset graphs can delay the opening of the asset editors a bit. */
	UPROPERTY(config, EditAnywhere, Category=Niagara)
	bool bDisplayAffectedAssetStats = true;

	/** The maximum amount of asset references that are searched before stopping. Setting this too high can lead to long load times when opening default assets (basically the same as disabling the breadth limit in the reference viewer). */
	UPROPERTY(config, EditAnywhere, Category=Niagara, meta=(EditCondition="bDisplayAffectedAssetStats", ClampMin=1))
	int32 AffectedAssetSearchLimit = 25;

	/** Speeds used for slowing down and speeding up the playback speeds */
	UPROPERTY(config, EditAnywhere, Category = Niagara)
	TArray<float> PlaybackSpeeds;

	UPROPERTY(config, EditAnywhere, Category = "Niagara Colors")
	FNiagaraActionColors ActionColors;

	/** This is built using PlaybackSpeeds, populated whenever it is accessed using GetPlaybackSpeeds() */
	mutable TOptional<TArray<float>> CachedPlaybackSpeeds;
	
	UPROPERTY(config)
	TMap<FName, FNiagaraNewAssetDialogConfig> NewAssetDialogConfigMap;

	UPROPERTY(config)
	TMap<FString, FString> HLSLKeywordReplacements;

	UPROPERTY()
	TArray<FNiagaraNamespaceMetadata> NamespaceMetadata;

	UPROPERTY()
	TArray<FNiagaraNamespaceMetadata> NamespaceModifierMetadata;

	UPROPERTY()
	FNiagaraNamespaceMetadata DefaultNamespaceMetadata;

	UPROPERTY()
	FNiagaraNamespaceMetadata DefaultNamespaceModifierMetadata;

	UPROPERTY(config, EditAnywhere, Category = Niagara)
	TArray<FNiagaraCurveTemplate> CurveTemplates;

	UPROPERTY(config)
	bool bShowGridInViewport;

	UPROPERTY(config)
	bool bShowInstructionsCount;
	
	UPROPERTY(config)
	bool bShowParticleCountsInViewport;

	UPROPERTY(config)
	bool bShowEmitterExecutionOrder;

	UPROPERTY(config)
	bool bShowGpuTickInformation;


	UPROPERTY(config)
	TArray<FNiagaraParameterPanelSectionStorage> SystemParameterPanelSectionData;

	FOnIsClassAllowed OnIsClassAllowedDelegate;
	FOnIsClassPathAllowed OnIsClassPathAllowedDelegate;

public:
	bool IsShowInstructionsCount() const;
	void SetShowInstructionsCount(bool bShowInstructionsCount);
	bool IsShowEmitterExecutionOrder() const;
	void SetShowEmitterExecutionOrder(bool bShowEmitterExecutionOrder);
	bool IsShowGpuTickInformation() const;
	void SetShowGpuTickInformation(bool bShowGpuTickInformation);
};
