// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementCounterWidget.h"

#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Elements/Columns/TypedElementValueCacheColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Framework/Text/TextLayout.h"
#include "Interfaces/IMainFrameModule.h"
#include "Layout/Margin.h"
#include "Misc/CoreDelegates.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "TypedElementUI_CounterWidget"


FAutoConsoleCommand EnableCounterWidgetsConsoleCommand(
	TEXT("TEDS.UI.EnableCounterWidgets"),
	TEXT("Adds registered counter widgets to the bottom right status bar of the main editor window."),
	FConsoleCommandDelegate::CreateLambda([]()
		{
			UTypedElementCounterWidgetFactory::EnableCounterWidgets();
		}));

//
// UTypedElementCounterWidgetFactory
//

FName UTypedElementCounterWidgetFactory::WigetPurpose(TEXT("LevelEditor.StatusBar.ToolBar"));
bool UTypedElementCounterWidgetFactory::bAreCounterWidgetsEnabled{ false };
bool UTypedElementCounterWidgetFactory::bHasBeenSetup{ false };

UTypedElementCounterWidgetFactory::UTypedElementCounterWidgetFactory()
{
	if (bAreCounterWidgetsEnabled)
	{
		IMainFrameModule::Get().OnMainFrameCreationFinished().AddStatic(&UTypedElementCounterWidgetFactory::SetupMainWindowIntegrations);
	}
}

void UTypedElementCounterWidgetFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage)
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	DataStorage.RegisterQuery(Select(TEXT("Sync counter widgets"), 
		FProcessor(DSI::EQueryTickPhase::FrameEnd, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::SyncWidgets))
			.ForceToGameThread(true),
		[](
			DSI::IQueryContext& Context,
			FTypedElementSlateWidgetReferenceColumn& Widget,
			FTypedElementU32IntValueCacheColumn& Comparison, 
			const FTypedElementCounterWidgetColumn& Counter
		)
		{
			DSI::FQueryResult Result = Context.RunQuery(Counter.Query);
			if (Result.Completed == DSI::FQueryResult::ECompletion::Fully && Result.Count != Comparison.Value)
			{
				TSharedPtr<SWidget> WidgetPointer = Widget.Widget.Pin();
				checkf(WidgetPointer, TEXT("Referenced widget is not valid. A constructed widget may not have been cleaned up. This can "
					"also happen if this processor is running in the same phase as the processors responsible for cleaning up old "
					"references."));
				checkf(WidgetPointer->GetType() == STextBlock::StaticWidgetClass().GetWidgetType(),
					TEXT("Stored widget with FTypedElementCounterWidgetFragment doesn't match type %s, but was a %s."),
					*(STextBlock::StaticWidgetClass().GetWidgetType().ToString()),
					*(WidgetPointer->GetTypeAsString()));

				STextBlock* WidgetInstance = static_cast<STextBlock*>(WidgetPointer.Get());
				WidgetInstance->SetText(FText::Format(Counter.LabelTextFormatter, Result.Count));
				Comparison.Value = Result.Count;
			}
		}
	).Compile());
}

void UTypedElementCounterWidgetFactory::RegisterWidgetPurposes(ITypedElementDataStorageUiInterface& DataStorageUi) const
{
	DataStorageUi.RegisterWidgetPurpose(WigetPurpose, ITypedElementDataStorageUiInterface::EPurposeType::Generic,
		LOCTEXT("ToolBarPurposeDescription", "Widgets added to the status bar at the bottom editor of the main editor window."));
}

void UTypedElementCounterWidgetFactory::RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
	ITypedElementDataStorageUiInterface& DataStorageUi) const
{
	using namespace TypedElementQueryBuilder;

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
	DataStorageUi.RegisterWidgetFactory(WigetPurpose, MoveTemp(ActorCounter));

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
	DataStorageUi.RegisterWidgetFactory(WigetPurpose, MoveTemp(WidgetCounter));
}

void UTypedElementCounterWidgetFactory::EnableCounterWidgets()
{
	bAreCounterWidgetsEnabled = true;
	UTypedElementCounterWidgetFactory::SetupMainWindowIntegrations(nullptr, false);
}

void UTypedElementCounterWidgetFactory::SetupMainWindowIntegrations(TSharedPtr<SWindow> ParentWindow, bool bIsRunningStartupDialog)
{
	if (!bHasBeenSetup)
	{
		UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
		checkf(Registry, TEXT(
			"FTypedElementsDataStorageUiModule didn't find the UTypedElementRegistry during main window integration when it should be available."));

		ITypedElementDataStorageUiInterface* UiInterface = Registry->GetMutableDataStorageUi();
		checkf(UiInterface, TEXT(
			"FTypedElementsDataStorageUiModule tried to integrate with the main window before the "
			"Typed Elements Data Storage UI interface is available."));

		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(WigetPurpose);

		TArray<TSharedRef<SWidget>> Widgets;
		UiInterface->ConstructWidgets(WigetPurpose, {},
			[&Widgets](const TSharedRef<SWidget>& NewWidget, TypedElementRowHandle Row)
			{
				Widgets.Add(NewWidget);
			});

		if (!Widgets.IsEmpty())
		{
			FToolMenuSection& Section = Menu->AddSection("DataStorageSection");
			int32 WidgetCount = Widgets.Num();

			Section.AddEntry(FToolMenuEntry::InitWidget("DataStorageStatusBarWidget_0", MoveTemp(Widgets[0]), FText::GetEmpty()));
			for (int32 I = 1; I < WidgetCount; ++I)
			{
				Section.AddSeparator(FName(*FString::Format(TEXT("DataStorageStatusBarWidgetDivider_{0}"), { FString::FromInt(I) })));
				Section.AddEntry(FToolMenuEntry::InitWidget(
					FName(*FString::Format(TEXT("DataStorageStatusBarWidget_{0}"), { FString::FromInt(I) })),
					MoveTemp(Widgets[I]), FText::GetEmpty()));
			}
		}
		bHasBeenSetup = true;
	}
}



//
// FTypedElementCounterWidgetConstructor
//

FTypedElementCounterWidgetConstructor::FTypedElementCounterWidgetConstructor()
	: Super(FTypedElementCounterWidgetConstructor::StaticStruct())
{
}

TConstArrayView<const UScriptStruct*> FTypedElementCounterWidgetConstructor::GetAdditionalColumnsList() const
{
	static TTypedElementColumnTypeList<FTypedElementCounterWidgetColumn, FTypedElementU32IntValueCacheColumn> Columns;
	return Columns;
}

TSharedPtr<SWidget> FTypedElementCounterWidgetConstructor::CreateWidget(const TypedElementDataStorage::FMetaDataView& Arguments)
{
	return SNew(STextBlock)
		.Text(FText::Format(LabelText, 0))
		.Margin(FMargin(4.0f, 0.0f))
		.ToolTipText(ToolTipText)
		.Justification(ETextJustify::Center);
}

bool FTypedElementCounterWidgetConstructor::SetColumns(ITypedElementDataStorageInterface* DataStorage, TypedElementRowHandle Row)
{
	FTypedElementCounterWidgetColumn* CounterColumn = DataStorage->GetColumn<FTypedElementCounterWidgetColumn>(Row);
	checkf(CounterColumn, TEXT("Added a new FTypedElementCounterWidgetColumn to the Typed Elements Data Storage, but didn't get a valid pointer back."));
	CounterColumn->LabelTextFormatter = LabelText;
	CounterColumn->Query = Query;

	FTypedElementU32IntValueCacheColumn* CacheColumn = DataStorage->GetColumn<FTypedElementU32IntValueCacheColumn>(Row);
	checkf(CacheColumn, TEXT("Added a new FTypedElementUnsigned32BitIntValueCache to the Typed Elements Data Storage, but didn't get a valid pointer back."));
	CacheColumn->Value = 0;

	return true;
}

#undef LOCTEXT_NAMESPACE