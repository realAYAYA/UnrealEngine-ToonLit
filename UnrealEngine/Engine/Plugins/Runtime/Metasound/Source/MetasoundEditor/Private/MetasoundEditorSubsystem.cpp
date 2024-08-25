// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundEditorSubsystem.h"

#include "IAssetTools.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MetasoundBuilderSubsystem.h"
#include "MetasoundEditorGraph.h"
#include "MetasoundEditorGraphBuilder.h"
#include "MetasoundEditorGraphSchema.h"
#include "MetasoundEditorSettings.h"
#include "MetasoundFactory.h"
#include "MetasoundUObjectRegistry.h"
#include "Sound/SoundSourceBusSend.h"
#include "Sound/SoundSubmixSend.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetasoundEditorSubsystem)

#define LOCTEXT_NAMESPACE "MetaSoundEditor"


TScriptInterface<IMetaSoundDocumentInterface> UMetaSoundEditorSubsystem::BuildToAsset(
	UMetaSoundBuilderBase* InBuilder,
	const FString& Author,
	const FString& AssetName,
	const FString& PackagePath,
	EMetaSoundBuilderResult& OutResult,
	const USoundWave* TemplateSoundWave
)
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	OutResult = EMetaSoundBuilderResult::Failed;

	if (InBuilder)
	{
		// AddToRoot to avoid builder getting gc'ed during CreateAsset call below, as the builder 
		// may be unreferenced by other UObjects and it must be persistent to finish initializing.
		const bool bWasRooted = InBuilder->IsRooted();
		if (!bWasRooted)
		{
			InBuilder->AddToRoot();
		}

		constexpr UFactory* Factory = nullptr;
		// Not about to follow this lack of const correctness down a multidecade in the works rabbit hole.
		UClass& BuilderUClass = const_cast<UClass&>(InBuilder->GetBuilderUClass());
		if (UObject* NewMetaSound = IAssetTools::Get().CreateAsset(AssetName, PackagePath, &BuilderUClass, Factory))
		{
			InBuilder->InitNodeLocations();
			InBuilder->SetAuthor(Author);

			// Initialize and Build
			{
				constexpr UObject* Parent = nullptr;
				constexpr bool bForceUniqueClassName = true;
				constexpr bool bAddToRegistry = true;
				const FMetaSoundBuilderOptions BuilderOptions { FName(*AssetName), bForceUniqueClassName, bAddToRegistry, NewMetaSound };
				InBuilder->Build(Parent, BuilderOptions);
			}

			// Apply template SoundWave settings
			{
				const bool bIsSource = &BuilderUClass == UMetaSoundSource::StaticClass();
				if (InBuilder->IsPreset())
				{
					FMetasoundAssetBase* PresetAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(NewMetaSound);
					check(PresetAsset);
					PresetAsset->ConformObjectDataToInterfaces();

					// Only use referenced UObject's SoundWave settings for sources if not overridden 
					if (TemplateSoundWave == nullptr && bIsSource)
					{
						if (const UObject* ReferencedObject = InBuilder->GetReferencedPresetAsset())
						{
							TemplateSoundWave = CastChecked<USoundWave>(ReferencedObject);
						}
					}
				}

				// Template SoundWave settings only apply to sources
				if (TemplateSoundWave != nullptr && bIsSource)
				{
					SetSoundWaveSettingsFromTemplate(*CastChecked<USoundWave>(NewMetaSound), *TemplateSoundWave);
				}
			}

			InitEdGraph(*NewMetaSound);

			if (!bWasRooted)
			{
				InBuilder->RemoveFromRoot();
			}

			OutResult = EMetaSoundBuilderResult::Succeeded;
			return NewMetaSound;
		}

		if (!bWasRooted)
		{
			InBuilder->RemoveFromRoot();
		}
	}

	return nullptr;
}

UMetaSoundEditorSubsystem& UMetaSoundEditorSubsystem::GetChecked()
{
	checkf(GEditor, TEXT("Cannot access UMetaSoundEditorSubsystem without editor loaded"));
	UMetaSoundEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMetaSoundEditorSubsystem>();
	checkf(EditorSubsystem, TEXT("Failed to find initialized 'UMetaSoundEditorSubsystem"));
	return *EditorSubsystem;
}

const UMetaSoundEditorSubsystem& UMetaSoundEditorSubsystem::GetConstChecked()
{
	checkf(GEditor, TEXT("Cannot access UMetaSoundEditorSubsystem without editor loaded"));
	UMetaSoundEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMetaSoundEditorSubsystem>();
	checkf(EditorSubsystem, TEXT("Failed to find initialized 'UMetaSoundEditorSubsystem"));
	return *EditorSubsystem;
}

const FString UMetaSoundEditorSubsystem::GetDefaultAuthor()
{
	FString Author = UKismetSystemLibrary::GetPlatformUserName();
	if (const UMetasoundEditorSettings* EditorSettings = GetDefault<UMetasoundEditorSettings>())
	{
		if (!EditorSettings->DefaultAuthor.IsEmpty())
		{
			Author = EditorSettings->DefaultAuthor;
		}
	}
	return Author;
}

const TArray<TSharedRef<FExtender>>& UMetaSoundEditorSubsystem::GetToolbarExtenders() const
{
	return EditorToolbarExtenders;
}

void UMetaSoundEditorSubsystem::InitAsset(UObject& InNewMetaSound, UObject* InReferencedMetaSound)
{
	using namespace Metasound;
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	TScriptInterface<IMetaSoundDocumentInterface> DocInterface = &InNewMetaSound;
	FMetaSoundFrontendDocumentBuilder Builder(DocInterface);
	Builder.InitDocument();
#if WITH_EDITORONLY_DATA
	Builder.InitNodeLocations();
#endif // WITH_EDITORONLY_DATA

	const FString& Author = GetDefaultAuthor();
	Builder.SetAuthor(Author);

	// Initialize asset as a preset
	if (InReferencedMetaSound)
	{
		// Ensure the referenced MetaSound is registered already
		RegisterGraphWithFrontend(*InReferencedMetaSound);

		// Initialize preset with referenced Metasound 
		TScriptInterface<IMetaSoundDocumentInterface> ReferencedDocInterface = InReferencedMetaSound;
		Builder.ConvertToPreset(ReferencedDocInterface->GetConstDocument());

		// Update asset object data from interfaces 
		FMetasoundAssetBase* PresetAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InNewMetaSound);
		check(PresetAsset);
		PresetAsset->ConformObjectDataToInterfaces();

		// Copy sound wave settings to preset for sources
		if (&ReferencedDocInterface->GetBaseMetaSoundUClass() == UMetaSoundSource::StaticClass())
		{
			SetSoundWaveSettingsFromTemplate(*CastChecked<USoundWave>(&InNewMetaSound), *CastChecked<USoundWave>(InReferencedMetaSound));
		}
	}

	// Initial graph generation is not something to be managed by the transaction
	// stack, so don't track dirty state until after initial setup if necessary.
	InitEdGraph(InNewMetaSound);
}

void UMetaSoundEditorSubsystem::InitEdGraph(UObject& InMetaSound)
{
	using namespace Metasound;
	using namespace Metasound::Editor;

	FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InMetaSound);
	checkf(MetaSoundAsset, TEXT("EdGraph can only be initialized on registered MetaSoundAsset type"));

	UMetasoundEditorGraph* Graph = Cast<UMetasoundEditorGraph>(MetaSoundAsset->GetGraph());
	if (!Graph)
	{
		Graph = NewObject<UMetasoundEditorGraph>(&InMetaSound, FName(), RF_Transactional);
		Graph->Schema = UMetasoundEditorGraphSchema::StaticClass();
		MetaSoundAsset->SetGraph(Graph);

		// Has to be done inline to have valid graph initially when opening editor for the first
		// time (as opposed to being applied on tick when the document's modify context has updates)
		FGraphBuilder::SynchronizeGraph(InMetaSound);
	}
}

void UMetaSoundEditorSubsystem::RegisterGraphWithFrontend(UObject& InMetaSound, bool bInForceViewSynchronization)
{
	Metasound::Editor::FGraphBuilder::RegisterGraphWithFrontend(InMetaSound, bInForceViewSynchronization);
}

void UMetaSoundEditorSubsystem::RegisterToolbarExtender(TSharedRef<FExtender> InExtender)
{
	EditorToolbarExtenders.AddUnique(InExtender);
}

bool UMetaSoundEditorSubsystem::UnregisterToolbarExtender(TSharedRef<FExtender> InExtender)
{
	const int32 NumRemoved = EditorToolbarExtenders.RemoveAllSwap([&InExtender](const TSharedRef<FExtender>& Extender) { return Extender == InExtender; });
	return NumRemoved > 0;
}

void UMetaSoundEditorSubsystem::SetNodeLocation(
	UMetaSoundBuilderBase* InBuilder,
	const FMetaSoundNodeHandle& InNode,
	const FVector2D& InLocation,
	EMetaSoundBuilderResult& OutResult)
{
	if (InBuilder)
	{
		InBuilder->SetNodeLocation(InNode, InLocation, OutResult);
	}
	else
	{
		OutResult = EMetaSoundBuilderResult::Failed;
	}
}

void UMetaSoundEditorSubsystem::SetSoundWaveSettingsFromTemplate(USoundWave& NewMetaSoundWave, const USoundWave& TemplateSoundWave) const
{
	// Sound 
	NewMetaSoundWave.Volume = TemplateSoundWave.Volume;
	NewMetaSoundWave.Pitch = TemplateSoundWave.Pitch;
	NewMetaSoundWave.SoundClassObject = TemplateSoundWave.SoundClassObject;

	// Attenuation 
	NewMetaSoundWave.AttenuationSettings = TemplateSoundWave.AttenuationSettings;
	NewMetaSoundWave.bDebug = TemplateSoundWave.bDebug;

	// Effects 
	NewMetaSoundWave.bEnableBusSends = TemplateSoundWave.bEnableBusSends;
	NewMetaSoundWave.SourceEffectChain = TemplateSoundWave.SourceEffectChain;
	NewMetaSoundWave.BusSends = TemplateSoundWave.BusSends;
	NewMetaSoundWave.PreEffectBusSends = TemplateSoundWave.PreEffectBusSends; 

	NewMetaSoundWave.bEnableBaseSubmix = TemplateSoundWave.bEnableBaseSubmix;
	NewMetaSoundWave.SoundSubmixObject = TemplateSoundWave.SoundSubmixObject;
	NewMetaSoundWave.bEnableSubmixSends = TemplateSoundWave.bEnableSubmixSends;
	NewMetaSoundWave.SoundSubmixSends = TemplateSoundWave.SoundSubmixSends;

	// Modulation 
	NewMetaSoundWave.ModulationSettings = TemplateSoundWave.ModulationSettings;
	
	// Voice Management 
	NewMetaSoundWave.VirtualizationMode = TemplateSoundWave.VirtualizationMode;
	NewMetaSoundWave.bOverrideConcurrency = TemplateSoundWave.bOverrideConcurrency;
	NewMetaSoundWave.ConcurrencySet = TemplateSoundWave.ConcurrencySet;
	NewMetaSoundWave.ConcurrencyOverrides = TemplateSoundWave.ConcurrencyOverrides;

	NewMetaSoundWave.bBypassVolumeScaleForPriority = TemplateSoundWave.bBypassVolumeScaleForPriority;
	NewMetaSoundWave.Priority = TemplateSoundWave.Priority;
}

#undef LOCTEXT_NAMESPACE // "MetaSoundEditor"
