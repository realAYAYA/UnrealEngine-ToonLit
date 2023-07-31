// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelInstancePivotDetails.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "SEnumCombo.h"
#include "ScopedTransaction.h"
#include "Engine/Brush.h"
#include "Engine/World.h"
#include "LevelInstance/LevelInstanceInterface.h"
#include "LevelInstance/LevelInstanceEditorPivotInterface.h"
#include "LevelInstance/LevelInstanceEditorPivot.h"

#define LOCTEXT_NAMESPACE "FLevelInstancePivotDetails"

FLevelInstancePivotDetails::FLevelInstancePivotDetails()
{
}

TSharedRef<IDetailCustomization> FLevelInstancePivotDetails::MakeInstance()
{
	return MakeShareable(new FLevelInstancePivotDetails);
}

void FLevelInstancePivotDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> EditingObjects;
	DetailBuilder.GetObjectsBeingCustomized(EditingObjects);

	if (EditingObjects.Num() > 1)
	{
		return;
	}

	TWeakObjectPtr<AActor> EditingObject = Cast<AActor>(EditingObjects[0].Get());

	UWorld* World = EditingObject->GetWorld();

	if (!World)
	{
		return;
	}

	PivotActor = nullptr;
	PivotType = ELevelInstancePivotType::CenterMinZ;
	PivotActors.Reset();
	for (AActor* Actor : EditingObject->GetLevel()->Actors)
	{
		if (Actor != nullptr && Actor != EditingObject && Actor->GetRootComponent() != nullptr && !Actor->IsA<ABrush>())
		{
			PivotActors.Add(MakeShareable(new FActorInfo{ Actor }));
		}
	}

	IDetailCategoryBuilder& ChangePivotCategory = DetailBuilder.EditCategory("Change Pivot", FText::GetEmpty(), ECategoryPriority::Important);
		
	ChangePivotCategory.AddCustomRow(LOCTEXT("PivotTypeRow", "Type"), false)
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PivotTypeText", "Type"))
		]
		.ValueContent()
		[
			SNew(SEnumComboBox, StaticEnum<ELevelInstancePivotType>())
			.ContentPadding(FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding"))
			.CurrentValue(this, &FLevelInstancePivotDetails::GetSelectedPivotType)
			.OnEnumSelectionChanged(this, &FLevelInstancePivotDetails::OnSelectedPivotTypeChanged)
		];

	ChangePivotCategory.AddCustomRow(LOCTEXT("PivotActorRow", "Actor"), false)
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PivotActorText", "Actor"))
		]
		.ValueContent()
		[
			SNew(SComboBox<TSharedPtr<FActorInfo>>)
			.ContentPadding(FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding"))
			.OptionsSource(&PivotActors)
			.OnGenerateWidget(this, &FLevelInstancePivotDetails::OnGeneratePivotActorWidget)
			.OnSelectionChanged(this, &FLevelInstancePivotDetails::OnSelectedPivotActorChanged)
			.IsEnabled(this, &FLevelInstancePivotDetails::IsPivotActorSelectionEnabled)
			[
				SNew(STextBlock)
				.Text(this, &FLevelInstancePivotDetails::GetSelectedPivotActorText)
			]
		];
			
	ChangePivotCategory.AddCustomRow(LOCTEXT("ApplyRow", "Apply"), false)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNew(SButton)
				.IsEnabled(this, &FLevelInstancePivotDetails::IsApplyButtonEnabled, EditingObject)
				.Text(LOCTEXT("ChangePivotButton", "Apply"))
				.HAlign(HAlign_Center)
				.OnClicked(this, &FLevelInstancePivotDetails::OnApplyButtonClicked, EditingObject)
			]
		];
}

bool FLevelInstancePivotDetails::IsApplyButtonEnabled(TWeakObjectPtr<AActor> LevelInstancePivot) const
{
	return PivotType != ELevelInstancePivotType::Actor || PivotActor.IsValid();
}

FReply FLevelInstancePivotDetails::OnApplyButtonClicked(TWeakObjectPtr<AActor> LevelInstancePivot)
{
	if (LevelInstancePivot.IsValid())
	{
		FScopedTransaction Transaction(LOCTEXT("ChangePivot", "Change Level Instance Pivot"));
		FLevelInstanceEditorPivotHelper::SetPivot(CastChecked<ILevelInstanceEditorPivotInterface>(LevelInstancePivot.Get()), PivotType, PivotActor.Get());
	}
	return FReply::Handled();
}

int32 FLevelInstancePivotDetails::GetSelectedPivotType() const
{
	return (int32)PivotType;
}

void FLevelInstancePivotDetails::OnSelectedPivotTypeChanged(int32 NewValue, ESelectInfo::Type SelectionType)
{
	PivotType = (ELevelInstancePivotType)NewValue;
}

TSharedRef<SWidget> FLevelInstancePivotDetails::OnGeneratePivotActorWidget(TSharedPtr<FActorInfo> ActorInfo) const
{
	AActor* Actor = ActorInfo->Actor.Get();

	// If a row wasn't generated just create the default one, a simple text block of the item's name.
	return SNew(STextBlock).Text(Actor ? FText::FromString(Actor->GetActorLabel()) : LOCTEXT("null", "null"));
}

FText FLevelInstancePivotDetails::GetSelectedPivotActorText() const
{
	return PivotActor.IsValid() ? FText::FromString(PivotActor->GetActorLabel()) : LOCTEXT("none", "None");
}

void FLevelInstancePivotDetails::OnSelectedPivotActorChanged(TSharedPtr<FActorInfo> NewValue, ESelectInfo::Type SelectionType)
{
	PivotActor = NewValue->Actor;
}

bool FLevelInstancePivotDetails::IsPivotActorSelectionEnabled() const
{
	return PivotType == ELevelInstancePivotType::Actor;
}



#undef LOCTEXT_NAMESPACE