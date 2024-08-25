// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Editor/Model/PropertyUtils.h"

#include "Algo/AnyOf.h"
#include "Replication/PropertyChainUtils.h"

namespace UE::ConcertClientSharedSlate::PropertyUtils
{
	void EnumerateAdditionalPropertiesToAdd(
		const UClass& ObjectClass,
		TConstArrayView<FConcertPropertyChain> PropertiesToAdd,
		TFunctionRef<void(FConcertPropertyChain&& AdditionalProperty)> Callback
		)
	{
		ConcertSyncCore::PropertyChain::ForEachReplicatableConcertProperty(ObjectClass, [&PropertiesToAdd, &Callback](FConcertPropertyChain&& ConsideredProperty)
		{
			const bool bIsChildOfAddedProperty = Algo::AnyOf(PropertiesToAdd, [&ConsideredProperty](const FConcertPropertyChain& AddedProperty)
			{
				return ConsideredProperty.IsChildOf(AddedProperty);
			});
			if (bIsChildOfAddedProperty)
			{
				Callback(MoveTemp(ConsideredProperty));
			}
			
			return EBreakBehavior::Continue;
		});
	}
}
