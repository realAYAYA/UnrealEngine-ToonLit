// Copyright Epic Games, Inc. All Rights Reserved.

#include "Views/Widgets/ObjectMixerEditorListMenuContext.h"

#include "Views/MainPanel/ObjectMixerEditorMainPanel.h"

#include "Elements/Framework/TypedElementSelectionSet.h"
#include "Elements/Interfaces/TypedElementObjectInterface.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "LevelEditor.h"
#include "LevelEditorContextMenu.h"
#include "LevelEditorMenuContext.h"
#include "LevelEditorViewport.h"
#include "Selection.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "ToolMenuContext.h"
#include "ToolMenuDelegates.h"
#include "ToolMenuEntry.h"
#include "ToolMenuSection.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SEditableTextBox.h"

#define LOCTEXT_NAMESPACE "ObjectMixerEditorListMenuContext"

FName UObjectMixerEditorListMenuContext::DefaultContextBaseMenuName("ObjectMixer.ContextMenuBase");

TSharedPtr<SWidget> UObjectMixerEditorListMenuContext::CreateContextMenu(const FObjectMixerEditorListMenuContextData InData)
{
	if (InData.SelectedItems.Num() == 0)
	{
		return nullptr;
	}

	return BuildContextMenu(InData);
}

TSharedPtr<SWidget> UObjectMixerEditorListMenuContext::BuildContextMenu(const FObjectMixerEditorListMenuContextData& InData)
{
	FLevelEditorContextMenu::RegisterComponentContextMenu();
	FLevelEditorContextMenu::RegisterActorContextMenu();
	FLevelEditorContextMenu::RegisterElementContextMenu();
	RegisterObjectMixerContextMenu();

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<ILevelEditor> LevelEditorPtr = LevelEditorModule.GetLevelEditorInstance().Pin();
	check(LevelEditorPtr);

	TSharedPtr<FUICommandList> LevelEditorActionsList = LevelEditorPtr->GetLevelEditorActions();
	FToolMenuContext Context;
	Context.AppendCommandList(LevelEditorActionsList);

	ULevelEditorContextMenuContext* LevelEditorContextObject = NewObject<ULevelEditorContextMenuContext>();
	LevelEditorContextObject->LevelEditor = LevelEditorPtr;
	LevelEditorContextObject->ContextType = ELevelEditorMenuContext::SceneOutliner;
	LevelEditorContextObject->CurrentSelection = LevelEditorPtr->GetElementSelectionSet();
	
	for (FSelectedEditableComponentIterator It(GEditor->GetSelectedEditableComponentIterator()); It; ++It)
	{
		LevelEditorContextObject->SelectedComponents.Add(CastChecked<UActorComponent>(*It));
	}
	
	Context.AddObject(LevelEditorContextObject, [](UObject* InContext)
	{
		ULevelEditorContextMenuContext* CastContext = CastChecked<ULevelEditorContextMenuContext>(InContext);
		CastContext->CurrentSelection = nullptr;
		CastContext->HitProxyElement.Release();
	});

	UObjectMixerEditorListMenuContext* ObjectMixerContextObject = NewObject<UObjectMixerEditorListMenuContext>();
	ObjectMixerContextObject->Data = InData;

	Context.AddObject(ObjectMixerContextObject, [](UObject* InContext)
	{
		UObjectMixerEditorListMenuContext* CastContext = CastChecked<UObjectMixerEditorListMenuContext>(InContext);
		CastContext->Data.SelectedItems.Empty();
		CastContext->Data.MainPanelPtr.Reset();
	});

	if (LevelEditorPtr->GetElementSelectionSet()->GetSelectedObjects<UActorComponent>().Num() == 0 &&
		LevelEditorPtr->GetElementSelectionSet()->GetSelectedObjects<AActor>().Num() > 0)
	{
		const TArray<AActor*> SelectedActors = LevelEditorPtr->GetElementSelectionSet()->GetSelectedObjects<AActor>();
	
		// Get all menu extenders for this context menu from the level editor module
		TArray<FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors> MenuExtenderDelegates = LevelEditorModule.GetAllLevelViewportContextMenuExtenders();
		TArray<TSharedPtr<FExtender>> Extenders;
		for (int32 i = 0; i < MenuExtenderDelegates.Num(); ++i)
		{
			if (MenuExtenderDelegates[i].IsBound())
			{
				Extenders.Add(MenuExtenderDelegates[i].Execute(LevelEditorActionsList.ToSharedRef(), SelectedActors));
			}
		}
	
		if (Extenders.Num() > 0)
		{
			Context.AddExtender(FExtender::Combine(Extenders));
		}
	}

	return UToolMenus::Get()->GenerateWidget("LevelEditor.ActorContextMenu", Context);
}

void UObjectMixerEditorListMenuContext::RegisterObjectMixerContextMenu()
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	
	if (ToolMenus->IsMenuRegistered("LevelEditor.ActorContextMenu"))
	{
		UToolMenu* Menu = ToolMenus->ExtendMenu("LevelEditor.ActorContextMenu");
	
		Menu->AddDynamicSection("DynamicCollectionsSection", FNewToolMenuDelegate::CreateLambda(
			[](UToolMenu* InMenu)
			{
				UObjectMixerEditorListMenuContext* Context = InMenu->FindContext<UObjectMixerEditorListMenuContext>();
				if (!Context || Context->Data.SelectedItems.Num() == 0)
				{
					return;
				}
	
				FToolMenuSection& Section = InMenu->FindOrAddSection("ObjectMixerCollections");
				Section.Label = LOCTEXT("ObjectMixerCollectionsSectionName", "Mixer Collections");
				
				FObjectMixerEditorListMenuContextData& ContextData = Context->Data;
			
				Section.AddSubMenu(
					"SelectCollectionsSubMenu",
					LOCTEXT("SelectCollectionsSubmenu", "Select or Add Collection"),
					LOCTEXT("SelectCollectionsSubmenu_Tooltip", "Select the collection to which you wish to assign this object."),
					FNewToolMenuDelegate::CreateLambda(
						[ContextData](UToolMenu* Menu)
						{
							FToolMenuEntry Args;
							Args.Type = EMultiBlockType::Widget;
							Args.MakeCustomWidget.BindLambda(
								[ContextData](const FToolMenuContext&, const FToolMenuCustomWidgetContext&)
								{
									return SNew(SBox)
											.MinDesiredWidth(200)
											.Padding(8, 0)
											[
												SNew(SEditableTextBox)
												.HintText(LOCTEXT("NewCollectionEditableHintText", "Enter a new collection name..."))
												.OnTextCommitted_Static(&UObjectMixerEditorListMenuContext::OnTextCommitted, ContextData)
											]
									;
								}
							);
							Menu->AddMenuEntry("NewCollectionInput", Args);
							
							if (const TSharedPtr<FObjectMixerEditorMainPanel> MainPanel = ContextData.MainPanelPtr.Pin())
							{
								if (TArray<FName> Collections = MainPanel->GetAllCollectionNames(); Collections.Num() > 0)
								{
									FToolMenuSection& Section = Menu->FindOrAddSection("Collections");
								   Section.Label = LOCTEXT("CollectionsSectionName", "Collections");

								   Collections.StableSort([](const FName& A, const FName B)
								   {
									   return A.LexicalLess(B);
								   });
								
								   for (const FName& Key : Collections)
								   {
									   const FText KeyText = FText::FromName(Key);
									
									   Section.AddMenuEntry(
										   Key,
										   KeyText,
										   FText::Format(LOCTEXT("AddObjectsToCollectionTooltipFormat", "Add selected to collection '{0}'"), KeyText),
										   FSlateIcon(),
										   FUIAction(
											   FExecuteAction::CreateStatic(&UObjectMixerEditorListMenuContext::OnClickCollectionMenuEntry, Key, ContextData),
											   FCanExecuteAction(),
											   FIsActionChecked::CreateStatic(&UObjectMixerEditorListMenuContext::AreAllObjectsInCollection, Key, ContextData)
										   ),
										   EUserInterfaceActionType::ToggleButton
									   );
								   }
								}
							}
						}
					)
				);
			}),
			FToolMenuInsert(NAME_None,EToolMenuInsertType::First)
		);
	}
}

void UObjectMixerEditorListMenuContext::OnTextCommitted(const FText& InText, ETextCommit::Type InCommitType, const FObjectMixerEditorListMenuContextData ContextData)
{
	if (InCommitType == ETextCommit::OnEnter)
	{
		AddObjectsToCollection(*InText.ToString(), ContextData);
	}
}

void UObjectMixerEditorListMenuContext::OnClickCollectionMenuEntry(const FName Key, const FObjectMixerEditorListMenuContextData ContextData)
{
	if (AreAllObjectsInCollection(Key, ContextData))
	{
		RemoveObjectsFromCollection(Key, ContextData);
	}
	else
	{
		AddObjectsToCollection(Key, ContextData);
	}
}

void UObjectMixerEditorListMenuContext::AddObjectsToCollection(const FName Key, const FObjectMixerEditorListMenuContextData ContextData)
{
	if (const TSharedPtr<FObjectMixerEditorMainPanel> MainPanel = ContextData.MainPanelPtr.Pin())
	{
		TSet<FSoftObjectPath> ObjectPaths;

		for (const TSharedPtr<FObjectMixerEditorListRow>& Item : ContextData.SelectedItems)
		{
			if (const UObject* Object = Item->GetObject())
			{
				ObjectPaths.Add(Object);
			}
		}
		
		MainPanel->RequestAddObjectsToCollection(Key, ObjectPaths);
	}
}

void UObjectMixerEditorListMenuContext::RemoveObjectsFromCollection(const FName Key, const FObjectMixerEditorListMenuContextData ContextData)
{
	if (const TSharedPtr<FObjectMixerEditorMainPanel> MainPanel = ContextData.MainPanelPtr.Pin())
	{
		TSet<FSoftObjectPath> ObjectPaths;

		for (const TSharedPtr<FObjectMixerEditorListRow>& Item : ContextData.SelectedItems)
		{
			if (const UObject* Object = Item->GetObject())
			{
				ObjectPaths.Add(Object);
			}
		}
		
		MainPanel->RequestRemoveObjectsFromCollection(Key, ObjectPaths);
	}
}

bool UObjectMixerEditorListMenuContext::AreAllObjectsInCollection(const FName Key, const FObjectMixerEditorListMenuContextData ContextData)
{
	bool bAreAllSelectedObjectsInCollection = false;

	if (const TSharedPtr<FObjectMixerEditorMainPanel> MainPanel = ContextData.MainPanelPtr.Pin())
	{
		for (const TSharedPtr<FObjectMixerEditorListRow>& Item : ContextData.SelectedItems)
		{
			if (const UObject* Object = Item->GetObject())
			{
				bAreAllSelectedObjectsInCollection = MainPanel->IsObjectInCollection(Key, Object);

				if (!bAreAllSelectedObjectsInCollection)
				{
					return false;
				}
			}
		}
	}

	return bAreAllSelectedObjectsInCollection;
}

#undef LOCTEXT_NAMESPACE
