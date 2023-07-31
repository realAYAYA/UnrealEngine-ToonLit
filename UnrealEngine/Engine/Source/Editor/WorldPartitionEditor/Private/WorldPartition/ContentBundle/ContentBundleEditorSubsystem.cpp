// Copyright Epic Games, Inc. All Rights Reserved.
#include "WorldPartition/ContentBundle/ContentBundleEditorSubsystem.h"

#include "WorldPartition/ContentBundle/ContentBundle.h"
#include "WorldPartition/ContentBundle/ContentBundleDescriptor.h"
#include "WorldPartition/ContentBundle/ContentBundle.h"
#include "WorldPartition/ContentBundle/ContentBundleEditor.h"
#include "WorldPartition/WorldPartition.h"
#include "Subsystems/ActorEditorContextSubsystem.h"
#include "Engine/Selection.h"
#include "WorldPartition/ContentBundle/ContentBundleWorldSubsystem.h"
#include "WorldPartition/ContentBundle/ContentBundleLog.h"
#include "Styling/AppStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Editor.h"

void UContentBundleEditionSubmodule::DoInitialize()
{
	UActorEditorContextSubsystem::Get()->RegisterClient(this);
}

void UContentBundleEditionSubmodule::DoDenitialize()
{
	UActorEditorContextSubsystem::Get()->UnregisterClient(this);

	EditingContentBundle = nullptr;
	ActiveContentBundlesStack.Empty();
}

void UContentBundleEditionSubmodule::OnExecuteActorEditorContextAction(UWorld* InWorld, const EActorEditorContextAction& InType, AActor* InActor /* = nullptr */)
{
	TSharedPtr<FContentBundleEditor> ContentBundleEditorPin = EditingContentBundle.Pin();

	switch (InType)
	{
	case EActorEditorContextAction::ApplyContext:
		if (ContentBundleEditorPin != nullptr)
		{
			ContentBundleEditorPin->AddActor(InActor);
		}
		break;
	case EActorEditorContextAction::ResetContext:
		break;
	case EActorEditorContextAction::PushContext:
		PushContentBundleEditing();
		break;
	case EActorEditorContextAction::PopContext:
		PopContentBundleEditing();
		break;
	}
}

bool UContentBundleEditionSubmodule::GetActorEditorContextDisplayInfo(UWorld* InWorld, FActorEditorContextClientDisplayInfo& OutDiplayInfo) const
{
	if (EditingContentBundle != nullptr)
	{
		constexpr TCHAR PLACEHOLDER_ContentBundle_Brush[] = TEXT("DataLayer.Editor"); // todo_ow: create placeholder brush for content bundle
		OutDiplayInfo.Title = TEXT("Content Bundle");
		OutDiplayInfo.Brush = FAppStyle::GetBrush(PLACEHOLDER_ContentBundle_Brush);
		return true;
	}

	return false;
}

bool UContentBundleEditionSubmodule::CanResetContext(UWorld* InWorld) const
{
	return false;
}

TSharedRef<SWidget> UContentBundleEditionSubmodule::GetActorEditorContextWidget(UWorld* World) const
{
	constexpr TCHAR PLACEHOLDER_ContentBundle_ColorIcon[] = TEXT("DataLayer.ColorIcon"); // todo_ow: create placeholder color icon for content bundle

	TSharedRef<SVerticalBox> OutWidget = SNew(SVerticalBox);

	if (EditingContentBundle != nullptr)
	{
		OutWidget->AddSlot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 1.0f, 1.0f, 1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush(PLACEHOLDER_ContentBundle_ColorIcon))
				.DesiredSizeOverride(FVector2D(8, 8))
			]
		+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f, 1.0f, 1.0f, 1.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					TSharedPtr<FContentBundleEditor> ContentBundleEditorPin = EditingContentBundle.Pin();
					if (ContentBundleEditorPin != nullptr)
					{
						return FText::FromString(ContentBundleEditorPin->GetDisplayName());
					}

					return FText::FromString(TEXT("Unset Display Name"));
				})
					
			]
		];
	}


	return OutWidget;
}

bool UContentBundleEditionSubmodule::ActivateContentBundleEditing(TSharedPtr<FContentBundleEditor>& ContentBundleEditor)
{
	EContentBundleStatus ContentBundleStatus = ContentBundleEditor->GetStatus();
	bool bCanActivateEditing = ContentBundleStatus == EContentBundleStatus::ContentInjected || ContentBundleStatus == EContentBundleStatus::ReadyToInject;
	if (bCanActivateEditing && !ContentBundleEditor->IsBeingEdited())
	{
		if (IsEditingContentBundle())
		{
			check(!IsEditingContentBundle(ContentBundleEditor.Get()));
			DeactivateCurrentContentBundleEditing();
		}

		EditingContentBundle = ContentBundleEditor;

		ContentBundleEditor->StartEditing();

		ActorEditorContextClientChanged.Broadcast(this);

		UE_LOG(LogContentBundle, Log, TEXT("[CB: %s] Content Bundle is being edited"), *ContentBundleEditor->GetDescriptor()->GetDisplayName());

		GetSubsystem()->NotifyContentBundleChanged(ContentBundleEditor.Get());

		return true;
	}
	return false;
}

bool UContentBundleEditionSubmodule::DeactivateContentBundleEditing(TSharedPtr<FContentBundleEditor>& ContentBundleEditor)
{
	if (IsEditingContentBundle(ContentBundleEditor.Get()))
	{
		DeactivateCurrentContentBundleEditing();
		return true;
	}

	return false;
}

bool UContentBundleEditionSubmodule::DeactivateCurrentContentBundleEditing()
{
	if (TSharedPtr<FContentBundleEditor> ContentBundleEditorPin = EditingContentBundle.Pin())
	{
		ContentBundleEditorPin->StopEditing();
		EditingContentBundle = nullptr;
		ActorEditorContextClientChanged.Broadcast(this);

		GetSubsystem()->NotifyContentBundleChanged(ContentBundleEditorPin.Get());

		UE_LOG(LogContentBundle, Log, TEXT("[CB: %s] Content Bundle is no longer being edited"), *ContentBundleEditorPin->GetDescriptor()->GetDisplayName());

		return true;
	}

	return false;
}

void UContentBundleEditionSubmodule::PushContentBundleEditing()
{
	ActiveContentBundlesStack.Add(EditingContentBundle);
	DeactivateCurrentContentBundleEditing();
}

void UContentBundleEditionSubmodule::PopContentBundleEditing()
{
	DeactivateCurrentContentBundleEditing();

	TSharedPtr<FContentBundleEditor> ContentBundleToActivate = ActiveContentBundlesStack.Pop().Pin();
	if (ContentBundleToActivate != nullptr)
	{
		ActivateContentBundleEditing(ContentBundleToActivate);
	}
}

UContentBundleEditorSubsystem::UContentBundleEditorSubsystem()
{
	
}

void UContentBundleEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UActorEditorContextSubsystem>();

	ContentBundleEditionSubModule = NewObject<UContentBundleEditionSubmodule>(this);
	ContentBundleEditionSubModule->Initialize();

	IContentBundleEditorSubsystemInterface::SetInstance(this);
}

void UContentBundleEditorSubsystem::Deinitialize()
{
	IContentBundleEditorSubsystemInterface::SetInstance(nullptr);

	ContentBundleEditionSubModule->Deinitialize();
	ContentBundleEditionSubModule = nullptr;
}

UWorld* UContentBundleEditorSubsystem::GetWorld() const
{
	return GEditor->GetEditorWorldContext().World();
}

void UContentBundleEditorSubsystem::NotifyContentBundleInjectedContent(const FContentBundleEditor* ContentBundle)
{
	NotifyContentBundleChanged(ContentBundle);
}

void UContentBundleEditorSubsystem::NotifyContentBundleRemovedContent(const FContentBundleEditor* ContentBundle)
{
	if (ContentBundle->IsBeingEdited())
	{
		GetEditionSubmodule()->DeactivateCurrentContentBundleEditing();
	}

	NotifyContentBundleChanged(ContentBundle);
}

TSharedPtr<FContentBundleEditor> UContentBundleEditorSubsystem::GetEditorContentBundleForActor(const AActor* Actor)
{
	for (TSharedPtr<FContentBundleEditor>& ContentBundle : GetEditorContentBundles())
	{
		if (ContentBundle->ContainsActor(Actor))
		{
			return ContentBundle;
		}
	}

	return nullptr;
}

TArray<TSharedPtr<FContentBundleEditor>> UContentBundleEditorSubsystem::GetEditorContentBundles()
{
	TArray<TSharedPtr<FContentBundleEditor>> EditorContentBundles;
	if (UContentBundleManager* ContentBundleManager = GetWorld()->ContentBundleManager)
	{
		ContentBundleManager->GetEditorContentBundle(EditorContentBundles);
	}

	return EditorContentBundles;
}

TSharedPtr<FContentBundleEditor> UContentBundleEditorSubsystem::GetEditorContentBundle(const UContentBundleDescriptor* ContentBundleDescriptor) const
{
	if (UContentBundleManager* ContentBundleManager = GetWorld()->ContentBundleManager)
	{
		return ContentBundleManager->GetEditorContentBundle(ContentBundleDescriptor, GetWorld());
	}

	return nullptr;
}

bool UContentBundleEditorSubsystem::HasContentBundle(const UContentBundleDescriptor* ContentBundleDescriptor) const
{
	return GetEditorContentBundle(ContentBundleDescriptor) != nullptr;
}

void UContentBundleEditorSubsystem::SelectActors(FContentBundleEditor& EditorContentBundle)
{
	SelectActorsInternal(EditorContentBundle, true);
}

void UContentBundleEditorSubsystem::DeselectActors(FContentBundleEditor& EditorContentBundle)
{
	SelectActorsInternal(EditorContentBundle, false);
}

void UContentBundleEditorSubsystem::ReferenceAllActors(FContentBundleEditor& EditorContentBundle)
{
	EditorContentBundle.ReferenceAllActors();
}

void UContentBundleEditorSubsystem::UnreferenceAllActors(FContentBundleEditor& EditorContentBundle)
{
	EditorContentBundle.UnreferenceAllActors();
}

bool UContentBundleEditorSubsystem::ActivateContentBundleEditing(TSharedPtr<FContentBundleEditor>& ContentBundleEditor)  const
{
	return ContentBundleEditionSubModule->ActivateContentBundleEditing(ContentBundleEditor);
}

bool UContentBundleEditorSubsystem::DectivateContentBundleEditing(TSharedPtr<FContentBundleEditor>& ContentBundleEditor) const
{
	return ContentBundleEditionSubModule->DeactivateContentBundleEditing(ContentBundleEditor);
}

bool UContentBundleEditorSubsystem::IsContentBundleEditingActivated(TSharedPtr<FContentBundleEditor>& ContentBundleEditor) const
{
	return ContentBundleEditionSubModule->IsEditingContentBundle(ContentBundleEditor.Get());
}

void UContentBundleEditorSubsystem::SelectActorsInternal(FContentBundleEditor& EditorContentBundle, bool bSelect)
{
	GEditor->GetSelectedActors()->BeginBatchSelectOperation();
	{
		TArray<AActor*> ContentBundleActors;
		EditorContentBundle.GetActors(ContentBundleActors);

		for (AActor* Actor : ContentBundleActors)
		{
			bool bNotifyForActor = false;
			GEditor->GetSelectedActors()->Modify();
			GEditor->SelectActor(Actor, bSelect, true, false);
		}
	}
	GEditor->GetSelectedActors()->EndBatchSelectOperation();

	GEditor->NoteSelectionChange();
}