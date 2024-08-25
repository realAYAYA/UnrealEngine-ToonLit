// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelInstanceActorDetails.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "UObject/WeakInterfacePtr.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "ScopedTransaction.h"

#include "Engine/World.h"
#include "LevelInstance/LevelInstanceInterface.h"

#define LOCTEXT_NAMESPACE "FLevelInstanceActorDetails"

namespace LevelInstanceActorDetailsCallbacks
{
	static bool IsEditCommitButtonEnabled(TWeakInterfacePtr<ILevelInstanceInterface> LevelInstancePtr)
	{
		if (ILevelInstanceInterface* LevelInstance = LevelInstancePtr.Get())
		{
			return LevelInstance->CanEnterEdit() || LevelInstance->CanExitEdit();
		}

		return false;
	}

	static FText GetEditCommitButtonText(TWeakInterfacePtr<ILevelInstanceInterface> LevelInstancePtr)
	{
		if (ILevelInstanceInterface* LevelInstance = LevelInstancePtr.Get())
		{
			if (LevelInstance->CanExitEdit())
			{
				return LOCTEXT("CommitChanges", "Commit Changes");
			}
		}
		
		return LOCTEXT("Edit", "Edit");
	}

	static FText GetEditCommitReasonText(TWeakInterfacePtr<ILevelInstanceInterface> LevelInstancePtr)
	{
		FText Reason;
		if (ILevelInstanceInterface* LevelInstance = LevelInstancePtr.Get())
		{
			if (!LevelInstance->IsEditing())
			{
				LevelInstance->CanEnterEdit(&Reason);
				return Reason;
			}

			LevelInstance->CanExitEdit(/*bDiscardEdits=*/false, &Reason);
		}
		return Reason;
	}

	static EVisibility GetEditCommitReasonVisibility(TWeakInterfacePtr<ILevelInstanceInterface> LevelInstancePtr)
	{
		if (ILevelInstanceInterface* LevelInstance = LevelInstancePtr.Get())
		{
			return IsEditCommitButtonEnabled(LevelInstance) ? EVisibility::Collapsed : EVisibility::Visible;
		}

		return EVisibility::Collapsed;
	}

	static FReply OnEditCommitButtonClicked(TWeakInterfacePtr<ILevelInstanceInterface> LevelInstancePtr)
	{
		if (ILevelInstanceInterface* LevelInstance = LevelInstancePtr.Get())
		{
			if (LevelInstance->CanExitEdit())
			{
				LevelInstance->ExitEdit();
			}
			else if (LevelInstance->CanEnterEdit())
			{
				LevelInstance->EnterEdit();
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

	TWeakInterfacePtr<ILevelInstanceInterface> LevelInstance = Cast<ILevelInstanceInterface>(EditingObjects[0].Get());
	UWorld* World = LevelInstance.GetObject()->GetWorld();

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
				.Visibility_Static(&LevelInstanceActorDetailsCallbacks::GetEditCommitReasonVisibility, LevelInstance)
				.Font(DetailBuilder.GetDetailFontBold())
				.BackgroundColor(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateLambda([]() { return FAppStyle::GetColor("ErrorReporting.WarningBackgroundColor"); })))
				.Text_Static(&LevelInstanceActorDetailsCallbacks::GetEditCommitReasonText, LevelInstance)
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
				.IsEnabled_Static(&LevelInstanceActorDetailsCallbacks::IsEditCommitButtonEnabled, LevelInstance)
				.Text_Static(&LevelInstanceActorDetailsCallbacks::GetEditCommitButtonText, LevelInstance)
				.HAlign(HAlign_Center)
				.OnClicked_Static(&LevelInstanceActorDetailsCallbacks::OnEditCommitButtonClicked, LevelInstance)
			]
		]
	];
}



#undef LOCTEXT_NAMESPACE