// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryCollection/ManagedArrayAccessor.h"
#include "GeometryCollection/ManagedArrayCollection.h"

namespace Chaos::Facades
{
	/**
	 * Provides an API to define anchoring properties on a collection
	 */
	class CHAOS_API FCollectionAnchoringFacade
	{
	public:
		FCollectionAnchoringFacade(FManagedArrayCollection& InCollection);

		bool HasInitialDynamicStateAttribute() const;
		
		EObjectStateType GetInitialDynamicState(int32 TransformIndex) const;
		void SetInitialDynamicState(int32 TransformIndex, EObjectStateType State);
		void SetInitialDynamicState(const TArray<int32>& TransformIndices, EObjectStateType State);
		
		bool HasAnchoredAttribute() const;
		void AddAnchoredAttribute();
		void CopyAnchoredAttribute(const FCollectionAnchoringFacade& Other);

		bool IsAnchored(int32 TransformIndex) const;
		void SetAnchored(int32 TransformIndex, bool bValue);
		void SetAnchored(const TArray<int32>& TransformIndices, bool bValue);
	
	private:
		/** Initial dynamic state of a transform ( can be changed at runtime ) */
		TManagedArrayAccessor<int32> InitialDynamicStateAttribute;

		/** Whether a transform is anchored ( orthogonal to the initial dynamic ) )*/
		TManagedArrayAccessor<bool> AnchoredAttribute;
	};
}