// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryCollection/ManagedArrayCollection.h"
#include "GeometryCollection/ManagedArrayAccessor.h"

namespace GeometryCollection::Facades
{

	/**
	 * Provides an API for connection-graph related attributes
	 */
	class CHAOS_API FCollectionConnectionGraphFacade
	{
	public:
		FCollectionConnectionGraphFacade(FManagedArrayCollection& InCollection);
		FCollectionConnectionGraphFacade(const FManagedArrayCollection& InCollection);

		/** Does the collection support the facade. */
		bool IsValid() const;

		/** Is the facade defined constant. */
		bool IsConst() const { return ConnectionsAttribute.IsConst(); }

		/** Create the facade attributes. */
		void DefineSchema();

		/** Remove the attributes */
		void ClearAttributes();

		/** Connect two bones */
		void Connect(int32 BoneA, int32 BoneB);

		/**  Connections between bones that have the same parent in the hierarchy */
		TManagedArrayAccessor<TSet<int32>> ConnectionsAttribute;

#if UE_BUILD_DEBUG
		/** Optional Parent array for validating connections in debug */
		TManagedArrayAccessor<int32> ParentAttribute;
#endif 
	};

}