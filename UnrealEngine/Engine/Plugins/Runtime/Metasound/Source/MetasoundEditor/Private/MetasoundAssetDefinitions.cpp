// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundAssetDefinitions.h"

#include "AssetRegistry/AssetRegistryModule.h"

#include "Components/AudioComponent.h"
#include "ContentBrowserMenuContexts.h"
#include "ContentBrowserModule.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "IAssetTools.h"
#include "IContentBrowserSingleton.h"
#include "Metasound.h"
#include "MetasoundAssetBase.h"
#include "MetasoundAssetManager.h"
#include "MetasoundBuilderSubsystem.h"
#include "MetasoundSource.h"
#include "MetasoundEditor.h"
#include "MetasoundEditorGraphBuilder.h"
#include "MetasoundEditorModule.h"
#include "MetasoundEditorSettings.h"
#include "MetasoundEditorSubsystem.h"
#include "MetasoundFactory.h"
#include "MetasoundUObjectRegistry.h"
#include "Misc/AssetRegistryInterface.h"
#include "Modules/ModuleManager.h"
#include "ObjectEditorUtils.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "ToolMenus.h"
#include "ToolMenuSection.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SOverlay.h"


#define LOCTEXT_NAMESPACE "MetaSoundEditor"


static bool GetIsPreset(const FSoftObjectPath& InSourcePath)
{
	using namespace Metasound;
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	bool bIsPreset = false;
	// If object in memory, try to resolve but not load
	if (UObject* Object = InSourcePath.ResolveObject())
	{
		const FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(Object);
		if (MetaSoundAsset)
		{
			bIsPreset = MetaSoundAsset->GetDocumentChecked().RootGraph.PresetOptions.bIsPreset;
		}
	}
	// Otherwise, try to pull from asset registry, but avoid load as this call
	// would then be slow and is called from the ContentBrowser many times
	else
	{
		FAssetData IconAssetData;
		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry");
		UE::AssetRegistry::EExists Exists = AssetRegistryModule.TryGetAssetByObjectPath(InSourcePath, IconAssetData);
		if (Exists == UE::AssetRegistry::EExists::Exists)
		{
			IconAssetData.GetTagValue(AssetTags::IsPreset, bIsPreset);
		}
	}

	return bIsPreset;
}

static const FSlateBrush* GetClassBrush(const FAssetData& InAssetData, FName InClassName, bool bIsThumbnail = false)
{
	const bool bIsPreset = GetIsPreset(InAssetData.ToSoftObjectPath());
	FString BrushName = FString::Printf(TEXT("MetasoundEditor.%s"), *InClassName.ToString());
	if (bIsPreset)
	{
		BrushName += TEXT(".Preset");
	}
	BrushName += bIsThumbnail ? TEXT(".Thumbnail") : TEXT(".Icon");

	return &Metasound::Editor::Style::GetSlateBrushSafe(FName(*BrushName));
}


FLinearColor UAssetDefinition_MetaSoundPatch::GetAssetColor() const
{
	if (const ISlateStyle* MetasoundStyle = FSlateStyleRegistry::FindSlateStyle("MetaSoundStyle"))
	{
		return MetasoundStyle->GetColor("MetaSoundPatch.Color").ToFColorSRGB();
	}

	return FColor::White;
}

TSoftClassPtr<UObject> UAssetDefinition_MetaSoundPatch::GetAssetClass() const
{
	return UMetaSoundPatch::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_MetaSoundPatch::GetAssetCategories() const
{
	static const auto Pinned_Categories = { EAssetCategoryPaths::Audio };
	static const auto Categories = { EAssetCategoryPaths::Audio / LOCTEXT("AssetSoundMetaSoundsSubMenu", "MetaSounds") };

	if (GetDefault<UMetasoundEditorSettings>()->bPinMetaSoundPatchInAssetMenu)
	{
		return Pinned_Categories;
	}

	return Categories;
}

EAssetCommandResult UAssetDefinition_MetaSoundPatch::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UMetaSoundPatch* Metasound : OpenArgs.LoadObjects<UMetaSoundPatch>())
	{
		TSharedRef<Metasound::Editor::FEditor> NewEditor = MakeShared<Metasound::Editor::FEditor>();
		NewEditor->InitMetasoundEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Metasound);
	}
	return EAssetCommandResult::Handled;
}

const FSlateBrush* UAssetDefinition_MetaSoundPatch::GetThumbnailBrush(const FAssetData& InAssetData, const FName InClassName) const
{
	constexpr bool bIsThumbnail = true;
	return GetClassBrush(InAssetData, InClassName, bIsThumbnail);
}

const FSlateBrush* UAssetDefinition_MetaSoundPatch::GetIconBrush(const FAssetData& InAssetData, const FName InClassName) const
{
	return GetClassBrush(InAssetData, InClassName);
}

FLinearColor UAssetDefinition_MetaSoundSource::GetAssetColor() const
{
 	if (const ISlateStyle* MetasoundStyle = FSlateStyleRegistry::FindSlateStyle("MetaSoundStyle"))
 	{
 		return MetasoundStyle->GetColor("MetaSoundSource.Color").ToFColorSRGB();
 	}
 
 	return FColor::White;
}

TSoftClassPtr<UObject> UAssetDefinition_MetaSoundSource::GetAssetClass() const
{
	return UMetaSoundSource::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_MetaSoundSource::GetAssetCategories() const
{
	static const auto Pinned_Categories = { EAssetCategoryPaths::Audio };
	static const auto Categories = { EAssetCategoryPaths::Audio / LOCTEXT("AssetSoundMetaSoundSourceSubMenu", "MetaSounds") };

	if (GetDefault<UMetasoundEditorSettings>()->bPinMetaSoundSourceInAssetMenu)
	{
		return Pinned_Categories;
	}

	return Categories;
}

EAssetCommandResult UAssetDefinition_MetaSoundSource::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UMetaSoundSource* Metasound : OpenArgs.LoadObjects<UMetaSoundSource>())
	{
		TSharedRef<Metasound::Editor::FEditor> NewEditor = MakeShared<Metasound::Editor::FEditor>();
		NewEditor->InitMetasoundEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Metasound);
	}

	return EAssetCommandResult::Handled;
}

const FSlateBrush* UAssetDefinition_MetaSoundSource::GetThumbnailBrush(const FAssetData& InAssetData, const FName InClassName) const
{
	constexpr bool bIsThumbnail = true;
	return GetClassBrush(InAssetData, InClassName, bIsThumbnail);
}

const FSlateBrush* UAssetDefinition_MetaSoundSource::GetIconBrush(const FAssetData& InAssetData, const FName InClassName) const
{
	return GetClassBrush(InAssetData, InClassName);
}

void UAssetDefinition_MetaSoundSource::ExecutePlaySound(const FToolMenuContext& InContext)
{
	if (UMetaSoundSource* MetaSoundSource = UContentBrowserAssetContextMenuContext::LoadSingleSelectedAsset<UMetaSoundSource>(InContext))
	{
		// If editor is open, call into it to play to start all visualization requirements therein
		// specific to auditioning MetaSounds (ex. priming audio bus used for volume metering, playtime
		// widget, etc.)
		TSharedPtr<Metasound::Editor::FEditor> Editor = Metasound::Editor::FGraphBuilder::GetEditorForMetasound(*MetaSoundSource);
		if (Editor.IsValid())
		{
			Editor->Play();
			return;
		}

		Metasound::Editor::FGraphBuilder::FGraphBuilder::RegisterGraphWithFrontend(*MetaSoundSource);
		UAssetDefinition_SoundBase::ExecutePlaySound(InContext);
	}
}

void UAssetDefinition_MetaSoundSource::ExecuteStopSound(const FToolMenuContext& InContext)
{
	if (UMetaSoundSource* MetaSoundSource = UContentBrowserAssetContextMenuContext::LoadSingleSelectedAsset<UMetaSoundSource>(InContext))
	{
		TSharedPtr<Metasound::Editor::FEditor> Editor = Metasound::Editor::FGraphBuilder::GetEditorForMetasound(*MetaSoundSource);
		if (Editor.IsValid())
		{
			Editor->Stop();
			return;
		}

		UAssetDefinition_SoundBase::ExecuteStopSound(InContext);
	}
}

bool UAssetDefinition_MetaSoundSource::CanExecutePlayCommand(const FToolMenuContext& InContext)
{
	return UAssetDefinition_SoundBase::CanExecutePlayCommand(InContext);
}

ECheckBoxState UAssetDefinition_MetaSoundSource::IsActionCheckedMute(const FToolMenuContext& InContext)
{
	return UAssetDefinition_SoundBase::IsActionCheckedMute(InContext);
}

ECheckBoxState UAssetDefinition_MetaSoundSource::IsActionCheckedSolo(const FToolMenuContext& InContext)
{
	return UAssetDefinition_SoundBase::IsActionCheckedSolo(InContext);
}

void UAssetDefinition_MetaSoundSource::ExecuteMuteSound(const FToolMenuContext& InContext)
{
	UAssetDefinition_SoundBase::ExecuteMuteSound(InContext);
}

void UAssetDefinition_MetaSoundSource::ExecuteSoloSound(const FToolMenuContext& InContext)
{
	UAssetDefinition_SoundBase::ExecuteSoloSound(InContext);
}

bool UAssetDefinition_MetaSoundSource::CanExecuteMuteCommand(const FToolMenuContext& InContext)
{
	return UAssetDefinition_SoundBase::CanExecuteMuteCommand(InContext);
}

bool UAssetDefinition_MetaSoundSource::CanExecuteSoloCommand(const FToolMenuContext& InContext)
{
	return UAssetDefinition_SoundBase::CanExecuteSoloCommand(InContext);
}

TSharedPtr<SWidget> UAssetDefinition_MetaSoundSource::GetThumbnailOverlay(const FAssetData& InAssetData) const
{
	auto OnClickedLambdaOverride = [InAssetData]() -> FReply
	{
		TSharedPtr<Metasound::Editor::FEditor> Editor = Metasound::Editor::FGraphBuilder::GetEditorForMetasound(*InAssetData.GetAsset());
		if (UE::AudioEditor::IsSoundPlaying(InAssetData))
		{
			if (Editor.IsValid())
			{
				Editor->Stop();
			}
			else
			{
				UE::AudioEditor::StopSound();
			}
		}
		else
		{
			if (Editor.IsValid())
			{
				Editor->Play();
			}
			else
			{
				// Load and play sound
				UE::AudioEditor::PlaySound(Cast<USoundBase>(InAssetData.GetAsset()));
			}
		}
		return FReply::Handled();
	};
	return UAssetDefinition_SoundBase::GetSoundBaseThumbnailOverlay(InAssetData, MoveTemp(OnClickedLambdaOverride));
}

EAssetCommandResult UAssetDefinition_MetaSoundSource::ActivateAssets(const FAssetActivateArgs& ActivateArgs) const
{
	if (ActivateArgs.ActivationMethod == EAssetActivationMethod::Previewed)
	{
		if (UMetaSoundSource* MetaSoundSource = ActivateArgs.LoadFirstValid<UMetaSoundSource>())
		{
			TSharedPtr<Metasound::Editor::FEditor> Editor = Metasound::Editor::FGraphBuilder::GetEditorForMetasound(*MetaSoundSource);
			UAudioComponent* PreviewComp = GEditor->GetPreviewAudioComponent();
	
			// If the editor is open, we need to stop or start the editor so it can light up while previewing it in the CB
			if (Editor.IsValid())
			{
				if (PreviewComp && PreviewComp->IsPlaying())
				{
					if (!MetaSoundSource || PreviewComp->Sound == MetaSoundSource)
					{
						Editor->Stop();
					}
				}
				else
				{
					Editor->Play();
				}

				return EAssetCommandResult::Handled;
			}
			else
			{
				return UAssetDefinition_SoundBase::ActivateSoundBase(ActivateArgs);
			}
		}
	}
	return EAssetCommandResult::Unhandled;
}

namespace MenuExtension_MetaSoundSourceTemplate
{
	template <typename TClass>
	void ExecuteCreateMetaSoundPreset(const FToolMenuContext& MenuContext)
	{
		if (const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext))
		{
			for (TClass* ReferencedMetaSound : Context->LoadSelectedObjects<TClass>())
			{
				FString PackagePath;
				FString AssetName;

				IAssetTools::Get().CreateUniqueAssetName(ReferencedMetaSound->GetOutermost()->GetName(), TEXT("_Preset"), PackagePath, AssetName);

				EMetaSoundBuilderResult BuilderResult;
				UMetaSoundBuilderBase& Builder = UMetaSoundBuilderSubsystem::GetChecked().CreatePresetBuilder(FName(AssetName), ReferencedMetaSound, BuilderResult);

				if (BuilderResult != EMetaSoundBuilderResult::Succeeded)
				{
					UE_LOG(LogMetaSound, Error, TEXT("Error creating a preset builder for MetaSound '%s'"), *AssetName);
					return;
				}

				UMetaSoundEditorSubsystem& MetaSoundEditorSubsystem = UMetaSoundEditorSubsystem::GetChecked();
				MetaSoundEditorSubsystem.BuildToAsset(&Builder, MetaSoundEditorSubsystem.GetDefaultAuthor(), AssetName, FPackageName::GetLongPackagePath(PackagePath), BuilderResult);
				if (BuilderResult != EMetaSoundBuilderResult::Succeeded)
				{
					UE_LOG(LogMetaSound, Error, TEXT("Error building to asset when creating preset '%s'"), *AssetName);
				}
			}
		}
	}

 	static FDelayedAutoRegisterHelper DelayedAutoRegister(EDelayedRegisterRunPhase::EndOfEngineInit, [] {
 		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
 		{
 			FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);
 
 			{
				UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UMetaSoundSource::StaticClass());

				FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
				{

					Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
					{
						{
							const TAttribute<FText> Label = LOCTEXT("Sound_PlaySound", "Play");
							const TAttribute<FText> ToolTip = LOCTEXT("Sound_PlaySoundTooltip", "Plays the selected sound.");
							const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "MediaAsset.AssetActions.Play.Small");

							FToolUIAction UIAction;
							UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&UAssetDefinition_MetaSoundSource::ExecutePlaySound);
							UIAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&UAssetDefinition_MetaSoundSource::CanExecutePlayCommand);
							InSection.AddMenuEntry("Sound_PlaySound", Label, ToolTip, Icon, UIAction);
						}
						{
							const TAttribute<FText> Label = LOCTEXT("Sound_StopSound", "Stop");
							const TAttribute<FText> ToolTip = LOCTEXT("Sound_StopSoundTooltip", "Stops the selected sounds.");
							const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "MediaAsset.AssetActions.Stop.Small");

							FToolUIAction UIAction;
							UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&UAssetDefinition_MetaSoundSource::ExecuteStopSound);
							InSection.AddMenuEntry("Sound_StopSound", Label, ToolTip, Icon, UIAction);
						}
						{
							const TAttribute<FText> Label = LOCTEXT("Sound_MuteSound", "Mute");
							const TAttribute<FText> ToolTip = LOCTEXT("Sound_MuteSoundTooltip", "Mutes the selected sounds.");
							const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "MediaAsset.AssetActions.Mute.Small");

							FToolUIAction UIAction;
							UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&UAssetDefinition_MetaSoundSource::ExecuteMuteSound);
							UIAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&UAssetDefinition_MetaSoundSource::CanExecuteMuteCommand);
							UIAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateStatic(&UAssetDefinition_MetaSoundSource::IsActionCheckedMute);
							InSection.AddMenuEntry("Sound_SoundMute", Label, ToolTip, Icon, UIAction, EUserInterfaceActionType::ToggleButton);
						}
						{
							const TAttribute<FText> Label = LOCTEXT("Sound_SoloSound", "Solo");
							const TAttribute<FText> ToolTip = LOCTEXT("Sound_SoloSoundTooltip", "Solos the selected sounds.");
							const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "MediaAsset.AssetActions.Solo.Small");

							FToolUIAction UIAction;
							UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&UAssetDefinition_MetaSoundSource::ExecuteSoloSound);
							UIAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&UAssetDefinition_MetaSoundSource::CanExecuteSoloCommand);
							UIAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateStatic(&UAssetDefinition_MetaSoundSource::IsActionCheckedSolo);
							InSection.AddMenuEntry("Sound_StopSolo", Label, ToolTip, Icon, UIAction, EUserInterfaceActionType::ToggleButton);
						}
						{
							const TAttribute<FText> Label = LOCTEXT("MetaSoundSource_CreatePreset", "Create MetaSound Source Preset");
							const TAttribute<FText> ToolTip = LOCTEXT("MetaSoundSource_CreatePresetToolTip", "Creates a MetaSoundSource Preset using the selected MetaSound's root graph as a reference.");
							const FSlateIcon Icon = Metasound::Editor::Style::CreateSlateIcon("ClassIcon.MetasoundSource");

							FToolUIAction UIAction;
							UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&ExecuteCreateMetaSoundPreset<UMetaSoundSource>);
							InSection.AddMenuEntry("MetaSoundSource_CreatePreset", Label, ToolTip, Icon, UIAction);
						}
					}));
				}
 			}
	
			{
				UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UMetaSoundPatch::StaticClass());

				FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
				Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
				{
					const TAttribute<FText> Label = LOCTEXT("MetaSoundPatch_CreatePreset", "Create MetaSound Patch Preset");
					const TAttribute<FText> ToolTip = LOCTEXT("MetaSoundPatch_CreatePresetToolTip", "Creates a MetaSoundSource Patch Preset using the selected MetaSound Patch's root graph as a reference.");

					const FSlateIcon Icon = Metasound::Editor::Style::CreateSlateIcon("ClassIcon.MetasoundPatch");

					FToolUIAction UIAction;
					UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&ExecuteCreateMetaSoundPreset<UMetaSoundPatch>);
					InSection.AddMenuEntry("MetaSoundPatch_CreatePreset", Label, ToolTip, Icon, UIAction);
				}));
			}
 		}));
	});
}


#undef LOCTEXT_NAMESPACE //MetaSoundEditor
