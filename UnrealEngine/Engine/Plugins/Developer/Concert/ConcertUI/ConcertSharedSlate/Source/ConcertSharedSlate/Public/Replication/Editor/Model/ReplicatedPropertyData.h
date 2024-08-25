// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Data/ConcertPropertySelection.h"

namespace UE::ConcertSharedSlate
{
	/** Instanced for each property row in SObjectToPropertyView.*/
	class FReplicatedPropertyData
	{
	public:
		
		FReplicatedPropertyData(FSoftClassPath OwningClass, FConcertPropertyChain Object)
			: OwningClass(MoveTemp(OwningClass))
			, Property(MoveTemp(Object))
		{}
		
		const FConcertPropertyChain& GetProperty() const { return Property; }
		const FSoftClassPath& GetOwningClass() const { return OwningClass; }

	private:

		/** The class with which the FProperty can be determined. */
		FSoftClassPath OwningClass;
		
		/** The property to be replicated */
		FConcertPropertyChain Property;
	};
}