// Copyright Epic Games, Inc. All Rights Reserved.

#include "Queries/TypedElementHierarchyQueries.h"

#include "Elements/Columns/TypedElementHiearchyColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"

void UTypedElementHiearchyQueriesFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage)
{
	using namespace TypedElementDataStorage;
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	DataStorage.RegisterQuery(
		Select(
			TEXT("Resolve hierarchy rows"),
			FProcessor(DSI::EQueryTickPhase::PrePhysics, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::Default)),
			[](DSI::IQueryContext& Context, RowHandle Row, const FTypedElementUnresolvedParentColumn& UnresolvedParent)
			{
				RowHandle ParentRow = Context.FindIndexedRow(UnresolvedParent.ParentIdHash);
				if (Context.IsRowAvailable(ParentRow))
				{
					Context.RemoveColumns<FTypedElementUnresolvedParentColumn>(Row);
					Context.AddColumn(Row, FTypedElementParentColumn{ .Parent = ParentRow });
				}
			}
		)
		.Compile()
	);
}
