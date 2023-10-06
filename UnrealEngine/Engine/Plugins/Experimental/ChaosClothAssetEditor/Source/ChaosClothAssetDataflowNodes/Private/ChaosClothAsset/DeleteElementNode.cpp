// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/DeleteElementNode.h"
#include "ChaosClothAsset/ClothGeometryTools.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "Dataflow/DataflowInputOutput.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DeleteElementNode)

#define LOCTEXT_NAMESPACE "ChaosClothAssetDeleteElementNode"

FChaosClothAssetDeleteElementNode::FChaosClothAssetDeleteElementNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Collection);
	RegisterOutputConnection(&Collection, &Collection);
}

void FChaosClothAssetDeleteElementNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		using namespace UE::Chaos::ClothAsset;

		// Evaluate in collection
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(InCollection));

		FCollectionClothFacade Cloth(ClothCollection);
		if (Cloth.IsValid())
		{
			switch (ElementType)
			{
			case EChaosClothAssetElementType::None:
			default:
				break;
			case EChaosClothAssetElementType::SimMesh:
			{
				FClothGeometryTools::DeleteSimMesh(ClothCollection);
			} break;
			case EChaosClothAssetElementType::RenderMesh:
			{
				FClothGeometryTools::DeleteRenderMesh(ClothCollection);
			} break;
			case EChaosClothAssetElementType::SimPattern:
			{
				if (Elements.IsEmpty())
				{
					// Remove all sim patterns
					Cloth.SetNumSimPatterns(0);
				}
				else
				{
					TArray<int32> PatternsToRemove;
					PatternsToRemove.Reserve(Elements.Num());
					for (int32 ElementIndex = 0; ElementIndex < Cloth.GetNumSimPatterns(); ++ElementIndex)
					{
						if (Elements.Find(ElementIndex) != INDEX_NONE)
						{
							PatternsToRemove.Add(ElementIndex);
						}
					}
					if (PatternsToRemove.Num())
					{
						Cloth.RemoveSimPatterns(PatternsToRemove);
					}
				}
			} break;
			case EChaosClothAssetElementType::RenderPattern:
			{
				if (Elements.IsEmpty())
				{
					// Remove all render patterns
					Cloth.SetNumRenderPatterns(0);
				}
				else
				{
					TArray<int32> PatternsToRemove;
					PatternsToRemove.Reserve(Elements.Num());
					for (int32 ElementIndex = 0; ElementIndex < Cloth.GetNumRenderPatterns(); ++ElementIndex)
					{
						if (Elements.Find(ElementIndex) != INDEX_NONE)
						{
							PatternsToRemove.Add(ElementIndex);
						}
					}
					if (PatternsToRemove.Num())
					{
						Cloth.RemoveRenderPatterns(PatternsToRemove);
					}
				}
			} break;
			case EChaosClothAssetElementType::SimVertex2D:
			{
				if (Elements.IsEmpty())
				{
					for (int32 ElementIndex = 0; ElementIndex < Cloth.GetNumSimPatterns(); ++ElementIndex)
					{
						FCollectionClothSimPatternFacade Pattern = Cloth.GetSimPattern(ElementIndex);
						Pattern.RemoveAllSimVertices2D();
					}
				}
				else
				{
					TArray<int32> SortedToDeleteList;
					SortedToDeleteList.Reserve(Elements.Num());
					for (int32 PatternIndex = Cloth.GetNumSimPatterns()-1; PatternIndex >= 0; --PatternIndex)
					{
						FCollectionClothSimPatternFacade Pattern = Cloth.GetSimPattern(PatternIndex);
						const int32 VertexOffset = Pattern.GetSimVertices2DOffset();
						SortedToDeleteList.Reset();
						for (int32 VertexIndex = 0; VertexIndex < Pattern.GetNumSimVertices2D(); ++VertexIndex)
						{
							if (Elements.Find(VertexIndex + VertexOffset) != INDEX_NONE)
							{
								SortedToDeleteList.Add(VertexIndex);
							}
						}
						if (SortedToDeleteList.Num())
						{
							Pattern.RemoveSimVertices2D(SortedToDeleteList);
						}
					}
				}
			} break;
			case EChaosClothAssetElementType::SimVertex3D:
			{
				if (Elements.IsEmpty())
				{
					Cloth.RemoveAllSimVertices3D();
				}
				else
				{
					TArray<int32> SortedToDeleteList;
					SortedToDeleteList.Reserve(Elements.Num());
					for (int32 VertexIndex = 0; VertexIndex < Cloth.GetNumSimVertices3D(); ++VertexIndex)
					{
						if (Elements.Find(VertexIndex) != INDEX_NONE)
						{
							SortedToDeleteList.Add(VertexIndex);
						}
					}
					Cloth.RemoveSimVertices3D(SortedToDeleteList);
				}
			} break;
			case EChaosClothAssetElementType::RenderVertex:
			{
				if (Elements.IsEmpty())
				{
					for (int32 ElementIndex = 0; ElementIndex < Cloth.GetNumRenderPatterns(); ++ElementIndex)
					{
						FCollectionClothRenderPatternFacade Pattern = Cloth.GetRenderPattern(ElementIndex);
						Pattern.SetNumRenderVertices(0);
					}
				}
				else
				{
					TArray<int32> SortedToDeleteList;
					SortedToDeleteList.Reserve(Elements.Num());
					for (int32 PatternIndex = Cloth.GetNumRenderPatterns()-1; PatternIndex >= 0; --PatternIndex)
					{
						FCollectionClothRenderPatternFacade Pattern = Cloth.GetRenderPattern(PatternIndex);
						const int32 VertexOffset = Pattern.GetRenderVerticesOffset();
						SortedToDeleteList.Reset();
						for (int32 VertexIndex = 0; VertexIndex < Pattern.GetNumRenderVertices(); ++VertexIndex)
						{
							if (Elements.Find(VertexIndex + VertexOffset) != INDEX_NONE)
							{
								SortedToDeleteList.Add(VertexIndex);
							}
						}
						if (SortedToDeleteList.Num())
						{
							Pattern.RemoveRenderVertices(SortedToDeleteList);
						}
					}
				}
			} break;
			case EChaosClothAssetElementType::SimFace:
			{
				if (Elements.IsEmpty())
				{
					for (int32 ElementIndex = 0; ElementIndex < Cloth.GetNumSimPatterns(); ++ElementIndex)
					{
						FCollectionClothSimPatternFacade Pattern = Cloth.GetSimPattern(ElementIndex);
						Pattern.SetNumSimFaces(0);
					}
				}
				else
				{
					TArray<int32> SortedToDeleteList;
					SortedToDeleteList.Reserve(Elements.Num());
					for (int32 PatternIndex = Cloth.GetNumSimPatterns()-1; PatternIndex >= 0; --PatternIndex)
					{
						FCollectionClothSimPatternFacade Pattern = Cloth.GetSimPattern(PatternIndex);
						const int32 FaceOffset = Pattern.GetSimFacesOffset();
						SortedToDeleteList.Reset();
						for (int32 FaceIndex = 0; FaceIndex < Pattern.GetNumSimFaces(); ++FaceIndex)
						{
							if (Elements.Find(FaceIndex + FaceOffset) != INDEX_NONE)
							{
								SortedToDeleteList.Add(FaceIndex);
							}
						}
						if (SortedToDeleteList.Num())
						{
							Pattern.RemoveSimFaces(SortedToDeleteList);
						}
					}
				}
			} break;
			case EChaosClothAssetElementType::RenderFace:
			{
				if (Elements.IsEmpty())
				{
					for (int32 PatternIndex = 0; PatternIndex < Cloth.GetNumRenderPatterns(); ++PatternIndex)
					{
						FCollectionClothRenderPatternFacade Pattern = Cloth.GetRenderPattern(PatternIndex);
						Pattern.SetNumRenderFaces(0);
					}
				}
				else
				{
					TArray<int32> SortedToDeleteList;
					SortedToDeleteList.Reserve(Elements.Num());
					for (int32 PatternIndex = Cloth.GetNumRenderPatterns()-1; PatternIndex >= 0; --PatternIndex)
					{
						FCollectionClothRenderPatternFacade Pattern = Cloth.GetRenderPattern(PatternIndex);
						const int32 FaceOffset = Pattern.GetRenderFacesOffset();
						SortedToDeleteList.Reset();
						for (int32 FaceIndex = 0; FaceIndex < Pattern.GetNumRenderFaces(); ++FaceIndex)
						{
							if (Elements.Find(FaceIndex + FaceOffset) != INDEX_NONE)
							{
								SortedToDeleteList.Add(FaceIndex);
							}
						}
						if (SortedToDeleteList.Num())
						{
							Pattern.RemoveRenderFaces(SortedToDeleteList);
						}
					}
				}
			} break;
			case EChaosClothAssetElementType::Seam:
			{
				if (Elements.IsEmpty())
				{
					// Remove all seams
					Cloth.SetNumSeams(0);
				}
				else
				{
					TArray<int32> SeamsToRemove;
					SeamsToRemove.Reserve(Elements.Num());
					for (int32 SeamIndex = 0; SeamIndex < Cloth.GetNumSeams(); ++SeamIndex)
					{
						if (Elements.Find(SeamIndex) != INDEX_NONE)
						{
							SeamsToRemove.Add(SeamIndex);
						}
					}
					if (SeamsToRemove.Num())
					{
						Cloth.RemoveSeams(SeamsToRemove);
					}
				}
			}break;
			}
		}
		SetValue(Context, MoveTemp(*ClothCollection), &Collection);
	}
}

#undef LOCTEXT_NAMESPACE
