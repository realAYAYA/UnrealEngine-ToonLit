// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorMenuHelpers.h"

#include "ContentBrowserMenuContexts.h"
#include "NiagaraEditorUtilities.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "Widgets/AssetBrowser/NiagaraMenuFilters.h"

#define LOCTEXT_NAMESPACE "NiagaraEditorMenuHelpers"

void NiagaraEditorMenuHelpers::RegisterToolMenus()
{
	using namespace FNiagaraEditorUtilities::AssetBrowser;

	UToolMenus::Get()->RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
	{
		FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);
		
		const FName ContentBrowserNiagaraTagMenuName = TEXT("NiagaraEditorModule.ContentBrowserNiagaraTags");
		UToolMenu* AllTagsMenu = UToolMenus::Get()->RegisterMenu(ContentBrowserNiagaraTagMenuName);
		
		AllTagsMenu->AddDynamicSection(NAME_None, FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
		{
			// We read from and write to the context
			UNiagaraTagsContentBrowserFilterContext* Context = InMenu->FindContext<UNiagaraTagsContentBrowserFilterContext>();

			if(Context == nullptr)
			{
				return;
			}

			TArray<FStructuredAssetTagDefinitionLookupData> AssetTagDefinitionData = GetStructuredSortedAssetTagDefinitions();

			auto GetCheckState = [](const FNiagaraAssetTagDefinition& AssetTagDefinition, const FToolMenuContext& Context) -> ECheckBoxState
			{
				return Context.FindContext<UNiagaraTagsContentBrowserFilterContext>()->FilterData->ContainsActiveTagGuid(AssetTagDefinition.TagGuid) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			};

			auto AddTag = [](const FNiagaraAssetTagDefinition& AssetTagDefinition, const FToolMenuContext& Context)
			{
				Context.FindContext<UNiagaraTagsContentBrowserFilterContext>()->FilterData->AddTagGuid(AssetTagDefinition.TagGuid);
			};

			auto RemoveTag = [](const FNiagaraAssetTagDefinition& AssetTagDefinition, const FToolMenuContext& Context)
			{
				Context.FindContext<UNiagaraTagsContentBrowserFilterContext>()->FilterData->RemoveTagGuid(AssetTagDefinition.TagGuid);
			};

			// We create 3 menu sections: core Niagara tag filters, Project specified filters, and filters coming from other sources such as Engine Plugins (other than Niagara)
			const FText NiagaraSectionName = GetAssetTagSectionNameFromSource(EAssetTagSectionSource::NiagaraInternal);
			const FText ProjectSectionName = GetAssetTagSectionNameFromSource(EAssetTagSectionSource::Project);
			const FText OtherSectionName = GetAssetTagSectionNameFromSource(EAssetTagSectionSource::Other);

			{
				FToolMenuSection& NiagaraSection = InMenu->AddSection(FName(NiagaraSectionName.ToString()), NiagaraSectionName);
				NiagaraSection.InsertPosition = FToolMenuInsert(NAME_None, EToolMenuInsertType::First);
			}

			{
				FToolMenuSection& ProjectSection = InMenu->AddSection(FName(ProjectSectionName.ToString()), ProjectSectionName);
				ProjectSection.InsertPosition = FToolMenuInsert(FName(NiagaraSectionName.ToString()), EToolMenuInsertType::After);
			}

			{
				FToolMenuSection& Section = InMenu->AddSection(FName(OtherSectionName.ToString()), OtherSectionName);
				Section.InsertPosition = FToolMenuInsert(FName(ProjectSectionName.ToString()), EToolMenuInsertType::After);
			}

			for(const FStructuredAssetTagDefinitionLookupData& AssetTagDefinitionLookupData : AssetTagDefinitionData)
			{
				FName SectionName = FName(GetAssetTagSectionNameFromSource(AssetTagDefinitionLookupData.Source).ToString());
				
				for(const FNiagaraAssetTagDefinition& AssetTagDefinition : AssetTagDefinitionLookupData.AssetTagDefinitions)
				{
					FToolMenuSection* TagSection = InMenu->FindSection(SectionName);
					
					FText AssetTagLabel = AssetTagDefinition.AssetTag;
					FText AssetTagDescription = AssetTagDefinition.Description;
					
					FToolUIAction Action;
					Action.ExecuteAction = FToolMenuExecuteAction::CreateLambda([AssetTagDefinition, &GetCheckState, &AddTag, &RemoveTag](const FToolMenuContext& Context)
					{
						UNiagaraTagsContentBrowserFilterContext* NiagaraTagsContentBrowserFilterContext = Context.FindContext<UNiagaraTagsContentBrowserFilterContext>();

						if(NiagaraTagsContentBrowserFilterContext == nullptr)
						{
							return;
						}
					
						ECheckBoxState CurrentState = GetCheckState(AssetTagDefinition, Context);

						if(CurrentState == ECheckBoxState::Checked)
						{
							RemoveTag(AssetTagDefinition, Context);
						}
						else if(CurrentState == ECheckBoxState::Unchecked)
						{
							AddTag(AssetTagDefinition, Context);
						}
					});
					Action.GetActionCheckState = FToolMenuGetActionCheckState::CreateLambda([AssetTagDefinition, &GetCheckState](const FToolMenuContext& Context)
					{
						UNiagaraTagsContentBrowserFilterContext* NiagaraTagsContentBrowserFilterContext = Context.FindContext<UNiagaraTagsContentBrowserFilterContext>();

						if(NiagaraTagsContentBrowserFilterContext == nullptr)
						{
							return ECheckBoxState::Unchecked;
						}
						
						return GetCheckState(AssetTagDefinition, Context);
					});
					
					FToolMenuEntry Entry = FToolMenuEntry::InitMenuEntry(
						FName(AssetTagLabel.ToString()),
						AssetTagLabel,
						AssetTagDescription,
						FSlateIcon(),
						FToolUIActionChoice(Action), EUserInterfaceActionType::ToggleButton);
					TagSection->AddEntry(Entry);
				}
			}
		}));
	}));
}

void NiagaraEditorMenuHelpers::RegisterMenuExtensions()
{
	using namespace FNiagaraEditorUtilities::AssetBrowser;
	
	UToolMenus::Get()->RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
	{
		FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);

		// First we generate a standalone menu that will populate with all available tags dynamically
		const FName StandaloneTagManagementMenuName = TEXT("NiagaraEditorModule.ManageAssetTags");
		UToolMenu* TagMenu = UToolMenus::Get()->RegisterMenu(StandaloneTagManagementMenuName);
		TagMenu->AddDynamicSection(NAME_None, FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
		{
			TArray<FStructuredAssetTagDefinitionLookupData> AssetTagDefinitionData = GetStructuredSortedAssetTagDefinitions();

			UContentBrowserAssetContextMenuContext* AssetContextMenuContext = InMenu->FindContext<UContentBrowserAssetContextMenuContext>();

			if(AssetContextMenuContext == nullptr)
			{
				return;
			}

			auto GetCheckState = [](const FNiagaraAssetTagDefinition& AssetTagDefinition, TArray<FAssetData> SelectedAssets) -> ECheckBoxState
			{
				bool bAnyContainsTag = false;
				bool bAnyDoesNotContainTag = false;
				for(const FAssetData& SelectedAsset : SelectedAssets)
				{
					if(SelectedAsset.GetClass() == UNiagaraEmitter::StaticClass())
					{
						if(SelectedAsset.IsAssetLoaded())
						{
							UNiagaraEmitter* Emitter = Cast<UNiagaraEmitter>(SelectedAsset.GetAsset());
							if(Emitter->AssetTags.Contains(AssetTagDefinition))
							{
								bAnyContainsTag = true;
							}
							else
							{
								bAnyDoesNotContainTag = true;
							}
						}
						else
						{
							if(AssetTagDefinition.DoesAssetDataContainTag(SelectedAsset))
							{
								bAnyContainsTag = true;
							}
							else
							{
								bAnyDoesNotContainTag = true;
							}
						}
					}
					if(SelectedAsset.GetClass() == UNiagaraSystem::StaticClass())
					{
						if(SelectedAsset.IsAssetLoaded())
						{
							UNiagaraSystem* System = Cast<UNiagaraSystem>(SelectedAsset.GetAsset());
							if(System->AssetTags.Contains(AssetTagDefinition))
							{
								bAnyContainsTag = true;
							}
							else
							{
								bAnyDoesNotContainTag = true;
							}
						}

						else
						{
							if(AssetTagDefinition.DoesAssetDataContainTag(SelectedAsset))
							{
								bAnyContainsTag = true;
							}
							else
							{
								bAnyDoesNotContainTag = true;
							}
						}						
					}
					if(SelectedAsset.GetClass() == UNiagaraScript::StaticClass())
					{
						if(SelectedAsset.IsAssetLoaded())
						{
							UNiagaraScript* Script = Cast<UNiagaraScript>(SelectedAsset.GetAsset());
							if(Script->GetLatestScriptData()->AssetTagDefinitionReferences.Contains(AssetTagDefinition))
							{
								bAnyContainsTag = true;
							}
							else
							{
								bAnyDoesNotContainTag = true;
							}
						}
						else
						{
							if(AssetTagDefinition.DoesAssetDataContainTag(SelectedAsset))
							{
								bAnyContainsTag = true;
							}
							else
							{
								bAnyDoesNotContainTag = true;
							}
						}
					}

					if(bAnyContainsTag && bAnyDoesNotContainTag)
					{
						return ECheckBoxState::Undetermined;
					}
				}

				if(bAnyContainsTag && bAnyDoesNotContainTag == false)
				{
					return ECheckBoxState::Checked;
				}

				return ECheckBoxState::Unchecked;
			};

			auto AddTag = [](const FNiagaraAssetTagDefinition& AssetTagDefinition, TArray<FAssetData> SelectedAssets)
			{
				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

				for(const FAssetData& SelectedAsset : SelectedAssets)
				{
					if(SelectedAsset.GetClass() == UNiagaraEmitter::StaticClass())
					{
						UNiagaraEmitter* Emitter = Cast<UNiagaraEmitter>(SelectedAsset.GetAsset());
						if(Emitter->AssetTags.Contains(AssetTagDefinition))
						{
							continue;
						}
						else
						{
							Emitter->Modify();
							Emitter->AssetTags.Add(AssetTagDefinition);
						}
					}
					if(SelectedAsset.GetClass() == UNiagaraSystem::StaticClass())
					{
						UNiagaraSystem* System = Cast<UNiagaraSystem>(SelectedAsset.GetAsset());
						if(System->AssetTags.Contains(AssetTagDefinition))
						{
							continue;
						}
						else
						{
							System->Modify();
							System->AssetTags.Add(AssetTagDefinition);
						}
					}
					if(SelectedAsset.GetClass() == UNiagaraScript::StaticClass())
					{
						UNiagaraScript* Script = Cast<UNiagaraScript>(SelectedAsset.GetAsset());
						if(Script->GetLatestScriptData()->AssetTagDefinitionReferences.Contains(AssetTagDefinition))
						{
							continue;
						}
						else
						{
							Script->Modify();
							Script->GetLatestScriptData()->AssetTagDefinitionReferences.Add(AssetTagDefinition);
						}
					}

					AssetRegistryModule.Get().AssetUpdateTags(SelectedAsset.GetAsset(), EAssetRegistryTagsCaller::Fast);
				}
			};
			
			auto RemoveTag = [](const FNiagaraAssetTagDefinition& AssetTagDefinition, TArray<FAssetData> SelectedAssets)
			{
				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

				for(const FAssetData& SelectedAsset : SelectedAssets)
				{
					if(SelectedAsset.GetClass() == UNiagaraEmitter::StaticClass())
					{
						UNiagaraEmitter* Emitter = Cast<UNiagaraEmitter>(SelectedAsset.GetAsset());
						if(Emitter->AssetTags.Contains(AssetTagDefinition) == false)
						{
							continue;
						}
						else
						{
							Emitter->Modify();
							Emitter->AssetTags.Remove(AssetTagDefinition);
						}
					}
					if(SelectedAsset.GetClass() == UNiagaraSystem::StaticClass())
					{
						UNiagaraSystem* System = Cast<UNiagaraSystem>(SelectedAsset.GetAsset());
						if(System->AssetTags.Contains(AssetTagDefinition) == false)
						{
							continue;
						}
						else
						{
							System->Modify();
							System->AssetTags.Remove(AssetTagDefinition);
						}
					}
					if(SelectedAsset.GetClass() == UNiagaraScript::StaticClass())
					{
						UNiagaraScript* Script = Cast<UNiagaraScript>(SelectedAsset.GetAsset());
						if(Script->GetLatestScriptData()->AssetTagDefinitionReferences.Contains(AssetTagDefinition) == false)
						{
							continue;
						}
						else
						{
							Script->Modify();
							Script->GetLatestScriptData()->AssetTagDefinitionReferences.Remove(AssetTagDefinition);
						}
					}

					AssetRegistryModule.Get().AssetUpdateTags(SelectedAsset.GetAsset(), EAssetRegistryTagsCaller::Fast);
				}
			};

			// We create 3 menu sections: core Niagara tag filters, Project specified filters, and filters coming from other sources such as Engine Plugins (other than Niagara)
			const FText NiagaraSectionName = GetAssetTagSectionNameFromSource(EAssetTagSectionSource::NiagaraInternal);
			const FText ProjectSectionName = GetAssetTagSectionNameFromSource(EAssetTagSectionSource::Project);
			const FText OtherSectionName = GetAssetTagSectionNameFromSource(EAssetTagSectionSource::Other);

			{
				FToolMenuSection& NiagaraSection = InMenu->AddSection(FName(NiagaraSectionName.ToString()), NiagaraSectionName);
				NiagaraSection.InsertPosition = FToolMenuInsert(NAME_None, EToolMenuInsertType::First);
			}

			{
				FToolMenuSection& ProjectSection = InMenu->AddSection(FName(ProjectSectionName.ToString()), ProjectSectionName);
				ProjectSection.InsertPosition = FToolMenuInsert(FName(NiagaraSectionName.ToString()), EToolMenuInsertType::After);
			}

			{
				FToolMenuSection& Section = InMenu->AddSection(FName(OtherSectionName.ToString()), OtherSectionName);
				Section.InsertPosition = FToolMenuInsert(FName(ProjectSectionName.ToString()), EToolMenuInsertType::After);
			}

			for(const FStructuredAssetTagDefinitionLookupData& AssetTagDefinitionLookupData : AssetTagDefinitionData)
			{
				FName SectionName = FName(GetAssetTagSectionNameFromSource(AssetTagDefinitionLookupData.Source).ToString());
				
				for(const FNiagaraAssetTagDefinition& AssetTagDefinition : AssetTagDefinitionLookupData.AssetTagDefinitions)
				{
					if(AssetTagDefinition.GetSupportedClasses().Contains(AssetContextMenuContext->CommonClass))
					{
						FToolMenuSection* TagSection = InMenu->FindSection(SectionName);
						
						FText AssetTagLabel = AssetTagDefinition.AssetTag;
						FText AssetTagDescription = AssetTagDefinition.Description;
						
						FToolUIAction Action;
						Action.ExecuteAction = FToolMenuExecuteAction::CreateLambda([AssetTagDefinition, &GetCheckState, &AddTag, &RemoveTag](const FToolMenuContext& Context)
						{
							UContentBrowserAssetContextMenuContext* AssetContextMenuContext = Context.FindContext<UContentBrowserAssetContextMenuContext>();

							if(AssetContextMenuContext == nullptr)
							{
								return;
							}
						
							ECheckBoxState CurrentState = GetCheckState(AssetTagDefinition, AssetContextMenuContext->SelectedAssets);

							if(CurrentState == ECheckBoxState::Checked)
							{
								RemoveTag(AssetTagDefinition, AssetContextMenuContext->SelectedAssets);
							}
							else if(CurrentState == ECheckBoxState::Unchecked)
							{
								AddTag(AssetTagDefinition, AssetContextMenuContext->SelectedAssets);
							}
							// if there is a mix of states, we are removing the tags
							else if(CurrentState == ECheckBoxState::Undetermined)
							{
								RemoveTag(AssetTagDefinition, AssetContextMenuContext->SelectedAssets);
							}
						});
						Action.GetActionCheckState = FToolMenuGetActionCheckState::CreateLambda([AssetTagDefinition, &GetCheckState](const FToolMenuContext& Context)
						{
							UContentBrowserAssetContextMenuContext* AssetContextMenuContext = Context.FindContext<UContentBrowserAssetContextMenuContext>();

							if(AssetContextMenuContext == nullptr)
							{
								return ECheckBoxState::Unchecked;
							}
							
							return GetCheckState(AssetTagDefinition, AssetContextMenuContext->SelectedAssets);
						});
						
						FToolMenuEntry Entry = FToolMenuEntry::InitMenuEntry(
							FName(AssetTagLabel.ToString()),
							AssetTagLabel,
							AssetTagDescription,
							FSlateIcon(),
							FToolUIActionChoice(Action), EUserInterfaceActionType::ToggleButton);
						TagSection->AddEntry(Entry);
					}
				}
			}
		}));

		for(UClass* ExtendedClass : {UNiagaraEmitter::StaticClass(), UNiagaraSystem::StaticClass(), UNiagaraScript::StaticClass()})
		{
			// Then we extend the asset context menu by adding an empty sub menu
			const FName SubMenuName = TEXT("ManageTags");
			UToolMenu* AssetContextMenu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(ExtendedClass);
			AssetContextMenu->AddSubMenu(OwnerScoped.GetOwner(), "GetAssetActions", SubMenuName, LOCTEXT("ManageTabsSubMenuLabel", "Manage Tags"));
			
			// Now that we have added a submenu with a specific name, we need the full asset context menu path including the same sub menu name
			const FName AssetContextMenuNameWithSubMenu = UToolMenus::JoinMenuPaths(AssetContextMenu->GetMenuName(), SubMenuName);

			// Then, we register the standalone menu as the parent of the submenu. This will 'link' the empty submenu with our standalone menu.
			UToolMenus::Get()->RegisterMenu(AssetContextMenuNameWithSubMenu, StandaloneTagManagementMenuName);
		}
	}));
}

#undef LOCTEXT_NAMESPACE
