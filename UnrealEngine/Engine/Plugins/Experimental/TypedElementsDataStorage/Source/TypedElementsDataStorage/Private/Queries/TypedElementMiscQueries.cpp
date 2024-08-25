// Copyright Epic Games, Inc. All Rights Reserved.

#include "Queries/TypedElementMiscQueries.h"

#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"

void UTypedElementRemoveSyncToWorldTagFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage)
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	DataStorage.RegisterQuery(
		Select(
			TEXT("Remove 'sync to world' tag"),
			FPhaseAmble(FPhaseAmble::ELocation::Postamble, DSI::EQueryTickPhase::FrameEnd),
			[](DSI::IQueryContext& Context, const TypedElementRowHandle* Rows)
			{
				Context.RemoveColumns<FTypedElementSyncBackToWorldTag>(TConstArrayView<TypedElementRowHandle>(Rows, Context.GetRowCount()));
			}
		)
		.Where()
			.All<FTypedElementSyncBackToWorldTag>()
		.Compile()
	);

	DataStorage.RegisterQuery(
	Select(
		TEXT("Remove 'sync from world' tag"),
		FPhaseAmble(FPhaseAmble::ELocation::Postamble, DSI::EQueryTickPhase::FrameEnd),
		[](DSI::IQueryContext& Context, const TypedElementRowHandle* Rows)
		{
			Context.RemoveColumns<FTypedElementSyncFromWorldTag>(TConstArrayView<TypedElementRowHandle>(Rows, Context.GetRowCount()));
		}
	)
	.Where()
		.All<FTypedElementSyncFromWorldTag>()
	.Compile()
);
}
