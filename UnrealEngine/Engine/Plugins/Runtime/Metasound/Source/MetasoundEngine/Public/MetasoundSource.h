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
#include "Sound/SoundWaveProcedural.h"
#include "UObject/MetaData.h"

#include "MetasoundSource.generated.h"


// Forward Declarations
namespace Metasound
{
	struct FMetaSoundEngineAssetHelper;
	class FMetasoundGenerator;
	namespace SourcePrivate
	{
		class FParameterRouter;
	}

	namespace DynamicGraph
	{
		class FDynamicOperatorTransactor;
	}
} // namespace Metasound

namespace Audio
{
	using DeviceID = uint32;
}

DECLARE_TS_MULTICAST_DELEGATE_TwoParams(FOnGeneratorInstanceCreated, uint64, TSharedPtr<Metasound::FMetasoundGenerator>);
DECLARE_TS_MULTICAST_DELEGATE_TwoParams(FOnGeneratorInstanceDestroyed, uint64, TSharedPtr<Metasound::FMetasoundGenerator>);

/**
 * This Metasound type can be played as an audio source.
 */
UCLASS(hidecategories = object, BlueprintType)
class METASOUNDENGINE_API UMetaSoundSource : public USoundWaveProcedural, public FMetasoundAssetBase, public IMetaSoundDocumentInterface
{
	GENERATED_BODY()

	friend struct Metasound::FMetaSoundEngineAssetHelper;
	friend class UMetaSoundSourceBuilder;

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

private:
	void PostEditChangeOutputFormat();
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

	// Returns Asset Metadata associated with this MetaSoundSource
	virtual Metasound::Frontend::FNodeClassInfo GetAssetClassInfo() const override;


	virtual bool ConformObjectDataToInterfaces() override;

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

	virtual bool IsPlayable() const override;
	virtual bool SupportsSubtitles() const override;
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
	FOnGeneratorInstanceCreated OnGeneratorInstanceCreated;
	FOnGeneratorInstanceDestroyed OnGeneratorInstanceDestroyed;

protected:
	Metasound::Frontend::FDocumentAccessPtr GetDocumentAccessPtr() override
	{
		using namespace Metasound::Frontend;
		// Return document using FAccessPoint to inform the TAccessPtr when the 
		// object is no longer valid.
		return MakeAccessPtr<FDocumentAccessPtr>(RootMetasoundDocument.AccessPoint, RootMetasoundDocument);
	}

	Metasound::Frontend::FConstDocumentAccessPtr GetDocumentConstAccessPtr() const override
	{
		using namespace Metasound::Frontend;
		// Return document using FAccessPoint to inform the TAccessPtr when the 
		// object is no longer valid.
		return MakeAccessPtr<FConstDocumentAccessPtr>(RootMetasoundDocument.AccessPoint, RootMetasoundDocument);
	}

	virtual const UClass& GetBaseMetaSoundUClass() const final override;
	virtual const FMetasoundFrontendDocument& GetDocument() const override;

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


	bool IsParameterValid(const FAudioParameter& InParameter, const FMetasoundFrontendVertex* InVertex) const;

	static Metasound::SourcePrivate::FParameterRouter& GetParameterRouter();

	Metasound::FOperatorSettings GetOperatorSettings(Metasound::FSampleRate InSampleRate) const;
	Metasound::FMetasoundEnvironment CreateEnvironment() const;
	Metasound::FMetasoundEnvironment CreateEnvironment(const FSoundGeneratorInitParams& InParams) const;
	Metasound::FMetasoundEnvironment CreateEnvironment(const Audio::FParameterTransmitterInitParams& InParams) const;
	const TArray<Metasound::FVertexName>& GetOutputAudioChannelOrder() const;

	mutable FCriticalSection GeneratorMapCriticalSection;
	TSortedMap<uint64, TWeakPtr<Metasound::FMetasoundGenerator>> Generators;
	void TrackGenerator(uint64 Id, TSharedPtr<Metasound::FMetasoundGenerator> Generator);
	void ForgetGenerator(ISoundGeneratorPtr Generator);

	/** Enable/disable dynamic generator.
	 *
	 * Once a dynamic generator is enabled, all changes to the MetaSound should be applied to the
	 * FDynamicOperatorTransactor in order to keep parity between the document and active graph.
	 *
	 * Note: Disabling the dynamic generator will sever the communication between any active generators
	 * even if the dynamic generator is re-enabled during the lifetime of the active generators
	 */
	TSharedPtr<Metasound::DynamicGraph::FDynamicOperatorTransactor> SetDynamicGeneratorEnabled(bool bInIsEnabled);

	/** Get dynamic transactor
	 *
	 * If dynamic generators are enabled, this will return a valid pointer to a dynamic transactor.
	 * Changes to this transactor will be forwarded to any active Dynamic MetaSound Generators.
	 */
	TSharedPtr<Metasound::DynamicGraph::FDynamicOperatorTransactor> GetDynamicGeneratorTransactor() const;

	TSharedPtr<Metasound::DynamicGraph::FDynamicOperatorTransactor> DynamicTransactor;
};
