// Copyright Epic Games, Inc. All Rights Reserved.
#include "SDetailsSplitter.h"

#include "AsyncDetailViewDiff.h"
#include "DetailTreeNode.h"
#include "IDetailsViewPrivate.h"
#include "PropertyNode.h"
#include "Styling/StyleColors.h"
#include "Framework/Application/SlateApplication.h"

// converts from a container helper like FScriptArrayHelper to a property like FArrayProperty
template<typename HelperType>
using TContainerPropertyType =
	std::conditional_t<std::is_same_v<HelperType, FScriptArrayHelper>, FArrayProperty,
	std::conditional_t<std::is_same_v<HelperType, FScriptMapHelper>, FMapProperty,
	std::conditional_t<std::is_same_v<HelperType, FScriptSetHelper>, FSetProperty,
	void>>>;

template<typename HelperType>
static bool TryGetSourceContainer(TSharedPtr<FDetailTreeNode> DetailsNode, FPropertyNode*& OutPropNode, TUniquePtr<HelperType>& OutResolvedProperty)
{
	using ContainerPropertyType = TContainerPropertyType<HelperType>;
	if (TryGetSourceContainer<ContainerPropertyType>(DetailsNode, OutPropNode))
	{
		const FPropertySoftPath SoftPropertyPath(FPropertyNode::CreatePropertyPath(OutPropNode->AsShared()).Get());
		const UObject* Object = DetailsNode->GetDetailsView()->GetSelectedObjects()[0].Get();
		const FResolvedProperty Resolved = SoftPropertyPath.Resolve(Object);
		const ContainerPropertyType* ContainerProperty = CastFieldChecked<ContainerPropertyType>(Resolved.Property);
		OutResolvedProperty = MakeUnique<HelperType>(ContainerProperty, ContainerProperty->template ContainerPtrToValuePtr<UObject*>(Resolved.Object));
		return true;
	}
	return false;
}

template<typename HelperType>
static bool TryGetDestinationContainer(TSharedPtr<FDetailTreeNode> DetailsNode, FPropertyNode*& OutPropNode, TUniquePtr<HelperType>& OutContainerHelper, int32& OutInsertIndex)
{
	using ContainerPropertyType = TContainerPropertyType<HelperType>;
	if (TryGetDestinationContainer<ContainerPropertyType>(DetailsNode, OutPropNode, OutInsertIndex))
	{
		const FPropertySoftPath SoftPropertyPath(FPropertyNode::CreatePropertyPath(OutPropNode->AsShared()).Get());
		const UObject* Object = DetailsNode->GetDetailsView()->GetSelectedObjects()[0].Get();
		const FResolvedProperty Resolved = SoftPropertyPath.Resolve(Object);
		const ContainerPropertyType* ContainerProperty = CastFieldChecked<ContainerPropertyType>(Resolved.Property);
		OutContainerHelper = MakeUnique<HelperType>(ContainerProperty, ContainerProperty->template ContainerPtrToValuePtr<UObject*>(Resolved.Object));
		return true;
	}
	return false;
}

template<typename ContainerPropertyType>
static bool TryGetSourceContainer(TSharedPtr<FDetailTreeNode> DetailsNode, FPropertyNode*& OutPropNode)
{
	if ((OutPropNode = DetailsNode->GetPropertyNode().Get()) == nullptr)
	{
		return false;
	}
	if ((OutPropNode = OutPropNode->GetParentNode()) == nullptr)
	{
		return false;
	}
	if (CastField<ContainerPropertyType>(OutPropNode->GetProperty()) != nullptr)
	{
		return true;
	}
	return false;
}

template<typename ContainerPropertyType>
static bool TryGetDestinationContainer(TSharedPtr<FDetailTreeNode> DetailsNode, FPropertyNode*& OutPropNode, int32& OutInsertIndex)
{
	if ((OutPropNode = DetailsNode->GetPropertyNode().Get()) == nullptr)
	{
		return false;
	}
	if (CastField<ContainerPropertyType>(OutPropNode->GetProperty()) != nullptr)
	{
		OutInsertIndex = 0;
		return true;
	}
	OutInsertIndex = OutPropNode->GetArrayIndex() + 1;
	while((OutPropNode = OutPropNode->GetParentNode()) != nullptr)
	{
		if (CastField<ContainerPropertyType>(OutPropNode->GetProperty()) != nullptr)
		{
			return true;
		}
		OutInsertIndex = OutPropNode->GetArrayIndex() + 1;
	}
	return false;
}

static void CopyPropertyValueForInsert(TSharedPtr<FDetailTreeNode> SourceDetailsNode, TSharedPtr<FDetailTreeNode> DestinationDetailsNode)
{
	TUniquePtr<FScriptArrayHelper> SourceArray;
	FPropertyNode* SourceArrayPropertyNode;
	if (TryGetSourceContainer(SourceDetailsNode, SourceArrayPropertyNode, SourceArray))
	{
		int32 InsertIndex;
		TUniquePtr<FScriptArrayHelper> DestinationArray;
		FPropertyNode* DestinationArrayPropertyNode;
		if (ensure(TryGetDestinationContainer(DestinationDetailsNode, DestinationArrayPropertyNode, DestinationArray, InsertIndex)))
		{
			DestinationArray->InsertValues(InsertIndex, 1);
			const void* SourceData = SourceArray->GetElementPtr(SourceDetailsNode->GetPropertyNode()->GetArrayIndex());
			void* DestinationData = DestinationArray->GetElementPtr(InsertIndex);
			const FArrayProperty* ArrayProperty = CastFieldChecked<FArrayProperty>(DestinationArrayPropertyNode->GetProperty());
			const FProperty* ElementProperty = ArrayProperty->Inner;
			ElementProperty->CopySingleValue(DestinationData, SourceData);
		}
	}
	
	TUniquePtr<FScriptSetHelper> SourceSet;
	FPropertyNode* SourceSetPropertyNode;
	if (TryGetSourceContainer(SourceDetailsNode, SourceSetPropertyNode, SourceSet))
	{
		int32 InsertIndex;
		TUniquePtr<FScriptSetHelper> DestinationSet;
		FPropertyNode* DestinationPropertyNode;
		if (ensure(TryGetDestinationContainer(DestinationDetailsNode, DestinationPropertyNode, DestinationSet, InsertIndex)))
		{
			const void* SourceData = SourceSet->FindNthElementPtr(SourceDetailsNode->GetPropertyNode()->GetArrayIndex());
			DestinationSet->AddElement(SourceData);
		}
	}
	
	TUniquePtr<FScriptMapHelper> SourceMap;
	FPropertyNode* SourceMapPropertyNode;
	if (TryGetSourceContainer(SourceDetailsNode, SourceMapPropertyNode, SourceMap))
	{
		int32 InsertIndex;
		TUniquePtr<FScriptMapHelper> DestinationMap;
		FPropertyNode* DestinationPropertyNode;
		if (ensure(TryGetDestinationContainer(DestinationDetailsNode, DestinationPropertyNode, DestinationMap, InsertIndex)))
		{
			const int32 Index = SourceMap->FindInternalIndex(SourceDetailsNode->GetPropertyNode()->GetArrayIndex());
			const void* SourceKey = SourceMap->GetKeyPtr(Index);
			const void* SourceVal = SourceMap->GetValuePtr(Index);
			DestinationMap->AddPair(SourceKey, SourceVal);
		}
	}
}

static void CopyPropertyValue(TSharedPtr<FDetailTreeNode> SourceDetailsNode, TSharedPtr<FDetailTreeNode> DestinationDetailsNode, ETreeDiffResult Diff)
{
	switch(Diff)
	{
		// traditional copy
		case ETreeDiffResult::DifferentValues:
			break;

		// insert
		case ETreeDiffResult::MissingFromTree1:
		case ETreeDiffResult::MissingFromTree2:
			CopyPropertyValueForInsert(SourceDetailsNode, DestinationDetailsNode);
			return;
		
		// no difference
		case ETreeDiffResult::Invalid:
		case ETreeDiffResult::Identical:
		default:
			return;
	}
	
	FResolvedProperty SourceResolved;
	FResolvedProperty DestinationResolved;
	bool bCopySingleValue = false;
	
	if (SourceDetailsNode)
	{
		const UObject* Object = SourceDetailsNode->GetDetailsView()->GetSelectedObjects()[0].Get();
		const FPropertyPath PropertyPath = SourceDetailsNode->GetPropertyPath();
		SourceResolved = FPropertySoftPath(PropertyPath).Resolve(Object);
		bCopySingleValue = PropertyPath.GetLeafMostProperty().ArrayIndex != INDEX_NONE;
	}
	if (DestinationDetailsNode)
	{
		const UObject* Object = DestinationDetailsNode->GetDetailsView()->GetSelectedObjects()[0].Get();
		const FPropertyPath PropertyPath = DestinationDetailsNode->GetPropertyPath();
		DestinationResolved = FPropertySoftPath(PropertyPath).Resolve(Object);
		bCopySingleValue = PropertyPath.GetLeafMostProperty().ArrayIndex != INDEX_NONE;
	}
		
	if (SourceResolved.Property && DestinationResolved.Property && SourceResolved.Object && DestinationResolved.Object)
	{
		const void* SourceData = SourceResolved.Property->ContainerPtrToValuePtr<void*>(SourceResolved.Object);
		void* DestinationData = (void*)DestinationResolved.Property->ContainerPtrToValuePtr<void*>(DestinationResolved.Object);
		if (bCopySingleValue)
		{
			DestinationResolved.Property->CopySingleValue(DestinationData, SourceData);
		}
		else
		{
			DestinationResolved.Property->CopyCompleteValue(DestinationData, SourceData);
		}
	}
}

// note: DestinationDetailsNode is the node before the position in the tree you wish to insert
static bool CanCopyPropertyValueForInsert(TSharedPtr<FDetailTreeNode> SourceDetailsNode, TSharedPtr<FDetailTreeNode> DestinationDetailsNode)
{
	FPropertyNode* SourceArrayPropertyNode;
	if (TryGetSourceContainer<FArrayProperty>(SourceDetailsNode, SourceArrayPropertyNode))
	{
		FPropertyNode* DestinationArrayPropertyNode;
		int32 InsertIndex;
		if (TryGetDestinationContainer<FArrayProperty>(DestinationDetailsNode, DestinationArrayPropertyNode, InsertIndex))
		{
			return true;
		}
		return false;
	}
	
	FPropertyNode* SourceSetPropertyNode;
	if (TryGetSourceContainer<FSetProperty>(SourceDetailsNode, SourceSetPropertyNode))
	{
		FPropertyNode* DestinationSetPropertyNode;
		int32 InsertIndex;
		if (TryGetDestinationContainer<FSetProperty>(DestinationDetailsNode, DestinationSetPropertyNode, InsertIndex))
		{
			return true;
		}
		return false;
	}
	
	FPropertyNode* SourceMapPropertyNode;
	if (TryGetSourceContainer<FMapProperty>(SourceDetailsNode, SourceMapPropertyNode))
	{
		FPropertyNode* DestinationMapPropertyNode;
		int32 InsertIndex;
		if (TryGetDestinationContainer<FMapProperty>(DestinationDetailsNode, DestinationMapPropertyNode, InsertIndex))
		{
			return true;
		}
		return false;
	}

	// you can only insert into containers
	return false;
}

static bool CanCopyPropertyValue(TSharedPtr<FDetailTreeNode> SourceDetailsNode, TSharedPtr<FDetailTreeNode> DestinationDetailsNode, ETreeDiffResult Diff)
{
	switch(Diff)
	{
	// traditional copy
	case ETreeDiffResult::DifferentValues:
		return true;

	// insert
	case ETreeDiffResult::MissingFromTree1:
	case ETreeDiffResult::MissingFromTree2:
		return CanCopyPropertyValueForInsert(SourceDetailsNode, DestinationDetailsNode);
		
	// no difference
	case ETreeDiffResult::Invalid:
	case ETreeDiffResult::Identical:
	default:
		return false;
	}
}

void SDetailsSplitter::Construct(const FArguments& InArgs)
{
	Splitter = SNew(SSplitter).PhysicalSplitterHandleSize(5.f);
	for(const FSlot::FSlotArguments& SlotArgs : InArgs._Slots)
	{
		AddSlot(SlotArgs);
	}
	
	ChildSlot
	[
		Splitter.ToSharedRef()
	];
}

void SDetailsSplitter::AddSlot(const FSlot::FSlotArguments& SlotArgs, int32 Index)
{
	if (Index == INDEX_NONE)
	{
		Index = Panels.Num();
	}
	
	Splitter->AddSlot(Index)
		.Value(SlotArgs._Value)
	[
		SNew(SBox).Padding(15.f,0.f, 15.f,0.f)
		[
			SlotArgs._DetailsView.ToSharedRef()
		]
	];
	Panels.Insert({
		SlotArgs._DetailsView,
		SlotArgs._IsReadonly,
		SlotArgs._DifferencesWithRightPanel,
	}, Index);
}

SDetailsSplitter::FPanel& SDetailsSplitter::GetPanel(int32 Index)
{
	return Panels[Index];
}

SDetailsSplitter::FSlot::FSlotArguments SDetailsSplitter::Slot()
{
	return FSlot::FSlotArguments(MakeUnique<FSlot>());
}

int32 SDetailsSplitter::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                                const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
                                const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 MaxLayerId = LayerId;
	MaxLayerId = Splitter->Paint(Args,AllottedGeometry,MyCullingRect,OutDrawElements,MaxLayerId,InWidgetStyle,bParentEnabled);
	++MaxLayerId;

	TMap<FSlateRect, FLinearColor> RowHighlights;
	for (int32 LeftIndex = 0; LeftIndex < Panels.Num(); ++LeftIndex)
	{
		const FPanel& LeftPanel = Panels[LeftIndex];
		if (!LeftPanel.DiffRight.IsBound())
		{
			continue;
		}
		
		if (const TSharedPtr<FAsyncDetailViewDiff> Diff = LeftPanel.DiffRight.Get())
		{
			FSlateRect PrevLeftPropertyRect;
			FSlateRect PrevRightPropertyRect;
			
			TSharedPtr<FDetailTreeNode> LastSeenRightDetailsNode;
			TSharedPtr<FDetailTreeNode> LastSeenLeftDetailsNode;
			
			Diff->ForEachRow([&](const TUniquePtr<FAsyncDetailViewDiff::DiffNodeType>& DiffNode, int32, int32)->ETreeTraverseControl
			{
				const FLinearColor Color = FLinearColor(0.f,1.f,1.f);
				
				FSlateRect LeftPropertyRect;
				if (const TSharedPtr<FDetailTreeNode> LeftDetailNode = DiffNode->ValueA.Pin())
				{
					LastSeenLeftDetailsNode = LeftDetailNode;
					// if the other tree doesn't have a matching node, treat this and all it's children as a single group
					const bool bIncludeChildren = !DiffNode->ValueB.IsValid();
					LeftPropertyRect = LeftPanel.DetailsView->GetPaintSpacePropertyBounds(LeftDetailNode.ToSharedRef(), bIncludeChildren);
				}
				if (!LeftPropertyRect.IsValid() && PrevLeftPropertyRect.IsValid())
				{
					LeftPropertyRect = PrevLeftPropertyRect;
					LeftPropertyRect.Top = LeftPropertyRect.Bottom;
				}
				if (LeftPropertyRect.IsValid() && DiffNode->DiffResult != ETreeDiffResult::Identical)
				{
					RowHighlights.Add(LeftPropertyRect, Color);
				}
		
				const int32 RightIndex = LeftIndex + 1;
				FSlateRect RightPropertyRect;
				if (Panels.IsValidIndex(RightIndex))
				{
					const FPanel& RightPanel = Panels[RightIndex];
					if (const TSharedPtr<FDetailTreeNode> RightDetailNode = DiffNode->ValueB.Pin())
					{
						LastSeenRightDetailsNode = RightDetailNode;
						// if the other tree doesn't have a matching node, treat this and all it's children as a single group
						const bool bIncludeChildren = !DiffNode->ValueA.IsValid();
						RightPropertyRect = RightPanel.DetailsView->GetPaintSpacePropertyBounds(RightDetailNode.ToSharedRef(), bIncludeChildren);
					}
					if (!RightPropertyRect.IsValid() && PrevRightPropertyRect.IsValid())
					{
						RightPropertyRect = PrevRightPropertyRect;
						RightPropertyRect.Top = RightPropertyRect.Bottom;
					}
					if (RightPropertyRect.IsValid() && DiffNode->DiffResult != ETreeDiffResult::Identical)
					{
						RowHighlights.Add(RightPropertyRect, Color);
					}
					
					if (LeftPropertyRect.IsValid() && RightPropertyRect.IsValid() && DiffNode->DiffResult != ETreeDiffResult::Identical)
					{
						FLinearColor FillColor = Color.Desaturate(.3f) * FLinearColor(0.04f,0.04f,0.04f);
						FillColor.A = 0.3f;
			
						FLinearColor OutlineColor = Color;
						OutlineColor.A = 0.7f;
						PaintPropertyConnector(OutDrawElements, MaxLayerId, LeftPropertyRect, RightPropertyRect, FillColor, OutlineColor);
						++MaxLayerId;

						if (!RightPanel.IsReadonly.Get(true) && CanCopyPropertyValue(LastSeenLeftDetailsNode, LastSeenRightDetailsNode, DiffNode->DiffResult))
						{
							PaintCopyPropertyButton(OutDrawElements, MaxLayerId, DiffNode, LeftPropertyRect, RightPropertyRect, EPropertyCopyDirection::CopyLeftToRight);
						}
						if (!LeftPanel.IsReadonly.Get(true) && CanCopyPropertyValue(LastSeenRightDetailsNode, LastSeenLeftDetailsNode, DiffNode->DiffResult))
						{
							PaintCopyPropertyButton(OutDrawElements, MaxLayerId, DiffNode, LeftPropertyRect, RightPropertyRect, EPropertyCopyDirection::CopyRightToLeft);
						}
						++MaxLayerId;
						
					}
				}
		
				PrevLeftPropertyRect = MoveTemp(LeftPropertyRect);
				PrevRightPropertyRect = MoveTemp(RightPropertyRect);

				// only traverse children if both trees have them
				if (!DiffNode->ValueA.IsValid() || !DiffNode->ValueB.IsValid())
				{
					return ETreeTraverseControl::SkipChildren;
				}
				return ETreeTraverseControl::Continue;
				
			});
		}
	}

	for (const auto& [PropertyRect, Color] : RowHighlights)
	{
		FLinearColor FillColor = Color.Desaturate(.3) * FLinearColor(0.04f,0.04f,0.04f);
		FillColor.A = 0.3f;
		
		FPaintGeometry Geometry(
			PropertyRect.GetTopLeft() + FVector2D{0.f,2.f},
			PropertyRect.GetSize() - FVector2D{0.f,4.f},
			1.f
		);
		
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 8, // draw in front of background but behind text, buttons, etc
			Geometry,
			FAppStyle::GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			FillColor
		);
	}
	
	return MaxLayerId;
}

FReply SDetailsSplitter::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	HoveredCopyButton = {};
	const FVector2D MousePosition = MouseEvent.GetScreenSpacePosition();
	
	for (int32 LeftIndex = 0; LeftIndex < Panels.Num() - 1; ++LeftIndex)
	{
		const FPanel& LeftPanel = Panels[LeftIndex];
		if (!LeftPanel.DiffRight.IsBound())
		{
			continue;
		}

		
		const int32 RightIndex = LeftIndex + 1;
		const FPanel& RightPanel = Panels[RightIndex];
		if (LeftPanel.IsReadonly.Get(true) && RightPanel.IsReadonly.Get(true))
		{
			continue;
		}
		if (const TSharedPtr<FAsyncDetailViewDiff> Diff = LeftPanel.DiffRight.Get())
		{
			
			TSharedPtr<FDetailTreeNode> LastSeenRightDetailsNode;
			TSharedPtr<FDetailTreeNode> LastSeenLeftDetailsNode;
			Diff->ForEachRow([&LastSeenLeftDetailsNode, &LastSeenRightDetailsNode, &LeftPanel, &RightPanel, &MousePosition, &HoveredCopyButton = HoveredCopyButton]
			(const TUniquePtr<FAsyncDetailViewDiff::DiffNodeType>& DiffNode, int32, int32)->ETreeTraverseControl
			{
				
				FSlateRect LeftPropertyRect;
				if (const TSharedPtr<FDetailTreeNode> LeftDetailNode = DiffNode->ValueA.Pin())
				{
					LastSeenLeftDetailsNode = LeftDetailNode;
					const bool bIncludeChildren = !DiffNode->ValueB.IsValid();
					LeftPropertyRect = LeftPanel.DetailsView->GetTickSpacePropertyBounds(LeftDetailNode.ToSharedRef(), bIncludeChildren);
				}
		
				FSlateRect RightPropertyRect;
				if (const TSharedPtr<FDetailTreeNode> RightDetailNode = DiffNode->ValueB.Pin())
				{
					LastSeenRightDetailsNode = RightDetailNode;
					const bool bIncludeChildren = !DiffNode->ValueA.IsValid();
					RightPropertyRect = RightPanel.DetailsView->GetTickSpacePropertyBounds(RightDetailNode.ToSharedRef(), bIncludeChildren);
				}
				
				if (DiffNode->DiffResult == ETreeDiffResult::Identical)
				{
					return ETreeTraverseControl::Continue;
				}

				if (LeftPropertyRect.IsValid() && !RightPanel.IsReadonly.Get(true))
				{
					const FSlateRect CopyButtonZoneLeftToRight = FSlateRect(
						LeftPropertyRect.Right,
						LeftPropertyRect.Top,
						LeftPropertyRect.Right + 15.f,
						LeftPropertyRect.Bottom
					);

					if (CopyButtonZoneLeftToRight.ContainsPoint(MousePosition))
					{
						if (CanCopyPropertyValue(LastSeenLeftDetailsNode, LastSeenRightDetailsNode, DiffNode->DiffResult))
						{
							HoveredCopyButton = {
								LastSeenLeftDetailsNode,
								LastSeenRightDetailsNode,
								DiffNode->DiffResult,
								EPropertyCopyDirection::CopyLeftToRight
							};
						}
						return ETreeTraverseControl::Break;
					}
				}
				
				if (RightPropertyRect.IsValid() && !LeftPanel.IsReadonly.Get(true))
				{
					const FSlateRect CopyButtonZoneRightToLeft = FSlateRect(
						RightPropertyRect.Left - 15.f,
						RightPropertyRect.Top,
						RightPropertyRect.Left,
						RightPropertyRect.Bottom
					);

					if (CopyButtonZoneRightToLeft.ContainsPoint(MousePosition))
					{
						if (CanCopyPropertyValue(LastSeenRightDetailsNode, LastSeenLeftDetailsNode, DiffNode->DiffResult))
						{
							HoveredCopyButton = {
								LastSeenRightDetailsNode,
								LastSeenLeftDetailsNode,
								DiffNode->DiffResult,
								EPropertyCopyDirection::CopyRightToLeft
							};
						}
						return ETreeTraverseControl::Break;
					}
				}
				
				return ETreeTraverseControl::Continue;
			});

			if (HoveredCopyButton.CopyDirection != EPropertyCopyDirection::Copy_None)
			{
				break;
			}
		}
	}
	return SCompoundWidget::OnMouseMove(MyGeometry, MouseEvent);
}

void SDetailsSplitter::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	HoveredCopyButton = {};
	SCompoundWidget::OnMouseLeave(MouseEvent);
}

FReply SDetailsSplitter::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	
	if (HoveredCopyButton.CopyDirection == EPropertyCopyDirection::Copy_None)
	{
		return FReply::Unhandled();
	}

	CopyPropertyValue(
		HoveredCopyButton.SourceDetailsNode.Pin(),
		HoveredCopyButton.DestinationDetailsNode.Pin(),
		HoveredCopyButton.DiffResult);
	return FReply::Handled();
}

void SDetailsSplitter::PaintPropertyConnector(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FSlateRect& LeftPropertyRect,
	const FSlateRect& RightPropertyRect, const FLinearColor& FillColor, const FLinearColor& OutlineColor) const
{
	FVector2D TopLeft = LeftPropertyRect.GetTopRight();
	FVector2D BottomLeft = LeftPropertyRect.GetBottomRight();
	
	FVector2D TopRight = RightPropertyRect.GetTopLeft();
	FVector2D BottomRight = RightPropertyRect.GetBottomLeft();

	{
		constexpr float YPadding = 2.f;
		if (BottomLeft.Y - TopLeft.Y > YPadding * 2.f)
		{
			BottomLeft.Y -= YPadding;
			TopLeft.Y += YPadding;
		}
		if (BottomRight.Y - TopRight.Y > YPadding * 2.f)
		{
			BottomRight.Y -= YPadding;
			TopRight.Y += YPadding;
		}
	}
	
	TArray<FSlateVertex> FillVerts;
	TArray<SlateIndex> FillIndices;
	TArray<FVector2D> TopBoarderLine;
	TArray<FVector2D> BottomBoarderLine;

	auto AddVert = [&FillVerts, &FillIndices](const FVector2D& Position, const FColor& VertColor)
	{
		FillVerts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
			FSlateRenderTransform(),
			FVector2f(Position),
			{0.f,0.f},
			VertColor
		));
		if (FillVerts.Num() >= 3)
		{
			FillIndices.Add(FillVerts.Num()-3);
			FillIndices.Add(FillVerts.Num()-2);
			FillIndices.Add(FillVerts.Num()-1);
		}
	};

	// interpolate between left and right corners and add vertices to the mesh
	constexpr float StepSize = 1.f / 30.f;
	constexpr float InterpBoarder = .3f; // make room for buttons

		FillVerts.Empty();
		FillIndices.Empty();
		TopBoarderLine.Empty();
		BottomBoarderLine.Empty();

	float Alpha = 0.f;
	while (true)
	{
		constexpr float Exp = 3.f;
		const float TopAlpha = FMath::Clamp(Alpha, 0.f, 1.f);
		const float BottomAlpha = FMath::Clamp(Alpha, 0.f, 1.f);
		const double TopX = FMath::Lerp(TopLeft.X, TopRight.X, TopAlpha);
		const double BottomX = FMath::Lerp(TopLeft.X, TopRight.X, BottomAlpha);
		
		constexpr float InterpolationRange = 1.f - 2.f * InterpBoarder;

		const float TopTransformedAlpha = FMath::Clamp((TopAlpha - InterpBoarder) / InterpolationRange, 0.f, 1.f);
		const float BottomTransformedAlpha = FMath::Clamp((BottomAlpha - InterpBoarder) / InterpolationRange, 0.f, 1.f);
		
		const double TopY = FMath::InterpEaseInOut(TopLeft.Y, TopRight.Y, TopTransformedAlpha, Exp);
		const double BottomY = FMath::InterpEaseInOut(BottomLeft.Y, BottomRight.Y, BottomTransformedAlpha, Exp);

		FLinearColor ColumnColor = FillColor;
		if (Alpha <= 0.5f)
		{
			ColumnColor.A = FMath::InterpEaseOut(FillColor.A, 1.f, Alpha * 2.f, 2.f);
		}
		else
		{
			ColumnColor.A = FMath::InterpEaseIn( 1.f,FillColor.A, (Alpha - 0.5f) * 2.f, 2.f);
		}
	
		TopBoarderLine.Emplace(TopX,TopY);
		FLinearColor TopColor = ColumnColor.Desaturate(0.5f);
		AddVert(TopBoarderLine.Last(), TopColor.ToFColorSRGB());
	
		BottomBoarderLine.Emplace(BottomX,BottomY);
		FLinearColor BottomColor = ColumnColor;
		AddVert(BottomBoarderLine.Last(), BottomColor.ToFColorSRGB());
	
		if (Alpha >= 1.f)
		{
			break;
		}
		Alpha = FMath::Clamp(Alpha + StepSize, 0.f, 1.f);
	}
	
	const FSlateResourceHandle ResourceHandle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FAppStyle::GetBrush("WhiteBrush"));
	FSlateDrawElement::MakeCustomVerts(
		OutDrawElements,
		LayerId,
		ResourceHandle,
		FillVerts,
		FillIndices,
		nullptr,
		0,
		0,
		ESlateDrawEffect::None
	);

	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId,
		FPaintGeometry(),
		TopBoarderLine,
		ESlateDrawEffect::None,
		OutlineColor,
		true,
		0.5f
	);

	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId,
		FPaintGeometry(),
		BottomBoarderLine,
		ESlateDrawEffect::None,
		OutlineColor,
		true,
		0.5f
	);
}

void SDetailsSplitter::PaintCopyPropertyButton(FSlateWindowElementList& OutDrawElements, int32 LayerId, const TUniquePtr<FAsyncDetailViewDiff::DiffNodeType>& DiffNode,
	const FSlateRect& LeftPropertyRect, const FSlateRect& RightPropertyRect, EPropertyCopyDirection CopyDirection) const
{
	constexpr float ButtonScale = 15.f;
	FPaintGeometry Geometry;
	const FSlateBrush* Brush = nullptr;
	FLinearColor ButtonColor = FStyleColors::Foreground.GetSpecifiedColor();
	switch (CopyDirection)
	{
	case EPropertyCopyDirection::CopyLeftToRight:
		if (LeftPropertyRect.GetSize().Y < UE_SMALL_NUMBER)
		{
			// space is too small to fit a button
			return;
		}
		Geometry = FPaintGeometry(
			FVector2D(LeftPropertyRect.Right, LeftPropertyRect.GetCenter().Y - 0.5f * ButtonScale),
			FVector2D(ButtonScale),
			1.f
		);
		Brush = FAppStyle::GetBrush("BlueprintDif.CopyPropertyRight");
		
		if (HoveredCopyButton.CopyDirection == EPropertyCopyDirection::CopyLeftToRight)
		{
			if (HoveredCopyButton.SourceDetailsNode == DiffNode->ValueA)
			{
				ButtonColor = FStyleColors::ForegroundHover.GetSpecifiedColor();
			}
		}
		break;
	case EPropertyCopyDirection::CopyRightToLeft:
		if (RightPropertyRect.GetSize().Y < UE_SMALL_NUMBER)
		{
			// space is too small to fit a button
			return;
		}
		Geometry = FPaintGeometry(
			FVector2D(RightPropertyRect.Left - ButtonScale, RightPropertyRect.GetCenter().Y - 0.5f * ButtonScale),
			FVector2D(ButtonScale),
			1.f
		);
		Brush = FAppStyle::GetBrush("BlueprintDif.CopyPropertyLeft");
		
		if (HoveredCopyButton.CopyDirection == EPropertyCopyDirection::CopyRightToLeft)
		{
			if (HoveredCopyButton.SourceDetailsNode == DiffNode->ValueB)
			{
				ButtonColor = FStyleColors::ForegroundHover.GetSpecifiedColor();
			}
		}
		break;
	default:
		return;
	}
	
		
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId, // draw in front of background but behind text, buttons, etc
		Geometry,
		Brush,
		ESlateDrawEffect::None,
		ButtonColor
	);
}