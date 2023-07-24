// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementCounterWidgetConstructor.h"

#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Elements/Columns/TypedElementValueCacheColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Framework/Text/TextLayout.h"
#include "Layout/Margin.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "TypedElementUI_CounterWidget"

void FTypedElementCounterWidgetConstructor::Register(
	ITypedElementDataStorageInterface& DataStorage, ITypedElementDataStorageUiInterface& DataStorageUi)
{
	using namespace TypedElementQueryBuilder;

	FName Purpose("LevelEditor.StatusBar.ToolBar");

	TUniquePtr<FTypedElementCounterWidgetConstructor> ActorCounter = MakeUnique<FTypedElementCounterWidgetConstructor>();
	ActorCounter->LabelText = LOCTEXT("ActorCounterStatusBarLabel", "{0} {0}|plural(one=Actor, other=Actors)");
	ActorCounter->ToolTipText = LOCTEXT(
		"ActorCounterStatusBarToolTip",
		"The total number of actors currently in the editor, excluding PIE/SIE and previews.");
	ActorCounter->Query = DataStorage.RegisterQuery(
		Count().
		Where().
		All("/Script/MassActors.MassActorFragment"_Type).
		Compile());
	DataStorageUi.RegisterWidgetFactory(Purpose, MoveTemp(ActorCounter));

	TUniquePtr<FTypedElementCounterWidgetConstructor> WidgetCounter = MakeUnique<FTypedElementCounterWidgetConstructor>();
	WidgetCounter->LabelText = LOCTEXT("WidgetCounterStatusBarLabel", "{0} {0}|plural(one=Widget, other=Widgets)");
	WidgetCounter->ToolTipText = LOCTEXT(
		"WidgetCounterStatusBarToolTip",
		"The total number of widgets in the editor hosted through the Typed Element's Data Storage.");
	WidgetCounter->Query = DataStorage.RegisterQuery(
		Count().
		Where().
		All<FTypedElementSlateWidgetReferenceColumn>().
		Compile());
	DataStorageUi.RegisterWidgetFactory(Purpose, MoveTemp(WidgetCounter));
}

TSharedPtr<SWidget> FTypedElementCounterWidgetConstructor::CreateWidget()
{
	return SNew(STextBlock)
		.Text(FText::Format(LabelText, 0))
		.Margin(FMargin(4.0f, 0.0f))
		.Justification(ETextJustify::Center);
}

void FTypedElementCounterWidgetConstructor::AddColumns(
	ITypedElementDataStorageInterface* DataStorage, TypedElementRowHandle Row, const TSharedPtr<SWidget>& Widget)
{
	Super::AddColumns(DataStorage, Row, Widget);

	FTypedElementCounterWidgetColumn* CounterColumn = DataStorage->AddOrGetColumn<FTypedElementCounterWidgetColumn>(Row);
	checkf(CounterColumn, TEXT("Added a new FTypedElementCounterWidgetColumn to the Typed Elements Data Storage, but didn't get a valid pointer back."));
	CounterColumn->LabelTextFormatter = LabelText;
	CounterColumn->Query = Query;

	FTypedElementU32IntValueCacheColumn* CacheColumn = DataStorage->AddOrGetColumn<FTypedElementU32IntValueCacheColumn>(Row);
	checkf(CacheColumn, TEXT("Added a new FTypedElementUnsigned32BitIntValueCache to the Typed Elements Data Storage, but didn't get a valid pointer back."));
	CacheColumn->Value = 0;
}

#undef LOCTEXT_NAMESPACE