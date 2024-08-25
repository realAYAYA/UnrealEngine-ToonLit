// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/TypedElementSlateWidgetReferenceColumnUpdateProcessor.h"

#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"

void UTypedElementSlateWidgetReferenceColumnUpdateFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage)
{
	RegisterDeleteRowOnWidgetDeleteQuery(DataStorage);
	RegisterDeleteColumnOnWidgetDeleteQuery(DataStorage);
}

void UTypedElementSlateWidgetReferenceColumnUpdateFactory::RegisterDeleteRowOnWidgetDeleteQuery(ITypedElementDataStorageInterface& DataStorage) const
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	DataStorage.RegisterQuery(
    	Select(
    		TEXT("Delete row with deleted widget"),
    		FPhaseAmble(FPhaseAmble::ELocation::Preamble, DSI::EQueryTickPhase::FrameEnd),
    		[](DSI::IQueryContext& Context, TypedElementRowHandle Row, const FTypedElementSlateWidgetReferenceColumn& WidgetReference)
    		{
    			if (!WidgetReference.TedsWidget.IsValid())
    			{
    				Context.RemoveRow(Row);
    			}
    		}
    	)
    	.Where()
    		.All<FTypedElementSlateWidgetReferenceDeletesRowTag>()
    	.Compile()
    	);
}

void UTypedElementSlateWidgetReferenceColumnUpdateFactory::RegisterDeleteColumnOnWidgetDeleteQuery(ITypedElementDataStorageInterface& DataStorage) const
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	DataStorage.RegisterQuery(
		Select(
			TEXT("Delete widget columns for deleted widget"),
			FPhaseAmble(FPhaseAmble::ELocation::Preamble, DSI::EQueryTickPhase::FrameEnd),
			[](DSI::IQueryContext& Context, TypedElementRowHandle Row, const FTypedElementSlateWidgetReferenceColumn& WidgetReference)
			{
				if (!WidgetReference.TedsWidget.IsValid())
				{
					Context.RemoveColumns<FTypedElementSlateWidgetReferenceColumn>(Row);
				}
			}
		)
		.Where()
			.None<FTypedElementSlateWidgetReferenceDeletesRowTag>()
		.Compile()
	);
}
