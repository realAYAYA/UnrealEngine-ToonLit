// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryCollection/ManagedArrayAccessor.h"
#include "GeometryCollection/ManagedArrayCollection.h"

namespace Chaos::Facades
{
	/**
	 * Provides an API to read and manipulate hierarchy in a managed array collection
	 */
	class CHAOS_API FCollectionHierarchyFacade
	{
	public:
		enum class EPersistencePolicy : uint8
		{
			KeepExistingPersistence,
			MustBePersistent
		};

	public:
		FCollectionHierarchyFacade(FManagedArrayCollection& InCollection);

		/** Valid if parent and children arrays are available */
		bool IsValid() const;

		/** whether the level attribute is available */
		bool HasLevelAttribute() const;

		/** whether the level attribute is persistent */
		bool IsLevelAttributePersistent() const;

		/** 
		* Update level attribute for all elements (and create it if it is missing ) 
		* @Param PersistencePolicy whether to make the attribute persistent or keep the existing state
		*/ 
		void GenerateLevelAttribute();

	private:
		TManagedArrayAccessor<int32>		ParentAttribute;
		TManagedArrayAccessor<TSet<int32>>	ChildrenAttribute;
		TManagedArrayAccessor<int32>		LevelAttribute;
	};
}