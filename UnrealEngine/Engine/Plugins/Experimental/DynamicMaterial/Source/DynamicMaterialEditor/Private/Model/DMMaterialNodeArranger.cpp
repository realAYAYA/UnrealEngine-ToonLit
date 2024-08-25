// Copyright Epic Games, Inc. All Rights Reserved.

#include "Model/DMMaterialNodeArranger.h"
#include "Containers/ArrayView.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"

namespace UE::DynamicMaterialEditor::BuildState::Private
{
	static constexpr int32 SpaceBetweenNodes = 50;
}

FDMMaterialNodeArranger::FDMMaterialNodeArranger(UMaterial* InDynamicMaterial)
	: DynamicMaterial(InDynamicMaterial)
	, OffsetStart({0, 0})
{
}

void FDMMaterialNodeArranger::ArrangeNodes()
{
	ArrangeMaterialInputNodes(DynamicMaterial->GetEditorOnlyData()->BaseColor.Expression);
	ArrangeMaterialInputNodes(DynamicMaterial->GetEditorOnlyData()->EmissiveColor.Expression);
	ArrangeMaterialInputNodes(DynamicMaterial->GetEditorOnlyData()->Opacity.Expression);
	ArrangeMaterialInputNodes(DynamicMaterial->GetEditorOnlyData()->OpacityMask.Expression);
	ArrangeMaterialInputNodes(DynamicMaterial->GetEditorOnlyData()->Metallic.Expression);
	ArrangeMaterialInputNodes(DynamicMaterial->GetEditorOnlyData()->Specular.Expression);
	ArrangeMaterialInputNodes(DynamicMaterial->GetEditorOnlyData()->Roughness.Expression);
	ArrangeMaterialInputNodes(DynamicMaterial->GetEditorOnlyData()->Anisotropy.Expression);
	ArrangeMaterialInputNodes(DynamicMaterial->GetEditorOnlyData()->Normal.Expression);
	ArrangeMaterialInputNodes(DynamicMaterial->GetEditorOnlyData()->Tangent.Expression);
	ArrangeMaterialInputNodes(DynamicMaterial->GetEditorOnlyData()->WorldPositionOffset.Expression);
	ArrangeMaterialInputNodes(DynamicMaterial->GetEditorOnlyData()->Refraction.Expression);
	ArrangeMaterialInputNodes(DynamicMaterial->GetEditorOnlyData()->AmbientOcclusion.Expression);
	ArrangeMaterialInputNodes(DynamicMaterial->GetEditorOnlyData()->PixelDepthOffset.Expression);

	FInt32Interval Vertical = {0, 0};

	for (const TPair<UMaterialExpression*, FIntPoint>& Pair : NodePositions)
	{
		Vertical.Min = FMath::Min(Vertical.Min, Pair.Value.Y);
		Vertical.Max = FMath::Max(Vertical.Max, Pair.Value.Y);
	}

	const int32 Offset = (Vertical.Max - Vertical.Min) / -2;

	for (const TPair<UMaterialExpression*, FIntPoint>& Pair : NodePositions)
	{
		Pair.Key->MaterialExpressionEditorY += Offset;
	}
}

void FDMMaterialNodeArranger::ArrangeMaterialInputNodes(UMaterialExpression* MaterialInputExpression)
{
	if (!MaterialInputExpression)
	{
		return;
	}

	FIntPoint NodeSize = {0, 0};
	ArrangeNode(NodePositions, OffsetStart, MaterialInputExpression, NodeSize);

	OffsetStart.X += NodeSize.X;
	OffsetStart.Y += NodeSize.Y + UE::DynamicMaterialEditor::BuildState::Private::SpaceBetweenNodes;
}

void FDMMaterialNodeArranger::ArrangeNode(TMap<UMaterialExpression*, FIntPoint>& InOutNodePositions, const FIntPoint& InOffsetStart, 
	UMaterialExpression* InNode, FIntPoint& InOutNodeSize)
{
	if (!InNode)
	{
		return;
	}

	TConstArrayView<FExpressionInput*> Inputs = InNode->GetInputsView();
	InOutNodeSize = {0, 0};
	const FIntPoint ThisNodeSize = {InNode->GetWidth() * 2, InNode->GetHeight()};
	FIntPoint ChildOffsetStart = InOffsetStart;
	ChildOffsetStart.X += ThisNodeSize.X + UE::DynamicMaterialEditor::BuildState::Private::SpaceBetweenNodes;

	for (FExpressionInput* Input : Inputs)
	{
		if (!Input->Expression)
		{
			continue;
		}

		FIntPoint ChildNodeSize;
		const FIntPoint* NodePosition = InOutNodePositions.Find(Input->Expression);

		if (NodePosition && (-NodePosition->X) > InOffsetStart.X)
		{
			ChildNodeSize = {Input->Expression->GetWidth(), Input->Expression->GetHeight()};
		}
		else
		{
			ArrangeNode(InOutNodePositions, ChildOffsetStart, Input->Expression, ChildNodeSize);
		}

		if (ChildNodeSize.X > 0)
		{
			InOutNodeSize.X = FMath::Max(InOutNodeSize.X, ChildNodeSize.X);
		}

		if (ChildNodeSize.Y > 0)
		{
			if (InOutNodeSize.Y > 0)
			{
				InOutNodeSize.Y += UE::DynamicMaterialEditor::BuildState::Private::SpaceBetweenNodes;
			}

			InOutNodeSize.Y += ChildNodeSize.Y;
		}

		ChildOffsetStart.Y = InOffsetStart.Y + InOutNodeSize.Y + UE::DynamicMaterialEditor::BuildState::Private::SpaceBetweenNodes;
	}

	if (InOutNodeSize.X > 0)
	{
		InOutNodeSize.X += UE::DynamicMaterialEditor::BuildState::Private::SpaceBetweenNodes;
	}

	InOutNodeSize.X += ThisNodeSize.X;
	InNode->MaterialExpressionEditorX = -InOffsetStart.X - ThisNodeSize.X;

	if (InOutNodeSize.Y == 0)
	{
		InOutNodeSize.Y = ThisNodeSize.Y;
		InNode->MaterialExpressionEditorY = InOffsetStart.Y;
	}
	else if (InOutNodeSize.Y <= ThisNodeSize.Y)
	{
		InOutNodeSize = ThisNodeSize.Y;
		InNode->MaterialExpressionEditorY = InOffsetStart.Y;
	}
	else
	{
		InNode->MaterialExpressionEditorY = InOffsetStart.Y + (InOutNodeSize.Y - ThisNodeSize.Y) / 2;
	}

	InOutNodePositions.Emplace(InNode, FIntPoint(InNode->MaterialExpressionEditorX, InNode->MaterialExpressionEditorY));
}
