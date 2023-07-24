// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/TypedElementSlateWidgetReferenceColumnUpdateProcessor.h"

#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"

UTypedElementSlateWidgetReferenceColumnUpdateProcessor::UTypedElementSlateWidgetReferenceColumnUpdateProcessor()
	: ColumnRemovalQuery(*this)
	, RowDeletionQuery(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Editor);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::SyncWorldToMass;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
}

void UTypedElementSlateWidgetReferenceColumnUpdateProcessor::ConfigureQueries()
{
	ColumnRemovalQuery.AddRequirement(FTypedElementSlateWidgetReferenceColumn::StaticStruct(), EMassFragmentAccess::ReadOnly);
	ColumnRemovalQuery.AddTagRequirement(*FTypedElementSlateWidgetReferenceDeletesRowTag::StaticStruct(), EMassFragmentPresence::None);
	
	RowDeletionQuery.AddRequirement(FTypedElementSlateWidgetReferenceColumn::StaticStruct(), EMassFragmentAccess::ReadOnly);
	RowDeletionQuery.AddTagRequirement(*FTypedElementSlateWidgetReferenceDeletesRowTag::StaticStruct(), EMassFragmentPresence::All);
}

void UTypedElementSlateWidgetReferenceColumnUpdateProcessor::Execute(
	FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	ColumnRemovalQuery.ForEachEntityChunk(EntityManager, Context, [](FMassExecutionContext& Context)
		{
			CheckStatus<false>(Context);
		});

	RowDeletionQuery.ForEachEntityChunk(EntityManager, Context, [](FMassExecutionContext& Context)
		{
			CheckStatus<true>(Context);
		});
}

template<bool DeleteRow>
void UTypedElementSlateWidgetReferenceColumnUpdateProcessor::CheckStatus(FMassExecutionContext& Context)
{
	int32 Index = 0;
	TArrayView<FTypedElementSlateWidgetReferenceColumn> WidgetColumns =
		Context.GetMutableFragmentView<FTypedElementSlateWidgetReferenceColumn>();
	for (FTypedElementSlateWidgetReferenceColumn& WidgetColumn : WidgetColumns)
	{
		if (!WidgetColumn.Widget.IsValid())
		{
			if constexpr (DeleteRow)
			{
				Context.Defer().DestroyEntity(Context.GetEntity(Index));
			}
			else
			{
				Context.Defer().RemoveFragment_RuntimeCheck<FTypedElementSlateWidgetReferenceColumn>(Context.GetEntity(Index));
			}
		}
		++Index;
	}
}
