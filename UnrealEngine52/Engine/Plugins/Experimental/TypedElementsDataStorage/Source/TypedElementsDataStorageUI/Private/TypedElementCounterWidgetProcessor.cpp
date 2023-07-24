// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementCounterWidgetProcessor.h"

#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Elements/Columns/TypedElementValueCacheColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "TypedElementCounterWidgetConstructor.h"
#include "TypedElementSubsystems.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "Widgets/Text/STextBlock.h"

UTypedElementCounterWidgetProcessor::UTypedElementCounterWidgetProcessor()
	: UiQuery(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Editor);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::UpdateWorldFromMass;
	ProcessingPhase = EMassProcessingPhase::FrameEnd;
	bRequiresGameThreadExecution = true;
}

void UTypedElementCounterWidgetProcessor::ConfigureQueries()
{
	UiQuery.AddRequirement(FTypedElementSlateWidgetReferenceColumn::StaticStruct(), EMassFragmentAccess::ReadWrite);
	UiQuery.AddRequirement(FTypedElementU32IntValueCacheColumn::StaticStruct(), EMassFragmentAccess::ReadWrite);
	UiQuery.AddRequirement(FTypedElementCounterWidgetColumn::StaticStruct(), EMassFragmentAccess::ReadOnly);
	
	ProcessorRequirements.AddSubsystemRequirement(UTypedElementDataStorageSubsystem::StaticClass(), EMassFragmentAccess::ReadWrite);
}

void UTypedElementCounterWidgetProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	ITypedElementDataStorageInterface* DataInterface = 
		Context.GetMutableSubsystemChecked<UTypedElementDataStorageSubsystem>().Get();
	checkf(DataInterface, TEXT(
		"FTypedElementsDataStorageUiModule tried to process widgets before the "
		"Typed Elements Data Storage interface is available."));

	UiQuery.ForEachEntityChunk(EntityManager, Context, [DataInterface](FMassExecutionContext& Context)
		{
			FTypedElementSlateWidgetReferenceColumn* WidgetIt = 
				Context.GetMutableFragmentView<FTypedElementSlateWidgetReferenceColumn>().GetData();
			FTypedElementU32IntValueCacheColumn* CacheIt = 
				Context.GetMutableFragmentView<FTypedElementU32IntValueCacheColumn>().GetData();
			FTypedElementCounterWidgetColumn* CounterIt = 
				Context.GetMutableFragmentView<FTypedElementCounterWidgetColumn>().GetData();

			const int32 NumEntities = Context.GetNumEntities();
			for (int32 Index = 0; Index < NumEntities; ++Index)
			{
				ITypedElementDataStorageInterface::FQueryResult Result = DataInterface->RunQuery(CounterIt->Query);
				if (Result.Completed == ITypedElementDataStorageInterface::FQueryResult::ECompletion::Fully &&
					Result.Count != CacheIt->Value)
				{
					checkf(WidgetIt->Widget.IsValid(),
						TEXT("Referenced widget is not valid. A constructed widget may not have been cleaned up. This can also happen if this "
						"processor is runningin the same phase as the processors responsible for cleaning up old references."));

					TSharedPtr<SWidget> WidgetPointer = WidgetIt->Widget.Pin();
					checkf(WidgetPointer, TEXT("Widget pointer was null during processing in Counter Widget Processor. This means "
						"that the processor that cleans up widgets hasn't been run before this processor."));
					checkf(WidgetPointer->GetType() == STextBlock::StaticWidgetClass().GetWidgetType(),
						TEXT("Stored widget with FTypedElementCounterWidgetFragment doesn't match type %s, but was a %s."),
						*(STextBlock::StaticWidgetClass().GetWidgetType().ToString()),
						*(WidgetPointer->GetTypeAsString()));

					STextBlock* Widget = static_cast<STextBlock*>(WidgetPointer.Get());
					Widget->SetText(FText::Format(CounterIt->LabelTextFormatter, Result.Count));
					CacheIt->Value = Result.Count;
				}

				++WidgetIt;
				++CacheIt;
				++CounterIt;
			}
		});
}
