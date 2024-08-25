// Copyright Epic Games, Inc. All Rights Reserved.
#include "LevelInstanceEditorModule.h"
#include "LevelInstanceActorDetails.h"
#include "LevelInstancePivotDetails.h"
#include "PackedLevelActorUtils.h"
#include "LevelInstanceFilterPropertyTypeCustomization.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "LevelInstance/LevelInstanceInterface.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "PackedLevelActor/PackedLevelActor.h"
#include "PackedLevelActor/PackedLevelActorBuilder.h"
#include "LevelInstanceEditorSettings.h"
#include "ToolMenus.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "EditorModeRegistry.h"
#include "FileHelpers.h"
#include "LevelInstanceEditorMode.h"
#include "LevelInstanceEditorModeCommands.h"
#include "LevelEditorMenuContext.h"
#include "ContentBrowserMenuContexts.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "LevelEditor.h"
#include "Engine/Selection.h"
#include "PropertyEditorModule.h"
#include "EditorLevelUtils.h"
#include "Modules/ModuleManager.h"
#include "Misc/MessageDialog.h"
#include "NewLevelDialogModule.h"
#include "Interfaces/IMainFrameModule.h"
#include "Editor/EditorEngine.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/BlueprintFactory.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SWindow.h"
#include "SNewLevelInstanceDialog.h"
#include "MessageLogModule.h"
#include "Settings/EditorExperimentalSettings.h"
#include "WorldPartition/WorldPartitionConverter.h"
#include "WorldPartition/WorldPartitionActorLoaderInterface.h"

IMPLEMENT_MODULE( FLevelInstanceEditorModule, LevelInstanceEditor );

#define LOCTEXT_NAMESPACE "LevelInstanceEditor"

DEFINE_LOG_CATEGORY_STATIC(LogLevelInstanceEditor, Log, All);

namespace LevelInstanceMenuUtils
{
	bool IsExperimentalSettingEnabled(ILevelInstanceInterface* LevelInstance)
	{
		if (APackedLevelActor* Actor = Cast<APackedLevelActor>(LevelInstance))
		{
			if (!GetDefault<UEditorExperimentalSettings>()->bPackedLevelActor)
			{
				return false;
			}
		}

		return GetDefault<UEditorExperimentalSettings>()->bLevelInstance;
	}

	FToolMenuSection& CreateLevelSection(UToolMenu* Menu)
	{
		const FName LevelSectionName = TEXT("Level");
		FToolMenuSection* SectionPtr = Menu->FindSection(LevelSectionName);
		if (!SectionPtr)
		{
			SectionPtr = &(Menu->AddSection(LevelSectionName, LOCTEXT("LevelSectionLabel", "Level")));
		}
		FToolMenuSection& Section = *SectionPtr;
		return Section;
	}

	void CreateEditMenuEntry(FToolMenuSection& Section, ILevelInstanceInterface* LevelInstance, AActor* ContextActor, bool bSingleEntry)
	{
		FToolUIAction LevelInstanceEditAction;
		FText EntryDesc;
		AActor* LevelInstanceActor = CastChecked<AActor>(LevelInstance);
		const bool bCanEdit = LevelInstance->CanEnterEdit(&EntryDesc);

		LevelInstanceEditAction.ExecuteAction.BindLambda([LevelInstance, ContextActor](const FToolMenuContext&)
		{
			LevelInstance->EnterEdit(ContextActor);
		});
		LevelInstanceEditAction.CanExecuteAction.BindLambda([bCanEdit](const FToolMenuContext&)
		{
			return bCanEdit;
		});

		FText EntryLabel = bSingleEntry ? LOCTEXT("EditLevelInstances", "Edit") : FText::FromString(LevelInstance->GetWorldAsset().GetAssetName());
		if (bCanEdit)
		{
			EntryDesc = FText::Format(LOCTEXT("LevelInstanceName", "{0}:{1}"), FText::FromString(LevelInstanceActor->GetActorLabel()), FText::FromString(LevelInstance->GetWorldAssetPackage()));
		}
		Section.AddMenuEntry(NAME_None, EntryLabel, EntryDesc, FSlateIcon(), LevelInstanceEditAction);
	}

	void CreateEditSubMenu(UToolMenu* Menu, TArray<ILevelInstanceInterface*> LevelInstanceHierarchy, AActor* ContextActor)
	{
		FToolMenuSection& Section = Menu->AddSection(NAME_None, LOCTEXT("LevelInstanceContextEditSection", "Context"));
		for (ILevelInstanceInterface* LevelInstance : LevelInstanceHierarchy)
		{
			CreateEditMenuEntry(Section, LevelInstance, ContextActor, false);
		}
	}
		
	void MoveSelectionToLevelInstance(ILevelInstanceInterface* DestinationLevelInstance)
	{
		TArray<AActor*> ActorsToMove;
		ActorsToMove.Reserve(GEditor->GetSelectedActorCount());
		for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		{
			if (AActor* Actor = Cast<AActor>(*It))
			{
				ActorsToMove.Add(Actor);
			}
		}

		DestinationLevelInstance->MoveActorsTo(ActorsToMove);
	}
		
	void CreateEditMenu(UToolMenu* Menu, AActor* ContextActor)
	{
		if (ULevelInstanceSubsystem* LevelInstanceSubsystem = ContextActor->GetWorld()->GetSubsystem<ULevelInstanceSubsystem>())
		{
			TArray<ILevelInstanceInterface*> LevelInstanceHierarchy;
			LevelInstanceSubsystem->ForEachLevelInstanceAncestorsAndSelf(ContextActor, [&LevelInstanceHierarchy](ILevelInstanceInterface* AncestorLevelInstance)
			{
				if (IsExperimentalSettingEnabled(AncestorLevelInstance))
				{
					LevelInstanceHierarchy.Add(AncestorLevelInstance);
				}
				return true;
			});

			// Don't create sub menu if only one Level Instance is available to edit
			if (LevelInstanceHierarchy.Num() == 1)
			{
				FToolMenuSection& Section = CreateLevelSection(Menu);
				CreateEditMenuEntry(Section, LevelInstanceHierarchy[0], ContextActor, true);
			}
			else if(LevelInstanceHierarchy.Num() > 1)
			{
				FToolMenuSection& Section = CreateLevelSection(Menu);
				Section.AddSubMenu(
					"EditLevelInstances",
					LOCTEXT("EditLevelInstances", "Edit"),
					TAttribute<FText>(),
					FNewToolMenuDelegate::CreateStatic(&CreateEditSubMenu, MoveTemp(LevelInstanceHierarchy), ContextActor)
				);
			}
		}
	}

	void CreateCommitDiscardMenu(UToolMenu* Menu, AActor* ContextActor)
	{
		ILevelInstanceInterface* LevelInstanceEdit = nullptr;
		if (ContextActor)
		{
			if (ULevelInstanceSubsystem* LevelInstanceSubsystem = ContextActor->GetWorld()->GetSubsystem<ULevelInstanceSubsystem>())
			{
				LevelInstanceEdit = LevelInstanceSubsystem->GetEditingLevelInstance();
			}
		}

		if (!LevelInstanceEdit)
		{
			if (ULevelInstanceSubsystem* LevelInstanceSubsystem = GEditor->GetEditorWorldContext().World()->GetSubsystem<ULevelInstanceSubsystem>())
			{
				LevelInstanceEdit = LevelInstanceSubsystem->GetEditingLevelInstance();
			}
		}

		if (LevelInstanceEdit)
		{
			FToolMenuSection& Section = CreateLevelSection(Menu);

			FText CommitTooltip;
			const bool bCanCommit = LevelInstanceEdit->CanExitEdit(/*bDiscardEdits=*/false, &CommitTooltip);

			FToolUIAction CommitAction;
			CommitAction.ExecuteAction.BindLambda([LevelInstanceEdit](const FToolMenuContext&) { LevelInstanceEdit->ExitEdit(/*bDiscardEdits=*/false); });
			CommitAction.CanExecuteAction.BindLambda([bCanCommit](const FToolMenuContext&) { return bCanCommit; });
			Section.AddMenuEntry(NAME_None, LOCTEXT("LevelInstanceCommitLabel", "Commit"), CommitTooltip, FSlateIcon(), CommitAction);

			FText DiscardTooltip;
			const bool bCanDiscard = LevelInstanceEdit->CanExitEdit(/*bDiscardEdits=*/true, &DiscardTooltip);

			FToolUIAction DiscardAction;
			DiscardAction.ExecuteAction.BindLambda([LevelInstanceEdit](const FToolMenuContext&) { LevelInstanceEdit->ExitEdit(/*bDiscardEdits=*/true); });
			DiscardAction.CanExecuteAction.BindLambda([bCanDiscard](const FToolMenuContext&) { return bCanDiscard; });
			Section.AddMenuEntry(NAME_None, LOCTEXT("LevelInstanceDiscardLabel", "Discard"), DiscardTooltip, FSlateIcon(), DiscardAction);
		}
	}

	void CreateSetCurrentMenu(UToolMenu* Menu, AActor* ContextActor)
	{
		ILevelInstanceInterface* LevelInstanceEdit = nullptr;
		if (ULevelInstanceSubsystem* LevelInstanceSubsystem = ContextActor->GetWorld()->GetSubsystem<ULevelInstanceSubsystem>())
		{
			LevelInstanceEdit = LevelInstanceSubsystem->GetEditingLevelInstance();

			if (LevelInstanceEdit)
			{
				FToolUIAction LevelInstanceSetCurrentAction;
				LevelInstanceSetCurrentAction.ExecuteAction.BindLambda([LevelInstanceEdit](const FToolMenuContext&)
				{
					LevelInstanceEdit->SetCurrent();
				});

				FToolMenuSection& Section = CreateLevelSection(Menu);
				Section.AddMenuEntry(TEXT("LevelInstanceSetCurrent"), LOCTEXT("LevelInstanceSetCurrent", "Set Current Level"), TAttribute<FText>(), FSlateIcon(), LevelInstanceSetCurrentAction);
			}
		}
	}

	void CreateMoveSelectionToMenu(UToolMenu* Menu)
	{
		if (GEditor->GetSelectedActorCount() > 0)
		{
			ILevelInstanceInterface* LevelInstanceEdit = nullptr;
			ULevelInstanceSubsystem* LevelInstanceSubsystem = GEditor->GetEditorWorldContext().World()->GetSubsystem<ULevelInstanceSubsystem>();
			if (LevelInstanceSubsystem)
			{
				LevelInstanceEdit = LevelInstanceSubsystem->GetEditingLevelInstance();
			}
			
			if (LevelInstanceEdit)
			{
				FToolUIAction LevelInstanceMoveSelectionAction;

				LevelInstanceMoveSelectionAction.CanExecuteAction.BindLambda([LevelInstanceEdit, LevelInstanceSubsystem](const FToolMenuContext& MenuContext)
					{
						for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
						{
							if (AActor* Actor = Cast<AActor>(*It))
							{
								if (Actor->GetLevel() == LevelInstanceSubsystem->GetLevelInstanceLevel(LevelInstanceEdit))
								{
									return false;
								}
							}
						}

						return GEditor->GetSelectedActorCount() > 0;
					});

				LevelInstanceMoveSelectionAction.ExecuteAction.BindLambda([LevelInstanceEdit](const FToolMenuContext&)
					{
						MoveSelectionToLevelInstance(LevelInstanceEdit);
					});

				FToolMenuSection& Section = CreateLevelSection(Menu);
				Section.AddMenuEntry(TEXT("LevelInstanceMoveSelectionTo"), LOCTEXT("LevelInstanceMoveSelectionTo", "Move Selection to"), TAttribute<FText>(), FSlateIcon(), LevelInstanceMoveSelectionAction);

			}
		}
	}

	UClass* GetDefaultLevelInstanceClass(ELevelInstanceCreationType CreationType)
	{
		if (CreationType == ELevelInstanceCreationType::PackedLevelActor)
		{
			return APackedLevelActor::StaticClass();
		}

		ULevelInstanceEditorSettings* LevelInstanceEditorSettings = GetMutableDefault<ULevelInstanceEditorSettings>();
		if (!LevelInstanceEditorSettings->LevelInstanceClassName.IsEmpty())
		{
			UClass* LevelInstanceClass = LoadClass<AActor>(nullptr, *LevelInstanceEditorSettings->LevelInstanceClassName, nullptr, LOAD_NoWarn);
			if (LevelInstanceClass && LevelInstanceClass->ImplementsInterface(ULevelInstanceInterface::StaticClass()))
			{
				return LevelInstanceClass;
			}
		}

		return ALevelInstance::StaticClass();
	}

	bool AreAllSelectedLevelInstancesRootSelections()
	{
		for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		{
			if (ILevelInstanceInterface* LevelInstance = Cast<ILevelInstanceInterface>(*It))
			{
				if (CastChecked<AActor>(*It)->GetSelectionParent() != nullptr)
				{
					return false;
				}
			}
		}

		return true;
	}
		
	void CreateLevelInstanceFromSelection(ULevelInstanceSubsystem* LevelInstanceSubsystem, ELevelInstanceCreationType CreationType)
	{
		TArray<AActor*> ActorsToMove;
		ActorsToMove.Reserve(GEditor->GetSelectedActorCount());
		for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		{
			if (AActor* Actor = Cast<AActor>(*It))
			{
				ActorsToMove.Add(Actor);
			}
		}

		IMainFrameModule& MainFrameModule = FModuleManager::GetModuleChecked<IMainFrameModule>("MainFrame");

		TSharedPtr<SWindow> NewLevelInstanceWindow =
			SNew(SWindow)
			.Title(FText::Format(LOCTEXT("NewLevelInstanceWindowTitle", "New {0}"), StaticEnum<ELevelInstanceCreationType>()->GetDisplayNameTextByValue((int64)CreationType)))
			.SupportsMinimize(false)
			.SupportsMaximize(false)
			.SizingRule(ESizingRule::Autosized);

		TSharedRef<SNewLevelInstanceDialog> NewLevelInstanceDialog =
			SNew(SNewLevelInstanceDialog)
			.ParentWindow(NewLevelInstanceWindow)
			.PivotActors(ActorsToMove);

		const bool bForceExternalActors = LevelInstanceSubsystem->GetWorld()->IsPartitionedWorld();
		FNewLevelInstanceParams& DialogParams = NewLevelInstanceDialog->GetCreationParams();
		DialogParams.Type = CreationType;
		DialogParams.bAlwaysShowDialog = GetDefault<ULevelInstanceEditorPerProjectUserSettings>()->bAlwaysShowDialog;
		DialogParams.PivotType = GetDefault<ULevelInstanceEditorPerProjectUserSettings>()->PivotType;
		DialogParams.PivotActor = DialogParams.PivotType == ELevelInstancePivotType::Actor ? ActorsToMove[0] : nullptr;
		DialogParams.HideCreationType();
		DialogParams.SetForceExternalActors(bForceExternalActors);
		NewLevelInstanceWindow->SetContent(NewLevelInstanceDialog);

		if (GetDefault<ULevelInstanceEditorPerProjectUserSettings>()->bAlwaysShowDialog)
		{
			FSlateApplication::Get().AddModalWindow(NewLevelInstanceWindow.ToSharedRef(), MainFrameModule.GetParentWindow());
		}
		
		if (!GetDefault<ULevelInstanceEditorPerProjectUserSettings>()->bAlwaysShowDialog || NewLevelInstanceDialog->ClickedOk())
		{
			FNewLevelInstanceParams CreationParams(NewLevelInstanceDialog->GetCreationParams());
			ULevelInstanceEditorPerProjectUserSettings::UpdateFrom(CreationParams);

			FNewLevelDialogModule& NewLevelDialogModule = FModuleManager::LoadModuleChecked<FNewLevelDialogModule>("NewLevelDialog");
			FString TemplateMapPackage;
			bool bOutIsPartitionedWorld = false;
			const bool bShowPartitionedTemplates = false;
			ULevelInstanceEditorSettings* LevelInstanceEditorSettings = GetMutableDefault<ULevelInstanceEditorSettings>();
			if (!LevelInstanceEditorSettings->TemplateMapInfos.Num() || NewLevelDialogModule.CreateAndShowTemplateDialog(MainFrameModule.GetParentWindow(), LOCTEXT("LevelInstanceTemplateDialog", "Choose Level Instance Template..."), GetMutableDefault<ULevelInstanceEditorSettings>()->TemplateMapInfos, TemplateMapPackage, bShowPartitionedTemplates, bOutIsPartitionedWorld))
			{
				UPackage* TemplatePackage = !TemplateMapPackage.IsEmpty() ? LoadPackage(nullptr, *TemplateMapPackage, LOAD_None) : nullptr;
				
				CreationParams.TemplateWorld = TemplatePackage ? UWorld::FindWorldInPackage(TemplatePackage) : nullptr;
				CreationParams.LevelInstanceClass = GetDefaultLevelInstanceClass(CreationType);
				CreationParams.bEnableStreaming = LevelInstanceEditorSettings->bEnableStreaming;

				if (!LevelInstanceSubsystem->CreateLevelInstanceFrom(ActorsToMove, CreationParams))
				{
					FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("CreateFromSelectionFailMsg", "Failed to create from selection. Check log for details."), LOCTEXT("CreateFromSelectionFailTitle", "Create from selection failed"));
				}
			}
		}
	}

	void CreateCreateMenu(UToolMenu* ToolMenu)
	{
		if (ULevelInstanceSubsystem* LevelInstanceSubsystem = GEditor->GetEditorWorldContext().World()->GetSubsystem<ULevelInstanceSubsystem>())
		{
			if (GEditor->GetSelectedActorCount() > 0)
			{
				FToolMenuSection& Section = ToolMenu->AddSection("ActorSelectionSectionName", LOCTEXT("ActorSelectionSectionLabel", "Actor Selection"));

				if (GetDefault<UEditorExperimentalSettings>()->bLevelInstance)
				{
					Section.AddMenuEntry(
						TEXT("CreateLevelInstance"),
						FText::Format(LOCTEXT("CreateFromSelectionLabel", "Create {0}..."), StaticEnum<ELevelInstanceCreationType>()->GetDisplayNameTextByValue((int64)ELevelInstanceCreationType::LevelInstance)),
						TAttribute<FText>(),
						FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.LevelInstance"),
						FExecuteAction::CreateStatic(&LevelInstanceMenuUtils::CreateLevelInstanceFromSelection, LevelInstanceSubsystem, ELevelInstanceCreationType::LevelInstance));
				}

				if (GetDefault<UEditorExperimentalSettings>()->bPackedLevelActor)
				{
					Section.AddMenuEntry(
						TEXT("CreatePackedLevelBlueprint"),
						FText::Format(LOCTEXT("CreateFromSelectionLabel", "Create {0}..."), StaticEnum<ELevelInstanceCreationType>()->GetDisplayNameTextByValue((int64)ELevelInstanceCreationType::PackedLevelActor)),
						TAttribute<FText>(),
						FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.PackedLevelActor"),
						FExecuteAction::CreateStatic(&LevelInstanceMenuUtils::CreateLevelInstanceFromSelection, LevelInstanceSubsystem, ELevelInstanceCreationType::PackedLevelActor));
				}
			}
		}
	}
		
	void CreateBreakSubMenu(UToolMenu* Menu, TArray<ILevelInstanceInterface*> BreakableLevelInstances)
	{
		static int32 BreakLevels = 1;
		ULevelInstanceEditorPerProjectUserSettings* Settings = GetMutableDefault<ULevelInstanceEditorPerProjectUserSettings>();

		if (ULevelInstanceSubsystem* LevelInstanceSubsystem = GEditor->GetEditorWorldContext().World()->GetSubsystem<ULevelInstanceSubsystem>())
		{
			FToolMenuSection& Section = Menu->AddSection("Options", LOCTEXT("LevelInstanceBreakOptionsSection", "Options"));

			FToolMenuEntry OrganizeInFoldersEntry = FToolMenuEntry::InitMenuEntry(
				"OrganizeInFolders",
				LOCTEXT("OrganizeActorsInFolders", "Keep Folders"),
				LOCTEXT(
					"OrganizeActorsInFoldersTooltip",
					"Should the actors be placed in the folder the Level Instance is in, "
					"and keep the folder structure they had inside the Level Instance?"
				),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([Settings]()
					{
						Settings->bKeepFoldersDuringBreak = !Settings->bKeepFoldersDuringBreak;
					}),
					FCanExecuteAction(),
					FGetActionCheckState::CreateLambda([Settings]
					{
						return Settings->bKeepFoldersDuringBreak ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
				),
				EUserInterfaceActionType::ToggleButton
			);
			OrganizeInFoldersEntry.bShouldCloseWindowAfterMenuSelection = false;
			Section.AddEntry(OrganizeInFoldersEntry);

			TSharedRef<SWidget> MenuWidget =
				SNew(SBox)
				.Padding(FMargin(5, 2, 5, 0))
				[
					SNew(SNumericEntryBox<int32>)
					.MinValue(1)
					.Value_Lambda([]() { return BreakLevels; })
					.OnValueChanged_Lambda([](int32 InValue) { BreakLevels = InValue; })
					.Label()
					[
						SNumericEntryBox<int32>::BuildLabel(LOCTEXT("BreakLevelsLabel", "Levels"), FLinearColor::White, FLinearColor::Transparent)
					]
				];

			Section.AddEntry(FToolMenuEntry::InitWidget("SetBreakLevels", MenuWidget, FText::GetEmpty(), false));

			Section.AddSeparator(NAME_None);

			FToolMenuEntry ExecuteEntry = FToolMenuEntry::InitMenuEntry(
				"ExecuteBreak",
				LOCTEXT("BreakLevelInstances_BreakLevelInstanceButton", "Break Level Instance(s)"),
				FText(),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([BreakableLevelInstances, LevelInstanceSubsystem, Settings]()
					{
						const FText LevelInstanceBreakWarning = FText::Format(
							LOCTEXT(
								"BreakingLevelInstance",
								"You are about to break {0} level instance(s). This action cannot be undone. Are you sure ?"
							),
							FText::AsNumber(BreakableLevelInstances.Num())
						);

						if (FMessageDialog::Open(EAppMsgType::YesNo, LevelInstanceBreakWarning) == EAppReturnType::Yes)
						{
							ELevelInstanceBreakFlags Flags = ELevelInstanceBreakFlags::None;
							if (Settings->bKeepFoldersDuringBreak)
							{
								Flags |= ELevelInstanceBreakFlags::KeepFolders;
							}

							for (ILevelInstanceInterface* LevelInstance : BreakableLevelInstances)
							{
								LevelInstanceSubsystem->BreakLevelInstance(LevelInstance, BreakLevels, nullptr, Flags);
							}
						}
					})
				),
				EUserInterfaceActionType::Button
			);

			Section.AddEntry(ExecuteEntry);
		}
	}

	void CreateBreakMenu(UToolMenu* Menu)
	{
		if(ULevelInstanceSubsystem* LevelInstanceSubsystem = GEditor->GetEditorWorldContext().World()->GetSubsystem<ULevelInstanceSubsystem>())
		{
			TArray<ILevelInstanceInterface*> BreakableLevelInstances;
			if (GEditor->GetSelectedActorCount() > 0)
			{
				for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
				{
					if (ILevelInstanceInterface* LevelInstance = Cast<ILevelInstanceInterface>(*It))
					{
						if (IsExperimentalSettingEnabled(LevelInstance) && !LevelInstanceSubsystem->IsEditingLevelInstance(LevelInstance) && !LevelInstanceSubsystem->LevelInstanceHasLevelScriptBlueprint(LevelInstance))
						{
							BreakableLevelInstances.Add(LevelInstance);
						}	
					}
				}
			}

			if (BreakableLevelInstances.Num())
			{
				FToolMenuSection& Section = CreateLevelSection(Menu);
				Section.AddSubMenu(
					"BreakLevelInstances",
					LOCTEXT("BreakLevelInstances", "Break..."),
					TAttribute<FText>(),
					FNewToolMenuDelegate::CreateStatic(&CreateBreakSubMenu, BreakableLevelInstances)
				);
			}
		}

	}

	void CreatePackedBlueprintMenu(UToolMenu* Menu, AActor* ContextActor)
	{
		if (ULevelInstanceSubsystem* LevelInstanceSubsystem = ContextActor->GetWorld()->GetSubsystem<ULevelInstanceSubsystem>())
		{
			ILevelInstanceInterface* ContextLevelInstance = nullptr;

			// Find the top level LevelInstance
			LevelInstanceSubsystem->ForEachLevelInstanceAncestorsAndSelf(ContextActor, [LevelInstanceSubsystem, ContextActor, &ContextLevelInstance](ILevelInstanceInterface* Ancestor)
			{
				if (CastChecked<AActor>(Ancestor)->GetLevel() == ContextActor->GetWorld()->GetCurrentLevel())
				{
					ContextLevelInstance = Ancestor;
					return false;
				}
				return true;
			});
						
			if (ContextLevelInstance && IsExperimentalSettingEnabled(ContextLevelInstance) && !ContextLevelInstance->IsEditing())
			{
				FToolMenuSection& Section = CreateLevelSection(Menu);
				;
				if (APackedLevelActor* PackedLevelActor = Cast<APackedLevelActor>(ContextLevelInstance))
				{
					if (TSoftObjectPtr<UBlueprint> BlueprintAsset = PackedLevelActor->GetClass()->ClassGeneratedBy.Get())
					{
						FToolUIAction UIAction;
						UIAction.ExecuteAction.BindLambda([ContextLevelInstance, BlueprintAsset](const FToolMenuContext& MenuContext)
						{
							FPackedLevelActorUtils::CreateOrUpdateBlueprint(ContextLevelInstance->GetWorldAsset(), BlueprintAsset);
						});
						UIAction.CanExecuteAction.BindLambda([](const FToolMenuContext& MenuContext)
						{
							return FPackedLevelActorUtils::CanPack() && GEditor->GetSelectedActorCount() > 0;
						});

						Section.AddMenuEntry(
							"UpdatePackedBlueprint",
							LOCTEXT("UpdatePackedBlueprint", "Update Packed Blueprint"),
							TAttribute<FText>(),
							TAttribute<FSlateIcon>(),
							UIAction);
					}
				}
			}
		}
	}

	class FLevelInstanceClassFilter : public IClassViewerFilter
	{
	public:
		
		virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
		{
			return InClass && InClass->ImplementsInterface(ULevelInstanceInterface::StaticClass()) && InClass->IsNative() && !InClass->HasAnyClassFlags(CLASS_Deprecated);
		}

		virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
		{
			return false;
		}
	};

	void CreateBlueprintFromWorld(UWorld* WorldAsset)
	{
		TSoftObjectPtr<UWorld> LevelInstancePtr(WorldAsset);

		int32 LastSlashIndex = 0;
		FString LongPackageName = LevelInstancePtr.GetLongPackageName();
		LongPackageName.FindLastChar('/', LastSlashIndex);
		
		FString PackagePath = LongPackageName.Mid(0, LastSlashIndex == INDEX_NONE ? MAX_int32 : LastSlashIndex);
		FString AssetName = "BP_" + LevelInstancePtr.GetAssetName();
		IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();

		UBlueprintFactory* BlueprintFactory = NewObject<UBlueprintFactory>();
		BlueprintFactory->AddToRoot();
		BlueprintFactory->OnConfigurePropertiesDelegate.BindLambda([](FClassViewerInitializationOptions* Options)
		{
			Options->bShowDefaultClasses = false;
			Options->bIsBlueprintBaseOnly = false;
			Options->InitiallySelectedClass = ALevelInstance::StaticClass();
			Options->bIsActorsOnly = true;
			Options->ClassFilters.Add(MakeShareable(new FLevelInstanceClassFilter));
		});
		ON_SCOPE_EXIT
		{
			BlueprintFactory->OnConfigurePropertiesDelegate.Unbind();
			BlueprintFactory->RemoveFromRoot();
		};

		if (UBlueprint* NewBlueprint = Cast<UBlueprint>(AssetTools.CreateAssetWithDialog(AssetName, PackagePath, UBlueprint::StaticClass(), BlueprintFactory, FName("Create LevelInstance Blueprint"))))
		{
			AActor* CDO = NewBlueprint->GeneratedClass->GetDefaultObject<AActor>();
			ILevelInstanceInterface* LevelInstanceCDO = CastChecked<ILevelInstanceInterface>(CDO);
			LevelInstanceCDO->SetWorldAsset(LevelInstancePtr);
			FBlueprintEditorUtils::MarkBlueprintAsModified(NewBlueprint);
			
			if (NewBlueprint->GeneratedClass->IsChildOf<APackedLevelActor>())
			{
				FPackedLevelActorUtils::UpdateBlueprint(NewBlueprint);
			}

			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			TArray<UObject*> Assets;
			Assets.Add(NewBlueprint);
			ContentBrowserModule.Get().SyncBrowserToAssets(Assets);
		}		
	}

	void CreateBlueprintFromMenu(UToolMenu* Menu, FAssetData WorldAsset)
	{
		FToolMenuSection& Section = CreateLevelSection(Menu);
		FToolUIAction UIAction;
		UIAction.ExecuteAction.BindLambda([WorldAsset](const FToolMenuContext& MenuContext)
		{
			if (UWorld* World = Cast<UWorld>(WorldAsset.GetAsset()))
			{
				CreateBlueprintFromWorld(World);
			}
		});

		Section.AddMenuEntry(
			"CreateLevelInstanceBlueprint",
			LOCTEXT("CreateLevelInstanceBlueprint", "New Blueprint..."),
			TAttribute<FText>(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.LevelInstance"),
			UIAction);
	}

	void AddPartitionedStreamingSupportFromWorld(UWorld* WorldAsset)
	{
		if (WorldAsset->GetStreamingLevels().Num())
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("AddPartitionedLevelInstanceStreamingSupportError_SubLevels", "Cannot convert this world has it contains sublevels."));
			return;
		}

		if (WorldAsset->WorldType != EWorldType::Inactive)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("AddPartitionedLevelInstanceStreamingSupportError_Loaded", "Cannot convert this world has it's already loaded in the editor."));
			return;
		}

		bool bSuccess = false;
		UWorld* World = GEditor->GetEditorWorldContext().World();
		ULevelInstanceSubsystem::ResetLoadersForWorldAsset(WorldAsset->GetPackage()->GetName());

		FWorldPartitionConverter::FParameters Parameters;
		Parameters.bConvertSubLevels = false;
		Parameters.bEnableStreaming = false;
		Parameters.bUseActorFolders = true;

		if (FWorldPartitionConverter::Convert(WorldAsset, Parameters))
		{
			TArray<UPackage*> PackagesToSave = WorldAsset->PersistentLevel->GetLoadedExternalObjectPackages();
			TSet<UPackage*> PackagesToSaveSet(PackagesToSave);

			PackagesToSaveSet.Add(WorldAsset->GetPackage());

			const bool bPromptUserToSave = false;
			const bool bSaveMapPackages = true;
			const bool bSaveContentPackages = true;
			const bool bFastSave = false;
			const bool bNotifyNoPackagesSaved = false;
			const bool bCanBeDeclined = true;

			if (FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages, bFastSave, bNotifyNoPackagesSaved, bCanBeDeclined, nullptr, [&PackagesToSaveSet](UPackage* PackageToSave) { return !PackagesToSaveSet.Contains(PackageToSave); }))
			{
				bSuccess = true;
				for (UPackage* PackageToSave : PackagesToSave)
				{
					if (PackageToSave->IsDirty())
					{
						UE_LOG(LogLevelInstanceEditor, Error, TEXT("Package '%s' failed to save"), *PackageToSave->GetName());
						bSuccess = false;
						break;
					}
				}
			}
		}

		if (!bSuccess)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("AddPartitionedLevelInstanceStreamingSupportError", "An error occured when adding partitioned level instance streaming support, check logs for details.."));
		}
	}

	void UpdatePackedBlueprintsFromMenu(UToolMenu* Menu, FAssetData WorldAsset)
	{
		FToolMenuSection& Section = CreateLevelSection(Menu);
		FToolUIAction UIAction;
		UIAction.CanExecuteAction.BindLambda([](const FToolMenuContext& MenuContext)
		{
			return FPackedLevelActorUtils::CanPack();
		});
		UIAction.ExecuteAction.BindLambda([WorldAsset](const FToolMenuContext& MenuContext)
		{
			FScopedSlowTask SlowTask(0.0f, LOCTEXT("UpdatePackedBlueprintsProgress", "Updating Packed Blueprints..."));
			TSet<TSoftObjectPtr<UBlueprint>> BlueprintAssets;
			FPackedLevelActorUtils::GetPackedBlueprintsForWorldAsset(TSoftObjectPtr<UWorld>(WorldAsset.GetSoftObjectPath()), BlueprintAssets, false);
			TSharedPtr<FPackedLevelActorBuilder> Builder = FPackedLevelActorBuilder::CreateDefaultBuilder();
			for (TSoftObjectPtr<UBlueprint> BlueprintAsset : BlueprintAssets)
			{
				if (UBlueprint* Blueprint = BlueprintAsset.Get())
				{
					Builder->UpdateBlueprint(Blueprint, false);
				}
			}
		});

		Section.AddMenuEntry(
			"UpdatePackedBlueprintsFromMenu",
			LOCTEXT("UpdatePackedBlueprintsFromMenu", "Update Packed Blueprints"),
			TAttribute<FText>(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.PackedLevelActor"),
			UIAction
		);
	}

	void AddPartitionedStreamingSupportFromMenu(UToolMenu* Menu, FAssetData WorldAsset)
	{
		FName WorldAssetName = WorldAsset.PackageName;
		if (!ULevel::GetIsLevelPartitionedFromPackage(WorldAssetName))
		{
			FToolMenuSection& Section = CreateLevelSection(Menu);
			FToolUIAction UIAction;
			UIAction.ExecuteAction.BindLambda([WorldAsset](const FToolMenuContext& MenuContext)
			{
				if (UWorld* World = Cast<UWorld>(WorldAsset.GetAsset()))
				{
					AddPartitionedStreamingSupportFromWorld(World);
				}
			});

			Section.AddMenuEntry(
				"AddPartitionedStreamingSupportFromMenu",
				LOCTEXT("AddPartitionedStreamingSupportFromMenu", "Add Partitioned Streaming Support"),
				TAttribute<FText>(),
				TAttribute<FSlateIcon>(),
				UIAction
			);
		}
	}
};

void FLevelInstanceEditorModule::StartupModule()
{
	ExtendContextMenu();

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout("LevelInstance", FOnGetDetailCustomizationInstance::CreateStatic(&FLevelInstanceActorDetails::MakeInstance));
	PropertyModule.RegisterCustomClassLayout("LevelInstancePivot", FOnGetDetailCustomizationInstance::CreateStatic(&FLevelInstancePivotDetails::MakeInstance));		
	PropertyModule.RegisterCustomPropertyTypeLayout("WorldPartitionActorFilter", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FLevelInstanceFilterPropertyTypeCustomization::MakeInstance, false), MakeShared<FLevelInstancePropertyTypeIdentifier>(false));
	PropertyModule.RegisterCustomPropertyTypeLayout("WorldPartitionActorFilter", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FLevelInstanceFilterPropertyTypeCustomization::MakeInstance, true), MakeShared<FLevelInstancePropertyTypeIdentifier>(true));
	PropertyModule.NotifyCustomizationModuleChanged();

	// GEditor needs to be set before this module is loaded
	check(GEditor);
	GEditor->OnLevelActorDeleted().AddRaw(this, &FLevelInstanceEditorModule::OnLevelActorDeleted);
	
	EditorLevelUtils::CanMoveActorToLevelDelegate.AddRaw(this, &FLevelInstanceEditorModule::CanMoveActorToLevel);

	// Register actor descriptor loading filter
	class FLevelInstanceActorDescFilter : public IWorldPartitionActorLoaderInterface::FActorDescFilter
	{
	public:
		bool PassFilter(class UWorld* InWorld, const FWorldPartitionHandle& InHandle) override
		{
			if (UWorld* OwningWorld = InWorld->PersistentLevel->GetWorld())
			{
				if (ULevelInstanceSubsystem* LevelInstanceSubsystem = OwningWorld->GetSubsystem<ULevelInstanceSubsystem>())
				{
					return LevelInstanceSubsystem->PassLevelInstanceFilter(InWorld, InHandle);
				}
			}

			return true;
		}

		// Leave [0, 19] for Game code
		virtual uint32 GetFilterPriority() const override { return 20; }

		virtual FText* GetFilterReason() const override
		{
			static FText UnloadedReason(LOCTEXT("LevelInstanceActorDescFilterReason", "Filtered"));
			return &UnloadedReason;
		}
	};
	IWorldPartitionActorLoaderInterface::RegisterActorDescFilter(MakeShareable<IWorldPartitionActorLoaderInterface::FActorDescFilter>(new FLevelInstanceActorDescFilter()));

	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	FMessageLogInitializationOptions InitOptions;
	InitOptions.bShowFilters = true;
	InitOptions.bShowPages = false;
	InitOptions.bAllowClear = true;
	MessageLogModule.RegisterLogListing("PackedLevelActor", LOCTEXT("PackedLevelActorLog", "Packed Level Actor Log"), InitOptions);
		
	FLevelInstanceEditorModeCommands::Register();

	if (!IsRunningCommandlet())
	{
		GLevelEditorModeTools().OnEditorModeIDChanged().AddRaw(this, &FLevelInstanceEditorModule::OnEditorModeIDChanged);
	}
}

void FLevelInstanceEditorModule::ShutdownModule()
{
	if (GEditor)
	{
		GEditor->OnLevelActorDeleted().RemoveAll(this);
	}

	EditorLevelUtils::CanMoveActorToLevelDelegate.RemoveAll(this);

	if (!IsRunningCommandlet() && GLevelEditorModeToolsIsValid())
	{
		GLevelEditorModeTools().OnEditorModeIDChanged().RemoveAll(this);
	}
}

void FLevelInstanceEditorModule::OnEditorModeIDChanged(const FEditorModeID& InModeID, bool bIsEnteringMode)
{
	if (InModeID == ULevelInstanceEditorMode::EM_LevelInstanceEditorModeId && !bIsEnteringMode)
	{
		ExitEditorModeEvent.Broadcast();
	}
}

void FLevelInstanceEditorModule::BroadcastTryExitEditorMode() 
{
	TryExitEditorModeEvent.Broadcast();
}

void FLevelInstanceEditorModule::ActivateEditorMode()
{
	if (!GLevelEditorModeTools().IsModeActive(ULevelInstanceEditorMode::EM_LevelInstanceEditorModeId))
	{
		GLevelEditorModeTools().ActivateMode(ULevelInstanceEditorMode::EM_LevelInstanceEditorModeId);
	}
}
void FLevelInstanceEditorModule::DeactivateEditorMode()
{
	if (GLevelEditorModeTools().IsModeActive(ULevelInstanceEditorMode::EM_LevelInstanceEditorModeId))
	{
		GLevelEditorModeTools().DeactivateMode(ULevelInstanceEditorMode::EM_LevelInstanceEditorModeId);
	}
}

void FLevelInstanceEditorModule::OnLevelActorDeleted(AActor* Actor)
{
	if (ULevelInstanceSubsystem* LevelInstanceSubsystem = Actor->GetWorld()->GetSubsystem<ULevelInstanceSubsystem>())
	{
		LevelInstanceSubsystem->OnActorDeleted(Actor);
	}
}

void FLevelInstanceEditorModule::CanMoveActorToLevel(const AActor* ActorToMove, const ULevel* DestLevel, bool& bOutCanMove)
{
	if (UWorld* World = ActorToMove->GetWorld())
	{
		if (ULevelInstanceSubsystem* LevelInstanceSubsystem = World->GetSubsystem<ULevelInstanceSubsystem>())
		{
			if (!LevelInstanceSubsystem->CanMoveActorToLevel(ActorToMove))
			{
				bOutCanMove = false;
				return;
			}
		}
	}
}

void FLevelInstanceEditorModule::ExtendContextMenu()
{
	if (UToolMenu* BuildMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Build"))
	{
		FToolMenuSection& Section = BuildMenu->AddSection("LevelEditorLevelInstance", LOCTEXT("PackedLevelActorsHeading", "Packed Level Actor"));
		FUIAction PackAction(
			FExecuteAction::CreateLambda([]() 
			{
				FPackedLevelActorUtils::PackAllLoadedActors();
			}), 
			FCanExecuteAction::CreateLambda([]()
			{
				return FPackedLevelActorUtils::CanPack();
			}),
			FIsActionChecked(),
			FIsActionButtonVisible::CreateLambda([]() 
			{
				return GetDefault<UEditorExperimentalSettings>()->bPackedLevelActor;
			}));

		FToolMenuEntry& Entry = Section.AddMenuEntry(NAME_None, LOCTEXT("PackLevelActorsTitle", "Pack Level Actors"),
			LOCTEXT("PackLevelActorsTooltip", "Update packed level actor blueprints"), FSlateIcon(), PackAction, EUserInterfaceActionType::Button);
	}

	auto AddDynamicSection = [](UToolMenu* ToolMenu)
	{				
		if (GEditor->GetPIEWorldContext())
		{
			return;
		}

		// Some actions aren't allowed on non root selection Level Instances (Readonly Level Instances)
		const bool bAreAllSelectedLevelInstancesRootSelections = LevelInstanceMenuUtils::AreAllSelectedLevelInstancesRootSelections();

		if (ULevelEditorContextMenuContext* LevelEditorMenuContext = ToolMenu->Context.FindContext<ULevelEditorContextMenuContext>())
		{
			// Use the actor under the cursor if available (e.g. right-click menu).
			// Otherwise use the first selected actor if there's one (e.g. Actor pulldown menu or outliner).
			AActor* ContextActor = LevelEditorMenuContext->HitProxyActor.Get();
			if (!ContextActor && GEditor->GetSelectedActorCount() != 0)
			{
				ContextActor = Cast<AActor>(GEditor->GetSelectedActors()->GetSelectedObject(0));
			}

			if (ContextActor)
			{
				// Allow Edit/Commmit on non root selected Level Instance
				LevelInstanceMenuUtils::CreateEditMenu(ToolMenu, ContextActor);
				LevelInstanceMenuUtils::CreateCommitDiscardMenu(ToolMenu, ContextActor);
				
				if (bAreAllSelectedLevelInstancesRootSelections)
				{
					LevelInstanceMenuUtils::CreatePackedBlueprintMenu(ToolMenu, ContextActor);
					LevelInstanceMenuUtils::CreateSetCurrentMenu(ToolMenu, ContextActor);
				}
			}

			if (bAreAllSelectedLevelInstancesRootSelections)
			{
				LevelInstanceMenuUtils::CreateMoveSelectionToMenu(ToolMenu);
			}
		}

		if (bAreAllSelectedLevelInstancesRootSelections)
		{
			LevelInstanceMenuUtils::CreateBreakMenu(ToolMenu);
			LevelInstanceMenuUtils::CreateCreateMenu(ToolMenu);
		}
	};

	if (UToolMenu* ToolMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.ActorContextMenu.LevelSubMenu"))
	{
		ToolMenu->AddDynamicSection("LevelInstanceEditorModuleDynamicSection", FNewToolMenuDelegate::CreateLambda(AddDynamicSection));
	}

	if (UToolMenu* ToolMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorSceneOutliner.ContextMenu.LevelSubMenu"))
	{
		ToolMenu->AddDynamicSection("LevelInstanceEditorModuleDynamicSection", FNewToolMenuDelegate::CreateLambda(AddDynamicSection));
	}
		
	if (UToolMenu* WorldAssetMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.World"))
	{
		FToolMenuSection& Section = WorldAssetMenu->AddDynamicSection("ActorLevelInstance", FNewToolMenuDelegate::CreateLambda([this](UToolMenu* ToolMenu)
		{
			if (GEditor->GetPIEWorldContext())
			{
				return;
			}

			if(!GetDefault<UEditorExperimentalSettings>()->bLevelInstance)
			{
				return;
			}

			if (ToolMenu)
			{
				if (UContentBrowserAssetContextMenuContext* AssetMenuContext = ToolMenu->Context.FindContext<UContentBrowserAssetContextMenuContext>())
				{
					if (AssetMenuContext->SelectedAssets.Num() != 1)
					{
						return;
					}

					const FAssetData& WorldAsset = AssetMenuContext->SelectedAssets[0];
					if (AssetMenuContext->SelectedAssets[0].IsInstanceOf<UWorld>())
					{
						LevelInstanceMenuUtils::CreateBlueprintFromMenu(ToolMenu, WorldAsset);
						LevelInstanceMenuUtils::UpdatePackedBlueprintsFromMenu(ToolMenu, WorldAsset);
						LevelInstanceMenuUtils::AddPartitionedStreamingSupportFromMenu(ToolMenu, WorldAsset);
					}
				}
			}
		}), FToolMenuInsert(NAME_None, EToolMenuInsertType::Default));
	}
}

bool FLevelInstanceEditorModule::IsEditInPlaceStreamingEnabled() const
{
	return GetDefault<ULevelInstanceEditorSettings>()->bIsEditInPlaceStreamingEnabled;
}

#undef LOCTEXT_NAMESPACE