// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundAssetTypeActions.h"

#include "Components/AudioComponent.h"
#include "ContentBrowserMenuContexts.h"
#include "ContentBrowserModule.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "IContentBrowserSingleton.h"
#include "Metasound.h"
#include "MetasoundAssetBase.h"
#include "MetasoundSource.h"
#include "MetasoundEditor.h"
#include "MetasoundEditorGraphBuilder.h"
#include "MetasoundEditorModule.h"
#include "MetasoundEditorSettings.h"
#include "MetasoundFactory.h"
#include "MetasoundUObjectRegistry.h"
#include "ObjectEditorUtils.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "ToolMenus.h"
#include "ToolMenuSection.h"
#include "Widgets/Input/SButton.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "MetasoundAssetManager.h"
#include "Misc/AssetRegistryInterface.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SOverlay.h"


#define LOCTEXT_NAMESPACE "MetaSoundEditor"

namespace Metasound
{
	namespace Editor
	{
		namespace AssetTypeActionsPrivate
		{
			static const FText PresetLabel = LOCTEXT("MetaSoundPatch_CreatePreset", "Create MetaSound Patch Preset");
			static const FText PresetToolTip = LOCTEXT("MetaSoundPatch_CreatePresetToolTip", "Creates a MetaSoundPatch Preset using the selected MetaSound's root graph as a reference.");
			static const FText SourcePresetLabel = LOCTEXT("MetaSoundSource_CreatePreset", "Create MetaSound Source Preset");
			static const FText SourcePresetToolTip = LOCTEXT("MetaSoundSource_CreatePresetToolTip", "Creates a MetaSoundSource Preset using the selected MetaSound's root graph as a reference.");

			template <typename TClass, typename TFactory>
			void ExecuteCreatePreset(const FToolMenuContext& MenuContext)
			{
				if (const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext))
				{
					for (TClass* MetaSound : Context->LoadSelectedObjects<TClass>())
					{
						FString PackagePath;
						FString AssetName;

						IAssetTools::Get().CreateUniqueAssetName(MetaSound->GetOutermost()->GetName(), TEXT("_Preset"), PackagePath, AssetName);

						TFactory* Factory = NewObject<TFactory>();
						check(Factory);

						Factory->ReferencedMetaSoundObject = MetaSound;

						IContentBrowserSingleton::Get().CreateNewAsset(AssetName, FPackageName::GetLongPackagePath(PackagePath), TClass::StaticClass(), Factory);
					}
				}
			}

			template <typename TPresetClass, typename TFactory, typename TReferenceClass = TPresetClass>
			void RegisterPresetAction(const TAttribute<FText>& InLabel, const TAttribute<FText>& InToolTip)
			{
				FString ClassName = TReferenceClass::StaticClass()->GetName();
				const FString MenuName = TEXT("ContentBrowser.AssetContextMenu.") + ClassName;
				UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(*MenuName);
				if (!ensure(Menu))
				{
					return;
				}

				FName PresetClassName = TPresetClass::StaticClass()->GetFName();
				const FString EntryName = FString::Printf(TEXT("%sTo%s_Preset"), *PresetClassName.ToString(), *ClassName);
				FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
				Section.AddDynamicEntry(*EntryName, FNewToolMenuSectionDelegate::CreateLambda([PresetClassName, Label = InLabel, ToolTip = InToolTip](FToolMenuSection& InSection)
				{
					const FName IconName = *FString::Printf(TEXT("ClassIcon.%s"), *PresetClassName.ToString());
					const FSlateIcon Icon = Style::CreateSlateIcon(IconName);
					const FToolMenuExecuteAction UIExecuteAction = FToolMenuExecuteAction::CreateStatic(&ExecuteCreatePreset<TPresetClass, TFactory>);

					const FString PresetEntryName = FString::Printf(TEXT("%s_CreatePreset"), *PresetClassName.ToString());
					InSection.AddMenuEntry(*PresetEntryName, Label, ToolTip, Icon, UIExecuteAction);
				}));
			}

			bool IsPlaying(const FSoftObjectPath& InSourcePath)
			{
				check(GEditor);
				if (const UAudioComponent* PreviewComponent = GEditor->GetPreviewAudioComponent())
				{
					if (PreviewComponent->IsPlaying())
					{
						if (const USoundBase* Sound = PreviewComponent->Sound)
						{
							return FSoftObjectPath(Sound) == InSourcePath;
						}
					}
				}

				return false;
			}

			void PlaySound(UMetaSoundSource& InSource)
			{
				// If editor is open, call into it to play to start all visualization requirements therein
				// specific to auditioning MetaSounds (ex. priming audio bus used for volume metering, playtime
				// widget, etc.)
				TSharedPtr<FEditor> Editor = FGraphBuilder::GetEditorForMetasound(InSource);
				if (Editor.IsValid())
				{
					Editor->Play();
					return;
				}

				check(GEditor);
				FGraphBuilder::RegisterGraphWithFrontend(InSource);
				GEditor->PlayPreviewSound(&InSource);
			}

			void StopSound(const UMetaSoundSource& InSource)
			{
				// If editor is open, call into it to play to start all visualization requirements therein
				// specific to auditioning MetaSounds (ex. priming audio bus used for volume metering, playtime
				// widget, etc.)
				TSharedPtr<FEditor> Editor = FGraphBuilder::GetEditorForMetasound(InSource);
				if (Editor.IsValid())
				{
					Editor->Stop();
					return;
				}

				check(GEditor);
				if (UAudioComponent* PreviewComponent = GEditor->GetPreviewAudioComponent())
				{
					PreviewComponent->Stop();
				}
			}

			bool GetIsPreset(const FSoftObjectPath& InSourcePath)
			{
				using namespace Metasound::Editor;
				using namespace Metasound::Frontend;

				int32 bIsPreset = 0;
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

				return static_cast<bool>(bIsPreset);
			}

			const FSlateBrush* GetClassBrush(const FAssetData& InAssetData, FName InClassName, bool bIsThumbnail = false)
			{
				const bool bIsPreset = AssetTypeActionsPrivate::GetIsPreset(InAssetData.ToSoftObjectPath());
				FString BrushName = FString::Printf(TEXT("MetasoundEditor.%s"), *InClassName.ToString());
				if (bIsPreset)
				{
					BrushName += TEXT(".Preset");
				}
				BrushName += bIsThumbnail ? TEXT(".Thumbnail") : TEXT(".Icon");

				return &Style::GetSlateBrushSafe(FName(*BrushName));
			}
		} // namespace AssetTypeActionsPrivate

		UClass* FAssetTypeActions_MetaSoundPatch::GetSupportedClass() const
		{
			return UMetaSoundPatch::StaticClass();
		}

		FColor FAssetTypeActions_MetaSoundPatch::GetTypeColor() const
		{
			if (const ISlateStyle* MetasoundStyle = FSlateStyleRegistry::FindSlateStyle("MetaSoundStyle"))
			{
				return MetasoundStyle->GetColor("MetaSoundPatch.Color").ToFColorSRGB();
			}

			return FColor::White;
		}

		void FAssetTypeActions_MetaSoundPatch::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> ToolkitHost)
		{
			const EToolkitMode::Type Mode = ToolkitHost.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

			for (UObject* Object : InObjects)
			{
				if (UMetaSoundPatch* Metasound = Cast<UMetaSoundPatch>(Object))
				{
					TSharedRef<FEditor> NewEditor = MakeShared<FEditor>();
					NewEditor->InitMetasoundEditor(Mode, ToolkitHost, Metasound);
				}
			}
		}

		void FAssetTypeActions_MetaSoundPatch::RegisterMenuActions()
		{
			using namespace AssetTypeActionsPrivate;
			RegisterPresetAction<UMetaSoundPatch, UMetaSoundFactory>(PresetLabel, PresetToolTip);
		}

		const TArray<FText>& FAssetTypeActions_MetaSoundPatch::GetSubMenus() const
		{
			if (GetDefault<UMetasoundEditorSettings>()->bPinMetaSoundPatchInAssetMenu)
			{
				static const TArray<FText> SubMenus;
				return SubMenus;
			}
			
			static const TArray<FText> SubMenus
			{
				LOCTEXT("AssetSoundMetaSoundsSubMenu", "MetaSounds"),
			};

			return SubMenus;
		}

		const FSlateBrush* FAssetTypeActions_MetaSoundPatch::GetThumbnailBrush(const FAssetData& InAssetData, const FName InClassName) const
		{
			constexpr bool bIsThumbnail = true;
			return AssetTypeActionsPrivate::GetClassBrush(InAssetData, InClassName, bIsThumbnail);
		}

		const FSlateBrush* FAssetTypeActions_MetaSoundPatch::GetIconBrush(const FAssetData& InAssetData, const FName InClassName) const
		{
			return AssetTypeActionsPrivate::GetClassBrush(InAssetData, InClassName);
		}

		UClass* FAssetTypeActions_MetaSoundSource::GetSupportedClass() const
		{
			return UMetaSoundSource::StaticClass();
		}

		FColor FAssetTypeActions_MetaSoundSource::GetTypeColor() const
		{
			if (const ISlateStyle* MetasoundStyle = FSlateStyleRegistry::FindSlateStyle("MetaSoundStyle"))
			{
				return MetasoundStyle->GetColor("MetaSoundSource.Color").ToFColorSRGB();
			}

			return FColor::White;
		}

		void FAssetTypeActions_MetaSoundSource::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> ToolkitHost)
		{
			const EToolkitMode::Type Mode = ToolkitHost.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

			for (UObject* Object : InObjects)
			{
				if (UMetaSoundSource* Metasound = Cast<UMetaSoundSource>(Object))
				{
					TSharedRef<FEditor> NewEditor = MakeShared<FEditor>();
					NewEditor->InitMetasoundEditor(Mode, ToolkitHost, Metasound);
				}
			}
		}

		void FAssetTypeActions_MetaSoundSource::RegisterMenuActions()
		{
			using namespace AssetTypeActionsPrivate;
			RegisterPresetAction<UMetaSoundSource, UMetaSoundSourceFactory>(SourcePresetLabel, SourcePresetToolTip);
		}

		const TArray<FText>& FAssetTypeActions_MetaSoundSource::GetSubMenus() const
		{
			if (GetDefault<UMetasoundEditorSettings>()->bPinMetaSoundSourceInAssetMenu)
			{
				static const TArray<FText> SubMenus;
				return SubMenus;
			}

			static const TArray<FText> SubMenus
			{
				LOCTEXT("AssetSoundMetaSoundSourceSubMenu", "MetaSounds"),
			};

			return SubMenus;
		}

		TSharedPtr<SWidget> FAssetTypeActions_MetaSoundSource::GetThumbnailOverlay(const FAssetData& InAssetData) const
		{
			TSharedPtr<SBox> Box = SNew(SBox)
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.Padding(2.0f);

			auto OnGetDisplayPlaybackLambda = [Box, Path = InAssetData.ToSoftObjectPath()]()
			{
				using namespace AssetTypeActionsPrivate;

				const bool bIsValid = Box.IsValid();
				if (!bIsValid)
				{
					return FCoreStyle::Get().GetBrush(TEXT("NoBrush"));
				}

				const bool bIsPlaying = IsPlaying(Path);
				const bool bIsHovered = Box->IsHovered();
				if (!bIsPlaying && !bIsHovered)
				{
					return FCoreStyle::Get().GetBrush(TEXT("NoBrush"));
				}

				FString IconName = TEXT("MetasoundEditor");
				IconName += bIsPlaying ? TEXT(".Stop.Thumbnail") : TEXT(".Play.Thumbnail");
				if (bIsHovered)
				{
					IconName += TEXT(".Hovered");
				}

				return &Style::GetSlateBrushSafe(FName(*IconName));
			};

			auto OnClickedLambda = [Path = InAssetData.ToSoftObjectPath()]()
			{
				using namespace AssetTypeActionsPrivate;
				// Load and play sound
				if (UMetaSoundSource* MetaSoundSource = Cast<UMetaSoundSource>(Path.TryLoad()))
				{
					if (IsPlaying(Path))
					{
						StopSound(*MetaSoundSource);
					}
					else
					{
						PlaySound(*MetaSoundSource);
					}
				}

				return FReply::Handled();
			};

			auto OnToolTipTextLambda = [ClassName = GetSupportedClass()->GetFName(), Path = InAssetData.ToSoftObjectPath()]()
			{
				using namespace AssetTypeActionsPrivate;

				FText Format;
				if (IsPlaying(Path))
				{
					Format = LOCTEXT("StopPreviewMetaSoundFromIconToolTip", "Stop Previewing {0}");
				}
				else
				{
					Format = LOCTEXT("PreviewMetaSoundFromIconToolTip_Editor", "Preview {0}");
				}

				return FText::Format(Format, FText::FromName(ClassName));
			};

			Box->SetContent(
				SNew(SButton)
				.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ToolTipText_Lambda(OnToolTipTextLambda)
				.Cursor(EMouseCursor::Default) // The outer widget can specify a DragHand cursor, so overridden here
				.ForegroundColor(FSlateColor::UseForeground())
				.IsFocusable(false)
				.OnClicked_Lambda(OnClickedLambda)
				[
					SNew(SImage)
					.Image_Lambda(OnGetDisplayPlaybackLambda)
				]
			);
			return Box;
		}

		const FSlateBrush* FAssetTypeActions_MetaSoundSource::GetThumbnailBrush(const FAssetData& InAssetData, const FName InClassName) const
		{
			constexpr bool bIsThumbnail = true;
			return AssetTypeActionsPrivate::GetClassBrush(InAssetData, InClassName, bIsThumbnail);
		}

		const FSlateBrush* FAssetTypeActions_MetaSoundSource::GetIconBrush(const FAssetData& InAssetData, const FName InClassName) const
		{
			return AssetTypeActionsPrivate::GetClassBrush(InAssetData, InClassName);
		}
	} // namespace Editor
} // namespace Metasound
#undef LOCTEXT_NAMESPACE //MetaSoundEditor
