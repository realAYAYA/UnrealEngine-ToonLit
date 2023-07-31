// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelInstanceActorDetails.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "ScopedTransaction.h"

#include "Engine/World.h"
#include "LevelInstance/LevelInstanceActor.h"

#define LOCTEXT_NAMESPACE "FLevelInstanceActorDetails"

namespace LevelInstanceActorDetailsCallbacks
{
	static bool IsEditCommitButtonEnabled(TWeakObjectPtr<ALevelInstance> LevelInstanceActorPtr)
	{
		if (ALevelInstance* LevelInstanceActor = LevelInstanceActorPtr.Get())
		{
			return LevelInstanceActor->CanEnterEdit() || LevelInstanceActor->CanExitEdit();
		}

		return false;
	}

	static FText GetEditCommitButtonText(TWeakObjectPtr<ALevelInstance> LevelInstanceActorPtr)
	{
		if (ALevelInstance* LevelInstanceActor = LevelInstanceActorPtr.Get())
		{
			if (LevelInstanceActor->CanExitEdit())
			{
				return LOCTEXT("CommitChanges", "Commit Changes");
			}
		}
		
		return LOCTEXT("Edit", "Edit");
	}

	static FText GetEditCommitReasonText(TWeakObjectPtr<ALevelInstance> LevelInstanceActorPtr)
	{
		FText Reason;
		if (ALevelInstance* LevelInstanceActor = LevelInstanceActorPtr.Get())
		{
			if (!LevelInstanceActor->IsEditing())
			{
				LevelInstanceActor->CanEnterEdit(&Reason);
				return Reason;
			}

			LevelInstanceActor->CanExitEdit(/*bDiscardEdits=*/false, &Reason);
		}
		return Reason;
	}

	static EVisibility GetEditCommitReasonVisibility(TWeakObjectPtr<ALevelInstance> LevelInstanceActorPtr)
	{
		if (ALevelInstance* LevelInstanceActor = LevelInstanceActorPtr.Get())
		{
			return IsEditCommitButtonEnabled(LevelInstanceActor) ? EVisibility::Collapsed : EVisibility::Visible;
		}

		return EVisibility::Collapsed;
	}

	static FReply OnEditCommitButtonClicked(TWeakObjectPtr<ALevelInstance> LevelInstanceActorPtr)
	{
		if (ALevelInstance* LevelInstanceActor = LevelInstanceActorPtr.Get())
		{
			if (LevelInstanceActor->CanExitEdit())
			{
				LevelInstanceActor->ExitEdit();
			}
			else if (LevelInstanceActor->CanEnterEdit())
			{
				LevelInstanceActor->EnterEdit();
			}
		}
		return FReply::Handled();
	}
}

FLevelInstanceActorDetails::FLevelInstanceActorDetails()
{
}

TSharedRef<IDetailCustomization> FLevelInstanceActorDetails::MakeInstance()
{
	return MakeShareable(new FLevelInstanceActorDetails);
}

void FLevelInstanceActorDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> EditingObjects;
	DetailBuilder.GetObjectsBeingCustomized(EditingObjects);

	if (EditingObjects.Num() > 1)
	{
		return;
	}

	TWeakObjectPtr<ALevelInstance> EditingObject = Cast<ALevelInstance>(EditingObjects[0].Get());
	UWorld* World = EditingObject->GetWorld();

	if (!World)
	{
		return;
	}

	IDetailCategoryBuilder& LevelInstanceEditingCategory = DetailBuilder.EditCategory("Level Edit", FText::GetEmpty(), ECategoryPriority::Transform);

	LevelInstanceEditingCategory.AddCustomRow(FText::GetEmpty())
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNew(SMultiLineEditableTextBox)
				.Visibility_Static(&LevelInstanceActorDetailsCallbacks::GetEditCommitReasonVisibility, EditingObject)
				.Font(DetailBuilder.GetDetailFontBold())
				.BackgroundColor(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateLambda([]() { return FAppStyle::GetColor("ErrorReporting.WarningBackgroundColor"); })))
				.Text_Static(&LevelInstanceActorDetailsCallbacks::GetEditCommitReasonText, EditingObject)
				.AutoWrapText(true)
				.IsReadOnly(true)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNew(SButton)
				.IsEnabled_Static(&LevelInstanceActorDetailsCallbacks::IsEditCommitButtonEnabled, EditingObject)
				.Text_Static(&LevelInstanceActorDetailsCallbacks::GetEditCommitButtonText, EditingObject)
				.HAlign(HAlign_Center)
				.OnClicked_Static(&LevelInstanceActorDetailsCallbacks::OnEditCommitButtonClicked, EditingObject)
			]
		]
	];
}



#undef LOCTEXT_NAMESPACE