// Copyright Epic Games, Inc. All Rights Reserved.

#include "Containers/DirectoryTree.h"

#include "Algo/BinarySearch.h"
#include "Misc/AutomationTest.h"

namespace UE::DirectoryTree
{

void FixupPathSeparator(FStringBuilderBase& InOutPath, int32 StartIndex, TCHAR InPathSeparator)
{
	if (InPathSeparator == '/')
	{
		return;
	}
	int32 SeparatorIndex;
	while (InOutPath.ToView().RightChop(StartIndex).FindChar('/', SeparatorIndex))
	{
		StartIndex += SeparatorIndex;
		InOutPath.GetData()[StartIndex] = InPathSeparator;
	}
}

int32 FindInsertionIndex(int32 NumChildNodes, const TUniquePtr<FString[]>& RelPaths, FStringView FirstPathComponent, bool& bOutExists)
{
	TConstArrayView<FString> RelPathsRange(RelPaths.Get(), NumChildNodes);
	int32 Index = Algo::LowerBound(RelPathsRange, FirstPathComponent,
		[](const FString& ChildRelPath, FStringView FirstPathComponent)
		{
			return FPathViews::Less(ChildRelPath, FirstPathComponent);
		}
	);
	bOutExists = Index < NumChildNodes && FPathViews::IsParentPathOf(FirstPathComponent, RelPaths[Index]);
	return Index;
}

} // namespace UE::DirectoryTree

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectoryTreeTest, "System.Core.DirectoryTree", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDirectoryTreeTest::RunTest(const FString& Parameters)
{
	constexpr int32 NumPathTypes = 5;
	constexpr int32 NumPaths = 9;
	FStringView PathsByTypeAndIndex[NumPathTypes][NumPaths] =
	{
		{
			TEXTVIEW("/Game/Dir2"),
			TEXTVIEW("/Game/Path1"),
			TEXTVIEW("/Game/Dir2/Path2"),
			TEXTVIEW("/Plugin1/Path1"),
			TEXTVIEW("/Plugin1/Dir2/Path2"),
			TEXTVIEW("/Engine/Path1"),
			TEXTVIEW("/Plugin2/Path1"),
			// Make sure we handle suffixes of an existing string with a leading value that sorts after /
			TEXTVIEW("/Game/Foo/Leaf"),
			TEXTVIEW("/Game/Foo-Bar/Leaf"),
		},
		{
			TEXTVIEW("d:\\root\\Project\\Content\\Dir2"),
			TEXTVIEW("d:\\root\\Project\\Content\\Path1.uasset"),
			TEXTVIEW("d:\\root\\Project\\Content\\Dir2\\Path2.uasset"),
			TEXTVIEW("d:\\root\\Project\\Plugins\\Plugin1\\Content\\Path1.uasset"),
			TEXTVIEW("d:\\root\\Project\\Plugins\\Plugin1\\Content\\Dir2\\Path2.uasset"),
			TEXTVIEW("d:\\root\\Engine\\Content\\Path1.uasset"),
			TEXTVIEW("e:\\root\\Project\\Plugins\\Plugin2\\Content\\Path1.uasset"),
			TEXTVIEW("d:\\root\\Project\\Content\\Foo\\Leaf"),
			TEXTVIEW("d:\\root\\Project\\Content\\Foo-Bar\\Leaf"),
		},
		{
			TEXTVIEW("d:/root/Project/Content/Dir2"),
			TEXTVIEW("d:/root/Project/Content/Path1.uasset"),
			TEXTVIEW("d:/root/Project/Content/Dir2/Path2.uasset"),
			TEXTVIEW("d:/root/Project/Plugins/Plugin1/Content/Path1.uasset"),
			TEXTVIEW("d:/root/Project/Plugins/Plugin1/Content/Dir2/Path2.uasset"),
			TEXTVIEW("d:/root/Engine/Content/Path1.uasset"),
			TEXTVIEW("e:/root/Project/Plugins/Plugin2/Content/Path1.uasset"),
			TEXTVIEW("d:/root/Project/Content/Foo/Leaf"),
			TEXTVIEW("d:/root/Project/Content/Foo-Bar/Leaf"),
		},
		{
			TEXTVIEW("..\\..\\..\\Project\\Content\\Dir2"),
			TEXTVIEW("..\\..\\..\\Project\\Content\\Path1.uasset"),
			TEXTVIEW("..\\..\\..\\Project\\Content\\Dir2\\Path2.uasset"),
			TEXTVIEW("..\\..\\..\\Project\\Plugins\\Plugin1\\Content\\Path1.uasset"),
			TEXTVIEW("..\\..\\..\\Project\\Plugins\\Plugin1\\Content\\Dir2\\Path2.uasset"),
			TEXTVIEW("..\\..\\..\\Engine\\Content\\Path1.uasset"),
			TEXTVIEW("e:\\root\\Project\\Plugins\\Plugin2\\Content\\Path1.uasset"),
			TEXTVIEW("..\\..\\..\\Project\\Content\\Foo\\Leaf"),
			TEXTVIEW("..\\..\\..\\Project\\Content\\Foo-Bar\\Leaf"),
		},
		{
			TEXTVIEW("../../../Project/Content/Dir2"),
			TEXTVIEW("../../../Project/Content/Path1.uasset"),
			TEXTVIEW("../../../Project/Content/Dir2/Path2.uasset"),
			TEXTVIEW("../../../Project/Plugins/Plugin1/Content/Path1.uasset"),
			TEXTVIEW("../../../Project/Plugins/Plugin1/Content/Dir2/Path2.uasset"),
			TEXTVIEW("../../../Engine/Content/Path1.uasset"),
			TEXTVIEW("e:/root/Project/Plugins/Plugin2/Content/Path1.uasset"),
			TEXTVIEW("../../../Project/Content/Foo/Leaf"),
			TEXTVIEW("../../../Project/Content/Foo-Bar/Leaf"),
		},
	};
	// PathSub0SubPath[i] provides a sub path of PathsByTypeAndIndex[i][0]
	FStringView Path0SubPath[NumPathTypes] =
	{
		TEXTVIEW("/Game/Dir2/Sub"),
		TEXTVIEW("d:\\root\\Project\\Content\\Dir2\\Sub"),
		TEXTVIEW("d:/root/Project/Content/Dir2/Sub"),
		TEXTVIEW("..\\..\\..\\Project\\Content\\Dir2"),
		TEXTVIEW("../../../Project/Content/Dir2/Sub"),
	};
	int32 ValueByIndex[NumPaths] = { 1,2,3,4,5,6,7,8,9 };
	FStringView NonPathsByTypeAndIndex0[] =
		{
			TEXTVIEW(""),
			TEXTVIEW("/"),
			TEXTVIEW("/Game"),
			TEXTVIEW("/Game/"),
			TEXTVIEW("/Plugin1"),
			TEXTVIEW("/Plugin1/"),
			TEXTVIEW("/Plugin1/Dir2"),
			TEXTVIEW("/Plugin1/Dir2/"),
			TEXTVIEW("/Engine"),
			TEXTVIEW("/Engine/"),
		};
	FStringView NonPathsByTypeAndIndex1[] =
		{
			TEXTVIEW(""),
			TEXTVIEW("d:\\"),
			TEXTVIEW("d:\\root1"),
			TEXTVIEW("d:\\root1\\"),
			TEXTVIEW("d:\\root1\\Project"),
			TEXTVIEW("d:\\root1\\Project\\"),
			TEXTVIEW("d:\\root1\\Project\\Content"),
			TEXTVIEW("d:\\root1\\Project\\Plugins\\"),
			TEXTVIEW("d:\\root1\\Project\\Plugins\\Content"),
			TEXTVIEW("d:\\root1\\Project\\Plugins\\Content\\"),
			TEXTVIEW("d:\\root1\\Project\\Plugins\\Content\\Plugin1"),
			TEXTVIEW("d:\\root1\\Project\\Plugins\\Content\\Plugin1\\"),
			TEXTVIEW("d:\\root1\\Project\\Plugins\\Content\\Plugin1\\Dir2"),
			TEXTVIEW("d:\\root1\\Project\\Plugins\\Content\\Plugin1\\Dir2\\"),
			TEXTVIEW("d:\\root1\\Engine"),
			TEXTVIEW("d:\\root1\\Engine\\"),
			TEXTVIEW("d:\\root1\\Engine\\Content"),
			TEXTVIEW("d:\\root1\\Engine\\Content\\"),
		};
	FStringView NonPathsByTypeAndIndex2[] =
		{
			TEXTVIEW(""),
			TEXTVIEW("d:/"),
			TEXTVIEW("d:/root1"),
			TEXTVIEW("d:/root1/Project"),
			TEXTVIEW("d:/root1/Project/Content"),
			TEXTVIEW("d:/root1/Project/Plugins/Content"),
			TEXTVIEW("d:/root1/Project/Plugins/Content/Plugin1"),
			TEXTVIEW("d:/root1/Project/Plugins/Content/Plugin1/Dir2"),
			TEXTVIEW("d:/root1/Engine"),
			TEXTVIEW("d:/root1/Engine/Content"),
		};
	FStringView NonPathsByTypeAndIndex3[] =
		{
			TEXTVIEW(""),
			TEXTVIEW("\\"),
			TEXTVIEW(".."),
			TEXTVIEW("..\\"),
			TEXTVIEW("..\\.."),
			TEXTVIEW("..\\..\\"),
			TEXTVIEW("..\\..\\.."),
			TEXTVIEW("..\\..\\..\\"),
			TEXTVIEW("..\\..\\..\\Project"),
			TEXTVIEW("..\\..\\..\\Project\\Content"),
			TEXTVIEW("..\\..\\..\\Project\\Plugins\\Content"),
			TEXTVIEW("..\\..\\..\\Project\\Plugins\\Content\\Plugin1"),
			TEXTVIEW("..\\..\\..\\Project\\Plugins\\Content\\Plugin1\\Dir2"),
			TEXTVIEW("..\\..\\..\\Engine"),
			TEXTVIEW("..\\..\\..\\Engine\\Content"),
		};
	FStringView NonPathsByTypeAndIndex4[] =
		{
			TEXTVIEW(""),
			TEXTVIEW("/"),
			TEXTVIEW(".."),
			TEXTVIEW("../.."),
			TEXTVIEW("../../.."),
			TEXTVIEW("../../../Project"),
			TEXTVIEW("../../../Project/Content"),
			TEXTVIEW("../../../Project/Plugins/Content"),
			TEXTVIEW("../../../Project/Plugins/Content/Plugin1"),
			TEXTVIEW("../../../Project/Plugins/Content/Plugin1/Dir2"),
			TEXTVIEW("../../../Engine"),
			TEXTVIEW("../../../Engine/Content"),
		};
	TConstArrayView<FStringView> NonPathsByTypeAndIndex[NumPathTypes] =
	{
		NonPathsByTypeAndIndex0, NonPathsByTypeAndIndex1, NonPathsByTypeAndIndex2, NonPathsByTypeAndIndex3,
		NonPathsByTypeAndIndex4
	};

	constexpr int32 NumPermutations = 2;
	int32 Permutations[NumPermutations][NumPaths] =
	{
		{ 0, 1, 2, 3, 4, 5, 6, 7, 8 },
		{ 8, 7, 6, 5, 4, 3, 2, 1, 0 },
	};

	auto GetDirTreeTestName = [](FStringView InTestName, int32 PathType, int32 Permutation, int32 EditPermutationIndex,
		int32 OtherPermutationIndex)
	{
		return FString::Printf(TEXT("%.*s(%d, %d, %d, %d)"),
			InTestName.Len(), InTestName.GetData(),
			PathType, Permutation, EditPermutationIndex, OtherPermutationIndex);
	};

	for (int32 PathType = 0; PathType < NumPathTypes; ++PathType)
	{
		int32 NumNonPaths = NonPathsByTypeAndIndex[PathType].Num();
		for (int32 Permutation = 0; Permutation < NumPermutations; ++Permutation)
		{
			TDirectoryTree<int32> Tree;

			// Add all the Paths in the given order and make Contains assertions after each addition
			for (int32 AddPathPermutationIndex = 0; AddPathPermutationIndex < NumPaths; ++AddPathPermutationIndex)
			{
				int32 AddPathIndex = Permutations[Permutation][AddPathPermutationIndex];
				FStringView AddPath = PathsByTypeAndIndex[PathType][AddPathIndex];

				// Add the path
				Tree.FindOrAdd(AddPath) = ValueByIndex[AddPathIndex];

				if (Tree.Num() != AddPathPermutationIndex+1)
				{
					AddError(GetDirTreeTestName(TEXTVIEW("TreeNum has expected value"), PathType, Permutation,
						AddPathPermutationIndex, 0));
				}

				// Assert all paths up to and including this one are included
				for (int32 OtherPermutationIndex = 0; OtherPermutationIndex <= AddPathPermutationIndex;
					++OtherPermutationIndex)
				{
					int32 OtherIndex = Permutations[Permutation][OtherPermutationIndex];
					FStringView OtherPath = PathsByTypeAndIndex[PathType][OtherIndex];
					int32 OtherValue = ValueByIndex[OtherIndex];
					int32* ExistingValue = Tree.Find(OtherPath);
					if (!ExistingValue)
					{
						AddError(GetDirTreeTestName(TEXTVIEW("OtherInList"), PathType, Permutation,
							AddPathPermutationIndex, OtherPermutationIndex));
					}
					else
					{
						if (*ExistingValue != OtherValue)
						{
							AddError(GetDirTreeTestName(TEXTVIEW("OtherInListMatchesValue"), PathType, Permutation,
								AddPathPermutationIndex, OtherPermutationIndex));
						}
						if (!Tree.ContainsPathOrParent(OtherPath))
						{
							AddError(GetDirTreeTestName(TEXTVIEW("ContainsPathOrParentOtherInList"), PathType, Permutation,
								AddPathPermutationIndex, OtherPermutationIndex));
						}
						else
						{
							FString ClosestPath;
							if (!Tree.TryFindClosestPath(OtherPath, ClosestPath))
							{
								AddError(GetDirTreeTestName(TEXTVIEW("TryFindClosestPathOtherInListSucceeds"), PathType, Permutation,
									AddPathPermutationIndex, OtherPermutationIndex));
							}
							else if (ClosestPath != OtherPath)
							{
								AddError(GetDirTreeTestName(TEXTVIEW("TryFindClosestPathOtherInListMatches"), PathType, Permutation,
									AddPathPermutationIndex, OtherPermutationIndex));
							}
						}
					}
				}

				// Assert all paths not yet added are not included
				for (int32 OtherPermutationIndex = AddPathPermutationIndex + 1; OtherPermutationIndex < NumPaths;
					++OtherPermutationIndex)
				{
					int32 OtherIndex = Permutations[Permutation][OtherPermutationIndex];
					FStringView OtherPath = PathsByTypeAndIndex[PathType][OtherIndex];
					if (Tree.Contains(OtherPath))
					{
						AddError(GetDirTreeTestName(TEXTVIEW("OtherNotInList"), PathType, Permutation,
							AddPathPermutationIndex, OtherPermutationIndex));
					}
					// Call ContainsPathOrParent to test whether it crashes, but not yet implemented that we verify 
					// what its return value should be.
					(void)Tree.ContainsPathOrParent(OtherPath);
				}

				// Assert all non paths are not included
				for (int32 NonPathIndex = 0; NonPathIndex < NumNonPaths; ++NonPathIndex)
				{
					FStringView NonPath = NonPathsByTypeAndIndex[PathType][NonPathIndex];
					if (Tree.Contains(NonPath))
					{
						AddError(GetDirTreeTestName(TEXTVIEW("NonPathNotInList"), PathType, Permutation,
							AddPathPermutationIndex, NonPathIndex));
					}
					// Call ContainsPathOrParent to test whether it crashes, but not yet implemented that we verify 
					// what its return value should be.
					(void)Tree.ContainsPathOrParent(NonPath);
				}
			}

			// Verify that the SubPath is present
			FString ExistingSubParentPath;
			int32* ExistingSubParentValue;
			if (!Tree.TryFindClosestPath(Path0SubPath[PathType], ExistingSubParentPath, &ExistingSubParentValue))
			{
				AddError(GetDirTreeTestName(TEXTVIEW("SubPathInTree"), PathType, Permutation, 0, 0));
			}
			else if (ExistingSubParentPath != PathsByTypeAndIndex[PathType][0] || *ExistingSubParentValue != ValueByIndex[0])
			{
				AddError(GetDirTreeTestName(TEXTVIEW("SubPathInTreeMatches"), PathType, Permutation, 0, 0));
			}

			// Remove all the Paths (in specified order) and make Contains assertions after each removal
			// Currently we only test removal in FIFO order; bugs that are specific to a removal-order
			// should be dependent only on the final added state and should not be dependent on the earlier add-order.
			for (int32 RemovePathPermutationIndex = 0; RemovePathPermutationIndex < NumPaths;
				++RemovePathPermutationIndex)
			{
				int32 RemovePathIndex = Permutations[Permutation][RemovePathPermutationIndex];
				FStringView RemovePath = PathsByTypeAndIndex[PathType][RemovePathIndex];

				// Remove the path
				bool bExisted;
				Tree.Remove(RemovePath, &bExisted);
				if (!bExisted)
				{
					AddError(GetDirTreeTestName(TEXTVIEW("RemoveFoundSomethingToRemove"), PathType, Permutation,
						RemovePathPermutationIndex, 0));
				}
				if (Tree.Num() != NumPaths - (RemovePathPermutationIndex+1))
				{
					AddError(GetDirTreeTestName(TEXTVIEW("TreeNum has expected value"), PathType, Permutation,
						RemovePathPermutationIndex, 0));
				}


				// Assert all paths not yet removed are still included
				for (int32 OtherPermutationIndex = RemovePathPermutationIndex+1; OtherPermutationIndex < NumPaths;
					++OtherPermutationIndex)
				{
					int32 OtherIndex = Permutations[Permutation][OtherPermutationIndex];
					FStringView OtherPath = PathsByTypeAndIndex[PathType][OtherIndex];
					int32 OtherValue = ValueByIndex[OtherIndex];
					int32* ExistingValue = Tree.Find(OtherPath);
					if (!ExistingValue)
					{
						AddError(GetDirTreeTestName(TEXTVIEW("OtherInListAfterRemoval"), PathType, Permutation,
							RemovePathPermutationIndex, OtherPermutationIndex));
					}
					else
					{
						if (*ExistingValue != OtherValue)
						{
							AddError(GetDirTreeTestName(TEXTVIEW("OtherInListAfterRemovalMatches"), PathType, Permutation,
								RemovePathPermutationIndex, OtherPermutationIndex));
						}
						if (!Tree.ContainsPathOrParent(OtherPath))
						{
							AddError(GetDirTreeTestName(TEXTVIEW("OtherContainsPathOrParentAfterRemoval"), PathType, Permutation,
								RemovePathPermutationIndex, OtherPermutationIndex));
						}
					}
				}

				// Assert all paths up to and including this one have been removed
				for (int32 OtherPermutationIndex = 0; OtherPermutationIndex <= RemovePathPermutationIndex;
					++OtherPermutationIndex)
				{
					int32 OtherIndex = Permutations[Permutation][OtherPermutationIndex];
					FStringView OtherPath = PathsByTypeAndIndex[PathType][OtherIndex];
					if (Tree.Contains(OtherPath))
					{
						AddError(GetDirTreeTestName(TEXT("OtherNotInListAfterRemoval"), PathType, Permutation,
							RemovePathPermutationIndex, OtherPermutationIndex));
					}
					// Call ContainsPathOrParent to test whether it crashes, but not yet implemented that we verify 
					// what its return value should be.
					(void)Tree.ContainsPathOrParent(OtherPath);
				}

				// Assert all non paths are not still included
				for (int32 NonPathIndex = 0; NonPathIndex < NumNonPaths; ++NonPathIndex)
				{
					FStringView NonPath = NonPathsByTypeAndIndex[PathType][NonPathIndex];
					if (Tree.Contains(NonPath))
					{
						AddError(GetDirTreeTestName(TEXTVIEW("NonPathNotInListAfterRemoval"), PathType, Permutation,
							RemovePathPermutationIndex, NonPathIndex));
					}
					// Call ContainsPathOrParent to test whether it crashes, but not yet implemented that we verify 
					// what its return value should be.
					(void)Tree.ContainsPathOrParent(NonPath);
				}
			}
			if (!Tree.IsEmpty())
			{
				AddError(GetDirTreeTestName(TEXTVIEW("TreeEmptyAfterRemoval"), PathType, Permutation, 0, 0));
			}
		}
	}

	// Testing some pathtype-independent scenarios
	{
		TDirectoryTree<int32> Tree;
		Tree.FindOrAdd(TEXTVIEW("/Root/Path1")) = 1;
		Tree.FindOrAdd(TEXTVIEW("/Root/Path2")) = 2;
		int32* FoundRoot = Tree.FindClosestValue(TEXTVIEW("/Root"));
		int32* FoundPath1 = Tree.FindClosestValue(TEXTVIEW("/Root/Path1"));
		int32* FoundPath1Sub = Tree.FindClosestValue(TEXTVIEW("/Root/Path1/Sub"));
		int32* FoundPath2 = Tree.FindClosestValue(TEXTVIEW("/Root/Path2"));
		int32* FoundPath2Sub = Tree.FindClosestValue(TEXTVIEW("/Root/Path2/Sub"));
		TestTrue(TEXT("TwoPaths Root does not exist"), FoundRoot == nullptr);
		TestTrue(TEXT("TwoPaths Path1 Value matches"), FoundPath1 && *FoundPath1 == 1);
		TestTrue(TEXT("TwoPaths Path1Sub Value matches"), FoundPath1Sub &&* FoundPath1Sub == 1);
		TestTrue(TEXT("TwoPaths Path2 Value matches"), FoundPath2 && *FoundPath2 == 2);
		TestTrue(TEXT("TwoPaths Path2Sub Value matches"), FoundPath2Sub && *FoundPath2Sub == 2);
	}
	{
		TDirectoryTree<int32> Tree;
		Tree.FindOrAdd(TEXTVIEW("/Root/Path1/A/B/C")) = 1;
		Tree.FindOrAdd(TEXTVIEW("/Root/Path2/A/B/C")) = 2;
		int32* FoundRoot = Tree.FindClosestValue(TEXTVIEW("/Root"));
		int32* FoundPath1 = Tree.FindClosestValue(TEXTVIEW("/Root/Path1/A/B/C"));
		int32* FoundPath1Sub = Tree.FindClosestValue(TEXTVIEW("/Root/Path1/A/B/C/Sub"));
		int32* FoundPath1Parent = Tree.FindClosestValue(TEXTVIEW("/Root/Path1/A"));
		int32* FoundPath2 = Tree.FindClosestValue(TEXTVIEW("/Root/Path2/A/B/C"));
		int32* FoundPath2Sub = Tree.FindClosestValue(TEXTVIEW("/Root/Path2/A/B/C/Sub"));
		int32* FoundPath2Parent = Tree.FindClosestValue(TEXTVIEW("/Root/Path2/A"));
		TestTrue(TEXT("TwoPathsLong Root does not exist"), FoundRoot == nullptr);
		TestTrue(TEXT("TwoPathsLong Path1 Value matches"), FoundPath1 && *FoundPath1 == 1);
		TestTrue(TEXT("TwoPathsLong Path1Sub Value matches"), FoundPath1Sub && *FoundPath1Sub == 1);
		TestTrue(TEXT("TwoPathsLong Path1 Parent does not exist"), FoundPath1Parent == nullptr);
		TestTrue(TEXT("TwoPathsLong Path2 Value matches"), FoundPath2 && *FoundPath2 == 2);
		TestTrue(TEXT("TwoPathsLong Path2Sub Value matches"), FoundPath2Sub && *FoundPath2Sub == 2);
		TestTrue(TEXT("TwoPathsLong Path2 Parent does not exist"), FoundPath2Parent == nullptr);
	}

	struct FMoveConstructOnly
	{
		FMoveConstructOnly() { Value = 437; }
		FMoveConstructOnly(FMoveConstructOnly&& Other) { Value = Other.Value; Other.Value = -1; }
		FMoveConstructOnly(const FMoveConstructOnly& Other) = delete;
		FMoveConstructOnly& operator=(FMoveConstructOnly&& Other) = delete;
		FMoveConstructOnly& operator=(const FMoveConstructOnly& Other) = delete;
		
		int32 Value;
	};

	{
		TDirectoryTree<FMoveConstructOnly> Tree;
		Tree.FindOrAdd(TEXTVIEW("/Root/PathM")).Value = 1;
		Tree.FindOrAdd(TEXTVIEW("/Root/PathP")).Value = 2;
		Tree.FindOrAdd(TEXTVIEW("/Root/PathA")).Value = 3;
		Tree.FindOrAdd(TEXTVIEW("/Root/PathZ"));

		FMoveConstructOnly* Value = Tree.Find(TEXTVIEW("/Root/PathA"));
		TestTrue(TEXT("MoveConstructOnlyValueA correct"), Value && Value->Value == 3);
		Value = Tree.Find(TEXTVIEW("/Root/PathM"));
		TestTrue(TEXT("MoveConstructOnlyValueM correct"), Value && Value->Value == 1);
		Value = Tree.Find(TEXTVIEW("/Root/PathP"));
		TestTrue(TEXT("MoveConstructOnlyValueP correct"), Value && Value->Value == 2);
		Value = Tree.Find(TEXTVIEW("/Root/PathZ"));
		TestTrue(TEXT("MoveConstructOnlyValueZ correct"), Value && Value->Value == 437);
	}
	return true;
}

#endif
