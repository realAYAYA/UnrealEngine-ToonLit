// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryCollection/ManagedArrayAccessor.h"
#include "GeometryCollection/ManagedArrayCollection.h"

namespace GeometryCollection::Facades
{
	/**
	 * Provides an API to read and manipulate hierarchy in a managed array collection
	 */
	class CHAOS_API FCollectionInstancedMeshFacade
	{
	public:
		FCollectionInstancedMeshFacade(FManagedArrayCollection& InCollection);
		FCollectionInstancedMeshFacade(const FManagedArrayCollection& InCollection);

		/** Create the facade attributes. */
		void DefineSchema();

		/** Valid if parent and children arrays are available */
		bool IsValid() const;

		/** Is the facade defined constant. */
		bool IsConst() const;

		/** get the total number of indices */
		int32 GetNumIndices() const;

		/** get the instance mesh index for specific transform index */
		int32 GetIndex(int32 TransformIndex) const;

		/** set the instance mesh index for specific transform index */
		void SetIndex(int32 TransformIndex, int32 InstanceMeshIndex);


	private:
		TManagedArrayAccessor<int32> InstancedMeshIndexAttribute;
	};
}