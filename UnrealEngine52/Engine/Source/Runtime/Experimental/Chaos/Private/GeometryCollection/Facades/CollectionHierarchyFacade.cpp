// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/Facades/CollectionHierarchyFacade.h"
#include "GeometryCollection/TransformCollection.h"

namespace Chaos::Facades
{
	FCollectionHierarchyFacade::FCollectionHierarchyFacade(FManagedArrayCollection& InCollection)
		: ParentAttribute(InCollection, FTransformCollection::ParentAttribute, FTransformCollection::TransformGroup)
	    , ChildrenAttribute(InCollection, FTransformCollection::ChildrenAttribute, FTransformCollection::TransformGroup)
		, LevelAttribute(InCollection, FTransformCollection::LevelAttribute, FTransformCollection::TransformGroup)
	{}

	FCollectionHierarchyFacade::FCollectionHierarchyFacade(const FManagedArrayCollection& InCollection)
		: ParentAttribute(InCollection, FTransformCollection::ParentAttribute, FTransformCollection::TransformGroup)
		, ChildrenAttribute(InCollection, FTransformCollection::ChildrenAttribute, FTransformCollection::TransformGroup)
		, LevelAttribute(InCollection, FTransformCollection::LevelAttribute, FTransformCollection::TransformGroup)
	{}

	bool FCollectionHierarchyFacade::IsValid() const
	{
		return ParentAttribute.IsValid() && ChildrenAttribute.IsValid();
	}

	bool FCollectionHierarchyFacade::HasLevelAttribute() const
	{
		return LevelAttribute.IsValid();
	}

	bool FCollectionHierarchyFacade::IsLevelAttributePersistent() const
	{
		return LevelAttribute.IsValid() && LevelAttribute.IsPersistent();
	}

	int32 FCollectionHierarchyFacade::GetRootIndex() const
	{
		if (IsValid())
		{
			const TManagedArray<int32>& Parents = ParentAttribute.Get();
			if (Parents.Num() > 0)
			{
				int32 RootTransformIndex = 0;
				while (Parents[RootTransformIndex] != INDEX_NONE)
				{
					RootTransformIndex = Parents[RootTransformIndex];
				}
				return RootTransformIndex;
			}
		}
		return INDEX_NONE;
	}

	TArray<int32> FCollectionHierarchyFacade::GetRootIndices() const
	{
		return GetRootIndices(ParentAttribute);
	}

	int32 FCollectionHierarchyFacade::GetInitialLevel(int32 TransformIndex) const
	{
		if (HasLevelAttribute())
		{
			return LevelAttribute.Get()[TransformIndex];
		}
		return INDEX_NONE;
	}

	void FCollectionHierarchyFacade::GenerateLevelAttribute()
	{
		check(!IsConst());
		if (IsValid())
		{
			// level attribute must actually be persistent 
			LevelAttribute.Add(ManageArrayAccessor::EPersistencePolicy::MakePersistent);

			TManagedArray<int32>& Levels = LevelAttribute.Modify();
			const TManagedArray<int32>& Parents = ParentAttribute.Get();

			// In place level computation
			LevelAttribute.Fill(INDEX_NONE);
			for (int32 TransformIndex = 0; TransformIndex < Levels.Num(); TransformIndex++)
			{
				// parent propagation may have already set the value for this transform
				if (Levels[TransformIndex] == INDEX_NONE)
				{
					// first compute the level by walking the parents
					Levels[TransformIndex] = 0;
					int32 ParentIndex = Parents[TransformIndex];
					while (ParentIndex != INDEX_NONE)
					{
						Levels[TransformIndex]++;

						// do we need to go further ? 
						const int32 ParentLevel = Levels[ParentIndex];
						if (ParentLevel != INDEX_NONE)
						{
							Levels[TransformIndex] += ParentLevel;
							break; // early out as the parent level has been computed already
						}

						ParentIndex = Parents[ParentIndex];
					}

					// second propagate to parents if they are still unitialized
					if (Levels[TransformIndex] > 0)
					{
						int32 ParentLevel = Levels[TransformIndex] - 1;
						ParentIndex = Parents[TransformIndex];
						while (ParentIndex != INDEX_NONE && Levels[ParentIndex] == INDEX_NONE)
						{
							Levels[ParentIndex] = ParentLevel;
							ParentLevel--;
							ParentIndex = Parents[ParentIndex];
						}
					}
				}
			}
		}
	}

	TArray<int32> FCollectionHierarchyFacade::GetRootIndices(const TManagedArrayAccessor<int32>& ParentAttribute)
	{
		TArray<int32> Roots;
		if (ParentAttribute.IsValid())
		{
			const TManagedArray<int32>& Parents = ParentAttribute.Get();
			for (int32 Idx = 0; Idx < Parents.Num(); ++Idx)
			{
				if (Parents[Idx] == INDEX_NONE)
				{
					Roots.Add(Idx);
				}
			}
		}

		return Roots;
	}
}