// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "EdGraph/EdGraph.h"
#include "HAL/CriticalSection.h"
#include "IAudioParameterTransmitter.h"
#include "Interfaces/MetasoundOutputFormatInterfaces.h"
#include "Metasound.h"
#include "MetasoundAssetBase.h"
#include "MetasoundFrontend.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundOperatorSettings.h"
#include "MetasoundParameterTransmitter.h"
#include "MetasoundRouter.h"
#include "MetasoundVertex.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/MetaData.h"

#include <atomic>

#include "MetasoundSource.generated.h"


// Forward Declarations
class UMetaSoundSettings;
struct FMetaSoundQualitySettings;

namespace Audio
{
	using DeviceID = uint32;
} // namespace Audio

namespace Metasound
{
	struct FMetaSoundEngineAssetHelper;
	struct FMetasoundGeneratorInitParams;

	class FMetasoundGenerator;

	namespace Frontend
	{
		class IDataTypeRegistry;
	} // namespace Frontend

	namespace SourcePrivate
	{
		class FParameterRouter;
		using FCookedQualitySettings = FMetaSoundQualitySettings;
	} // namespace SourcePrivate

	namespace DynamicGraph
	{
		class FDynamicOperatorTransactor;
	} // namespace DynamicGraph
} // namespace Metasound


DECLARE_TS_MULTICAST_DELEGATE_TwoParams(FOnGeneratorInstanceCreated, uint64, TSharedPtr<Metasound::FMetasoundGenerator>);
DECLARE_TS_MULTICAST_DELEGATE_TwoParams(FOnGeneratorInstanceDestroyed, uint64, TSharedPtr<Metasound::FMetasoundGenerator>);

/**
 * This Metasound type can be played as an audio source.
 */
UCLASS(hidecategories = object, BlueprintType, config = Metasound, defaultconfig)
class METASOUNDENGINE_API UMetaSoundSource : public USoundWaveProcedural, public FMetasoundAssetBase, public IMetaSoundDocumentInterface
{
	GENERATED_BODY()

	friend struct Metasound::FMetaSoundEngineAssetHelper;
	friend class UMetaSoundSourceBuilder;

	// FRuntimeInput represents an input to a MetaSound which can be manipulated.
	struct FRuntimeInput
	{
		// Name of input vertex
		FName Name;
		// Data type name of input vertex.
		FName TypeName;
		// Access type of input vertex.
		EMetasoundFrontendVertexAccessType AccessType;
		// Default parameter of input vertex.
		FAudioParameter DefaultParameter;
		// True if the data type is transmittable. False otherwise.
		bool bIsTransmittable;
	};

	struct FRuntimeInputData
	{
		std::atomic<bool> bIsValid = false;
		Metasound::TSortedVertexNameMap<FRuntimeInput> InputMap;
	};

protected:
	UPROPERTY(EditAnywhere, Category = CustomView)
	FMetasoundFrontendDocument RootMetasoundDocument;

	UPROPERTY()
	TSet<FString> ReferencedAssetClassKeys;

	UPROPERTY()
	TSet<TObjectPtr<UObject>> ReferencedAssetClassObjects;

	UPROPERTY()
	TSet<FSoftObjectPath> ReferenceAssetClassCache;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UMetasoundEditorGraphBase> Graph;
#endif // WITH_EDITORONLY_DATA

public:
	UMetaSoundSource(const FObjectInitializer& ObjectInitializer);

	// The output audio format of the metasound source.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Metasound)
	EMetaSoundOutputAudioFormat OutputFormat;

#if WITH_EDITORONLY_DATA

	// The Quality this Metasound will use. These are defined in the MetaSounds project settings.
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, meta = (GetOptions="MetasoundEngine.MetaSoundQualityHelper.GetQualityList"), Category = "Metasound")
	FName QualitySetting;

	// This a editor only look up for the Quality Setting above. Preventing orphaning of the original name.
	UPROPERTY()
	FGuid QualitySettingGuid;

	// Override the BlockRate for this Sound (overrides Quality). NOTE: A Zero value will have no effect and use either the Quality setting (if set), or the defaults.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = Metasound, meta = (UIMin = 0, UIMax = 1000.0, DisplayAfter="OutputFormat", DisplayName = "Override Block Rate (in Hz)"))
	FPerPlatformFloat BlockRateOverride = 0.f;

	// Override the SampleRate for this Sound (overrides Quality). NOTE: A Zero value will have no effect and use either the Quality setting (if set), or the Device Rate
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = Metasound, meta = (UIMin = 0, UIMax = 96000, DisplayName = "Override Sample Rate (in Hz)"))
	FPerPlatformInt SampleRateOverride = 0;
	
#endif //WITH_EDITOR_DATA
	
	UPROPERTY(AssetRegistrySearchable)
	FGuid AssetClassID;

#if WITH_EDITORONLY_DATA
	UPROPERTY(AssetRegistrySearchable)
	FString RegistryInputTypes;

	UPROPERTY(AssetRegistrySearchable)
	FString RegistryOutputTypes;

	UPROPERTY(AssetRegistrySearchable)
	int32 RegistryVersionMajor = 0;

	UPROPERTY(AssetRegistrySearchable)
	int32 RegistryVersionMinor = 0;

	UPROPERTY(AssetRegistrySearchable)
	bool bIsPreset = false;

	// Sets Asset Registry Metadata associated with this MetaSoundSource
	virtual void SetRegistryAssetClassInfo(const Metasound::Frontend::FNodeClassInfo& InNodeInfo) override;

	// Returns document name (for editor purposes, and avoids making document public for edit
	// while allowing editor to reference directly)
	static FName GetDocumentPropertyName()
	{
		return GET_MEMBER_NAME_CHECKED(UMetaSoundSource, RootMetasoundDocument);
	}

	// Name to display in editors
	virtual FText GetDisplayName() const override;

	// Returns the graph associated with this Metasound. Graph is required to be referenced on
	// Metasound UObject for editor serialization purposes.
	// @return Editor graph associated with UMetaSoundSource.
	virtual UEdGraph* GetGraph() override;
	virtual const UEdGraph* GetGraph() const override;
	virtual UEdGraph& GetGraphChecked() override;
	virtual const UEdGraph& GetGraphChecked() const override;

	// Sets the graph associated with this Metasound. Graph is required to be referenced on
	// Metasound UObject for editor serialization purposes.
	// @param Editor graph associated with UMetaSoundSource.
	virtual void SetGraph(UEdGraph* InGraph) override
	{
		Graph = CastChecked<UMetasoundEditorGraphBase>(InGraph);
	}
#endif // #if WITH_EDITORONLY_DATA


#if WITH_EDITOR
	virtual void PostEditUndo() override;

	virtual bool GetRedrawThumbnail() const override
	{
		return false;
	}

	virtual void SetRedrawThumbnail(bool bInRedraw) override
	{
	}

	virtual bool CanVisualizeAsset() const override
	{
		return false;
	}

	virtual void PostDuplicate(EDuplicateMode::Type InDuplicateMode) override;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& InEvent) override;

	virtual bool CanEditChange(const FProperty* InProperty) const override;

private:
	void PostEditChangeOutputFormat();
	void PostEditChangeQualitySettings();
public:

#endif // WITH_EDITOR

	virtual const TSet<FString>& GetReferencedAssetClassKeys() const override
	{
		return ReferencedAssetClassKeys;
	}
	virtual TArray<FMetasoundAssetBase*> GetReferencedAssets() override;
	virtual const TSet<FSoftObjectPath>& GetAsyncReferencedAssetClassPaths() const override;
	virtual void OnAsyncReferencedAssetsLoaded(const TArray<FMetasoundAssetBase*>& InAsyncReferences) override;

	virtual void BeginDestroy() override;
	virtual void PreSave(FObjectPreSaveContext InSaveContext) override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;

	void PostLoadQualitySettings();

	virtual bool ConformObjectDataToInterfaces() override;

	virtual FTopLevelAssetPath GetAssetPathChecked() const override;

	UObject* GetOwningAsset() override
	{
		return this;
	}

	const UObject* GetOwningAsset() const override
	{
		return this;
	}
	virtual void InitParameters(TArray<FAudioParameter>& ParametersToInit, FName InFeatureName) override;

	virtual void InitResources() override;
	virtual void RegisterGraphWithFrontend(Metasound::Frontend::FMetaSoundAssetRegistrationOptions InRegistrationOptions = Metasound::Frontend::FMetaSoundAssetRegistrationOptions()) override;

	virtual bool IsPlayable() const override;
	virtual float GetDuration() const override;
	virtual bool ImplementsParameterInterface(Audio::FParameterInterfacePtr InInterface) const override;
	virtual ISoundGeneratorPtr CreateSoundGenerator(const FSoundGeneratorInitParams& InParams, TArray<FAudioParameter>&& InDefaultParameters) override;
	virtual void OnEndGenerate(ISoundGeneratorPtr Generator) override;
	virtual TSharedPtr<Audio::IParameterTransmitter> CreateParameterTransmitter(Audio::FParameterTransmitterInitParams&& InParams) const override;
	virtual bool IsParameterValid(const FAudioParameter& InParameter) const override;
	virtual bool IsLooping() const override;
	virtual bool IsOneShot() const override;
	virtual bool EnableSubmixSendsOnPreview() const override { return true; }

	TWeakPtr<Metasound::FMetasoundGenerator> GetGeneratorForAudioComponent(uint64 ComponentId) const;
	bool IsDynamic() const;

	FOnGeneratorInstanceCreated OnGeneratorInstanceCreated;
	FOnGeneratorInstanceDestroyed OnGeneratorInstanceDestroyed;
	Metasound::FOperatorSettings GetOperatorSettings(Metasound::FSampleRate InDeviceSampleRate) const;

	virtual const FMetasoundFrontendDocument& GetConstDocument() const override;

protected:
	Metasound::Frontend::FDocumentAccessPtr GetDocumentAccessPtr() override;
	Metasound::Frontend::FConstDocumentAccessPtr GetDocumentConstAccessPtr() const override;

	virtual const UClass& GetBaseMetaSoundUClass() const final override;

	/** Gets all the default parameters for this Asset.  */
	virtual bool GetAllDefaultParameters(TArray<FAudioParameter>& OutParameters) const override;

#if WITH_EDITOR
	virtual void SetReferencedAssetClasses(TSet<Metasound::Frontend::IMetaSoundAssetManager::FAssetInfo>&& InAssetClasses) override;
#endif // #if WITH_EDITOR

private:
	virtual FMetasoundFrontendDocument& GetDocument() override
	{
		return RootMetasoundDocument;
	}

	virtual bool IsBuilderActive() const override;
	virtual void OnBeginActiveBuilder() override;
	virtual void OnFinishActiveBuilder() override;

	void InitParametersInternal(const Metasound::TSortedVertexNameMap<FRuntimeInput>& InputMap, TArray<FAudioParameter>& ParametersToInit, FName InFeatureName) const;
	bool IsParameterValidInternal(const FAudioParameter& InParameter, const FName& InTypeName, Metasound::Frontend::IDataTypeRegistry& InDataTypeRegistry) const;

	static Metasound::SourcePrivate::FParameterRouter& GetParameterRouter();

public:
	Metasound::FMetasoundEnvironment CreateEnvironment(const FSoundGeneratorInitParams& InParams) const;
	const TArray<Metasound::FVertexName>& GetOutputAudioChannelOrder() const;

private:
	TSharedPtr<const Metasound::IGraph> TryGetMetaSoundPresetBaseGraph() const;
	void MergePresetOverridesAndSuppliedDefaults(const TArray<FAudioParameter>& InSuppliedDefaults, TArray<FAudioParameter>& OutMerged);
	
	Metasound::FMetasoundEnvironment CreateEnvironment() const;
	Metasound::FMetasoundEnvironment CreateEnvironment(const Audio::FParameterTransmitterInitParams& InParams) const;

	mutable FCriticalSection GeneratorMapCriticalSection;
	TSortedMap<uint64, TWeakPtr<Metasound::FMetasoundGenerator>> Generators;
	void TrackGenerator(uint64 Id, TSharedPtr<Metasound::FMetasoundGenerator> Generator);
	void ForgetGenerator(ISoundGeneratorPtr Generator);

	static FRuntimeInput CreateRuntimeInput(const Metasound::Frontend::IDataTypeRegistry& Registry, const FMetasoundFrontendClassInput& Input, bool bCreateUObjectProxies);
	Metasound::TSortedVertexNameMap<FRuntimeInput> CreateRuntimeInputMap(bool bCreateUObjectProxies) const;
	void CacheRuntimeInputData();
	void InvalidateCachedRuntimeInputData();

	FRuntimeInputData RuntimeInputData;

	/** Enable/disable dynamic generator.
	 *
	 * Once a dynamic generator is enabled, all changes to the MetaSound should be applied to the
	 * FDynamicOperatorTransactor in order to keep parity between the document and active graph.
	 *
	 * Note: Disabling the dynamic generator will sever the communication between any active generators
	 * even if the dynamic generator is re-enabled during the lifetime of the active generators
	 */
	TSharedPtr<Metasound::DynamicGraph::FDynamicOperatorTransactor> SetDynamicGeneratorEnabled(const FTopLevelAssetPath& InAssetPath, bool bInIsEnabled);

	/** Get dynamic transactor
	 *
	 * If dynamic generators are enabled, this will return a valid pointer to a dynamic transactor.
	 * Changes to this transactor will be forwarded to any active Dynamic MetaSound Generators.
	 */
	TSharedPtr<Metasound::DynamicGraph::FDynamicOperatorTransactor> GetDynamicGeneratorTransactor() const;

	TSharedPtr<Metasound::DynamicGraph::FDynamicOperatorTransactor> DynamicTransactor;

	// Cache the AudioDevice Samplerate. (so that if we have to regenerate operator settings without the device rate we can use this).
	mutable Metasound::FSampleRate CachedAudioDeviceSampleRate = 0;
	
	bool bIsBuilderActive = false;

	// Preset graph inflation is a performance optimization intended for use with the MetaSoundOperatorPool. If multiple presets 
	// utilize the same base MetaSound, they may be able to share their operators in the operator pool. This makes for a more
	// efficient use of the operator pool.
	bool bIsPresetGraphInflationSupported = false;

	// Quality settings. 
	bool GetQualitySettings(const FName InPlatformName, Metasound::SourcePrivate::FCookedQualitySettings& OutQualitySettings) const;
	void ResolveQualitySettings(const UMetaSoundSettings* Settings);	
	void SerializeCookedQualitySettings(const FName PlatformName, FArchive& Ar);
	TPimplPtr<Metasound::SourcePrivate::FCookedQualitySettings> CookedQualitySettings;
};
