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

		FCollectionHierarchyFacade(FManagedArrayCollection& InCollection);
		FCollectionHierarchyFacade(const FManagedArrayCollection& InCollection);

		/** Create the facade attributes. */
		void DefineSchema() {}

		/** Valid if parent and children arrays are available */
		bool IsValid() const;

		/** Is this facade const access */
		bool IsConst() const { return ParentAttribute.IsConst(); }

		/** whether the level attribute is available */
		bool HasLevelAttribute() const;

		/** whether the level attribute is persistent */
		bool IsLevelAttributePersistent() const;

		/** Get the root index */
		int32 GetRootIndex() const;

		/** Get the root indicies */
		TArray<int32> GetRootIndices() const;

		/** 
		* Get initial level of a specific transform index 
		* If the attribute is missing return INDEX_NONE
		*/
		int32 GetInitialLevel(int32 TransformIndex) const;

		/** 
		* Update level attribute for all elements (and create it if it is missing ) 
		* @Param PersistencePolicy whether to make the attribute persistent or keep the existing state
		*/ 
		void GenerateLevelAttribute();

	public:
		/** Get the root indicies */
		static TArray<int32> GetRootIndices(const TManagedArrayAccessor<int32>& ParentAttribute);

	private:
		TManagedArrayAccessor<int32>		ParentAttribute;
		TManagedArrayAccessor<TSet<int32>>	ChildrenAttribute;
		TManagedArrayAccessor<int32>		LevelAttribute;
	};
}