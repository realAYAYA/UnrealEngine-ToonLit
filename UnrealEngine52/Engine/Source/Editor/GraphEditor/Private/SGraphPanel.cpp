// Copyright Epic Games, Inc. All Rights Reserved.


#include "SGraphPanel.h"

#include "AssetRegistry/AssetData.h"
#include "AssetSelection.h"
#include "ConnectionDrawingPolicy.h"
#include "Containers/EnumAsByte.h"
#include "Containers/SparseArray.h"
#include "DiffResults.h"
#include "DragAndDrop/ActorDragDropGraphEdOp.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "DragAndDrop/GraphNodeDragDropOp.h"
#include "DragAndDrop/LevelDragDropOp.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphNode_Comment.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/World.h"
#include "Framework/Application/IMenu.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/InputChord.h"
#include "Framework/Commands/UICommandInfo.h"
#include "GraphEditAction.h"
#include "GraphEditorActions.h"
#include "GraphEditorDragDropAction.h"
#include "GraphEditorSettings.h"
#include "HAL/PlatformCrt.h"
#include "Input/DragAndDrop.h"
#include "InputCoreTypes.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "KismetNodes/KismetNodeInfoContext.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/ArrangedWidget.h"
#include "Layout/Children.h"
#include "Layout/PaintGeometry.h"
#include "Layout/SlateRect.h"
#include "Layout/Visibility.h"
#include "Layout/WidgetPath.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "MarqueeOperation.h"
#include "Math/Color.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Guid.h"
#include "Misc/Optional.h"
#include "NodeFactory.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Rendering/DrawElements.h"
#include "Rendering/RenderingCommon.h"
#include "Rendering/SlateLayoutTransform.h"
#include "SGraphNode.h"
#include "ScopedTransaction.h"
#include "Settings/EditorExperimentalSettings.h"
#include "Settings/EditorStyleSettings.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/WidgetStyle.h"
#include "Templates/Casts.h"
#include "Templates/Tuple.h"
#include "Templates/TypeHash.h"
#include "Templates/UnrealTemplate.h"
#include "Trace/Detail/Channel.h"
#include "Types/PaintArgs.h"
#include "Types/SlateAttributeMetaData.h"
#include "Types/WidgetActiveTimerDelegate.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/InvalidateWidgetReason.h"
#include "Widgets/SWidget.h"
#include "Widgets/SWindow.h"

DEFINE_LOG_CATEGORY_STATIC(LogGraphPanel, Log, All);

//////////////////////////////////////////////////////////////////////////
// SGraphPanel

void SGraphPanel::Construct( const SGraphPanel::FArguments& InArgs )
{
	SNodePanel::Construct();

	this->OnGetContextMenuFor = InArgs._OnGetContextMenuFor;
	this->GraphObj = InArgs._GraphObj;
	this->DiffResults = InArgs._DiffResults;
	this->FocusedDiffResult = InArgs._FocusedDiffResult;
	this->SelectionManager.OnSelectionChanged = InArgs._OnSelectionChanged;
	this->IsEditable = InArgs._IsEditable;
	this->DisplayAsReadOnly = InArgs._DisplayAsReadOnly;
	this->OnNodeDoubleClicked = InArgs._OnNodeDoubleClicked;
	this->OnDropActor = InArgs._OnDropActor;
	this->OnDropStreamingLevel = InArgs._OnDropStreamingLevel;
	this->OnVerifyTextCommit = InArgs._OnVerifyTextCommit;
	this->OnTextCommitted = InArgs._OnTextCommitted;
	this->OnSpawnNodeByShortcut = InArgs._OnSpawnNodeByShortcut;
	this->OnUpdateGraphPanel = InArgs._OnUpdateGraphPanel;
	this->OnDisallowedPinConnection = InArgs._OnDisallowedPinConnection;
	this->OnDoubleClicked = InArgs._OnDoubleClicked;

	this->bPreservePinPreviewConnection = false;
	this->PinVisibility = SGraphEditor::Pin_Show;

	CachedAllottedGeometryScaledSize = FVector2D(160, 120);
	if (InArgs._InitialZoomToFit)
	{
		ZoomToFit(/*bOnlySelection=*/ false);
		bTeleportInsteadOfScrollingWhenZoomingToFit = true;
	}

	BounceCurve.AddCurve(0.0f, 1.0f);

	FEditorDelegates::BeginPIE.AddRaw( this, &SGraphPanel::OnBeginPIE );
	FEditorDelegates::EndPIE.AddRaw( this, &SGraphPanel::OnEndPIE );

	// Register for notifications
	MyRegisteredGraphChangedDelegate = FOnGraphChanged::FDelegate::CreateSP(this, &SGraphPanel::OnGraphChanged);
	MyRegisteredGraphChangedDelegateHandle = this->GraphObj->AddOnGraphChangedHandler(MyRegisteredGraphChangedDelegate);
	
	ShowGraphStateOverlay = InArgs._ShowGraphStateOverlay;

	SavedMousePosForOnPaintEventLocalSpace = FVector2D::ZeroVector;
	PreviousFrameSavedMousePosForSplineOverlap = FVector2D::ZeroVector;

	TimeLeftToInvalidatePerTick = 0.0f;
}

SGraphPanel::~SGraphPanel()
{
	FEditorDelegates::BeginPIE.RemoveAll( this );
	FEditorDelegates::EndPIE.RemoveAll( this );

	this->GraphObj->RemoveOnGraphChangedHandler(MyRegisteredGraphChangedDelegateHandle);
}

//////////////////////////////////////////////////////////////////////////

int32 SGraphPanel::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	CachedAllottedGeometryScaledSize = AllottedGeometry.GetLocalSize() * AllottedGeometry.Scale;

	//Style used for objects that are the same between revisions
	FWidgetStyle FadedStyle = InWidgetStyle;
	FadedStyle.BlendColorAndOpacityTint(FLinearColor(0.45f,0.45f,0.45f,0.30f));

	// First paint the background
	const UEditorExperimentalSettings& Options = *GetDefault<UEditorExperimentalSettings>();

	const FSlateBrush* DefaultBackground = FAppStyle::GetBrush(TEXT("Graph.Panel.SolidBackground"));
	const FSlateBrush* CustomBackground = &GetDefault<UEditorStyleSettings>()->GraphBackgroundBrush;
	const FSlateBrush* BackgroundImage = CustomBackground->HasUObject() ? CustomBackground : DefaultBackground;
	PaintBackgroundAsLines(BackgroundImage, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId);

	const float ZoomFactor = AllottedGeometry.Scale * GetZoomAmount();

	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	ArrangeChildNodes(AllottedGeometry, ArrangedChildren);

	// Determine some 'global' settings based on current LOD
	const bool bDrawShadowsThisFrame = GetCurrentLOD() > EGraphRenderingLOD::LowestDetail;

	// Because we paint multiple children, we must track the maximum layer id that they produced in case one of our parents
	// wants to an overlay for all of its contents.

	// Save LayerId for comment boxes to ensure they always appear below nodes & wires
	const int32 CommentNodeShadowLayerId = LayerId++;
	const int32 CommentNodeLayerId = LayerId++;

	const int32 NodeDiffHighlightLayerID = LayerId++;

	// Save a LayerId for wires, which appear below nodes but above comments
	// We will draw them later, along with the arrows which appear above nodes.
	const int32 WireLayerId = LayerId++;

	const int32 NodeShadowsLayerId = LayerId;
	const int32 NodeLayerId = NodeShadowsLayerId + 1;
	int32 MaxLayerId = NodeLayerId;

	const FPaintArgs NewArgs = Args.WithNewParent(this);

	const FVector2D NodeShadowSize = GetDefault<UGraphEditorSettings>()->GetShadowDeltaSize();
	const UEdGraphSchema* Schema = GraphObj->GetSchema();

	
	// If we were provided diff results, organize those by owner
	TMap<UEdGraphNode*, FDiffSingleResult> NodeDiffResults;
	TMap<UEdGraphPin*, FDiffSingleResult> PinDiffResults;
	if (DiffResults.IsValid())
	{
		// diffs with Node1/Pin1 get precedence so set those first
		for (const FDiffSingleResult& Result : *DiffResults)
		{
			if (Result.Pin1)
			{
				PinDiffResults.FindOrAdd(Result.Pin1, Result);

				// when zoomed out, make it easier to see diffed pins by also highlighting the node
				if(ZoomLevel <= 6)
				{
					NodeDiffResults.FindOrAdd(Result.Pin1->GetOwningNode(), Result);
				}
			}
			else if (Result.Node1)
			{
				NodeDiffResults.FindOrAdd(Result.Node1, Result);
			}
		}

		// only diffs with Node2/Pin2 if those nodes don't already have a diff result
		for (const FDiffSingleResult& Result : *DiffResults)
		{
			if (Result.Pin2)
			{
				PinDiffResults.FindOrAdd(Result.Pin2, Result);

				// when zoomed out, make it easier to see diffed pins by also highlighting the node
				if(ZoomLevel <= 6)
				{
					NodeDiffResults.FindOrAdd(Result.Pin2->GetOwningNode(), Result);
				}
			}
			else if (!Result.Pin1 && Result.Node2)
			{
				NodeDiffResults.FindOrAdd(Result.Node2, Result);
			}
		}
	}

	// Draw the child nodes
	{
		// When drawing a marquee, need a preview of what the selection will be.
		const FGraphPanelSelectionSet* SelectionToVisualize = &(SelectionManager.SelectedNodes);
		FGraphPanelSelectionSet SelectionPreview;
		if ( Marquee.IsValid() )
		{			
			ApplyMarqueeSelection(Marquee, SelectionManager.SelectedNodes, SelectionPreview);
			SelectionToVisualize = &SelectionPreview;
		}

		// Context for rendering node infos
		FKismetNodeInfoContext Context(GraphObj);

		for (int32 ChildIndex = 0; ChildIndex < ArrangedChildren.Num(); ++ChildIndex)
		{
			FArrangedWidget& CurWidget = ArrangedChildren[ChildIndex];
			TSharedRef<SGraphNode> ChildNode = StaticCastSharedRef<SGraphNode>(CurWidget.Widget);
			
			// Examine node to see what layers we should be drawing in
			int32 ShadowLayerId = NodeShadowsLayerId;
			int32 ChildLayerId = NodeLayerId;

			// If a comment node, draw in the dedicated comment slots
			{
				UObject* NodeObj = ChildNode->GetObjectBeingDisplayed();
				if (NodeObj && NodeObj->IsA(UEdGraphNode_Comment::StaticClass()))
				{
					ShadowLayerId = CommentNodeShadowLayerId;
					ChildLayerId = CommentNodeLayerId;
				}
			}


			const bool bNodeIsVisible = FSlateRect::DoRectanglesIntersect( CurWidget.Geometry.GetLayoutBoundingRect(), MyCullingRect );

			if (bNodeIsVisible)
			{
				const bool bSelected = SelectionToVisualize->Contains( StaticCastSharedRef<SNodePanel::SNode>(CurWidget.Widget)->GetObjectBeingDisplayed() );
				
				UEdGraphNode* NodeObj = Cast<UEdGraphNode>(ChildNode->GetObjectBeingDisplayed());
				float Alpha = 1.0f;

				// Handle Node renaming once the node is visible
				if( bSelected && ChildNode->IsRenamePending() )
				{
					// Only open a rename when the window has focus
					TSharedPtr<SWindow> OwnerWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
					if (!OwnerWindow.IsValid() || FSlateApplication::Get().HasFocusedDescendants(OwnerWindow.ToSharedRef()))
					{
						ChildNode->ApplyRename();
					}
				}

				/** if this graph is being diffed, highlight the changes in the graph */
				if(DiffResults.IsValid())
				{
					/** When diffing nodes, color code shadow based on diff result */
					if (NodeDiffResults.Contains(NodeObj))
					{
						const FDiffSingleResult& DiffResult = NodeDiffResults[NodeObj];
						for (const SNode::DiffHighlightInfo& Highlight : ChildNode->GetDiffHighlights(DiffResult))
						{
							FSlateDrawElement::MakeBox(
								OutDrawElements,
								NodeDiffHighlightLayerID,
								CurWidget.Geometry.ToInflatedPaintGeometry(NodeShadowSize),
								Highlight.Brush,
								ESlateDrawEffect::None,
								Highlight.Tint
								);
						}
					}
				}
				
				/** When diffing, set the backround of the differing pins to their diff colors */
				for (UEdGraphPin* Pin : NodeObj->Pins)
				{
					if (TSharedPtr<SGraphPin> PinWidget = ChildNode->FindWidgetForPin(Pin))
					{
						if (FDiffSingleResult* DiffResult = PinDiffResults.Find(Pin))
						{
							// if the diff result associated with this pin is focused, highlight the pin
							if (DiffResults.IsValid() && FocusedDiffResult.IsSet())
							{
								const int32 Index = FocusedDiffResult.Get();
								if (DiffResults->IsValidIndex(Index))
								{
									const FDiffSingleResult& Focused = (*DiffResults)[Index];
									PinWidget->SetDiffHighlighted(*DiffResult == Focused);
								}
							}
						
							FLinearColor PinDiffColor = DiffResult->GetDisplayColor();
							PinDiffColor.A = 0.7f;
							PinWidget->SetPinDiffColor(PinDiffColor);
							PinWidget->SetFadeConnections(false);
						}
						else
						{
							PinWidget->SetDiffHighlighted(false);
							PinWidget->SetPinDiffColor(TOptional<FLinearColor>());

							// when zoomed out, fade out pin connections that aren't involved in a diff
							PinWidget->SetFadeConnections(ZoomLevel <= 6 && (!NodeDiffResults.Contains(NodeObj) || NodeDiffResults[NodeObj].Pin1));
						}
					}
				}

				// Draw the node's shadow.
				if (bDrawShadowsThisFrame || bSelected)
				{
					const FSlateBrush* ShadowBrush = ChildNode->GetShadowBrush(bSelected);
					FSlateDrawElement::MakeBox(
						OutDrawElements,
						ShadowLayerId,
						CurWidget.Geometry.ToInflatedPaintGeometry(NodeShadowSize),
						ShadowBrush,
						ESlateDrawEffect::None,
						FLinearColor(1.0f, 1.0f, 1.0f, Alpha)
						);
				}

				// Draw the comments and information popups for this node, if it has any.
				{
					const SNodePanel::SNode::FNodeSlot* CommentSlot = ChildNode->GetSlot( ENodeZone::TopCenter );
					float CommentBubbleY = CommentSlot ? -CommentSlot->GetSlotOffset().Y : 0.f;
					Context.bSelected = bSelected;
					TArray<FGraphInformationPopupInfo> Popups;

					{
						ChildNode->GetNodeInfoPopups(&Context, /*out*/ Popups);
					}

					for (int32 PopupIndex = 0; PopupIndex < Popups.Num(); ++PopupIndex)
					{
						FGraphInformationPopupInfo& Popup = Popups[PopupIndex];
						PaintComment(Popup.Message, CurWidget.Geometry, MyCullingRect, OutDrawElements, ChildLayerId, Popup.BackgroundColor, /*inout*/ CommentBubbleY, InWidgetStyle);
					}
				}

				int32 CurWidgetsMaxLayerId;
				{
					/* When dragging off a pin, we want to duck the alpha of some nodes */
					TSharedPtr< SGraphPin > OnlyStartPin = (1 == PreviewConnectorFromPins.Num()) ? PreviewConnectorFromPins[0].FindInGraphPanel(*this) : TSharedPtr< SGraphPin >();
					const bool bNodeIsNotUsableInCurrentContext = Schema->FadeNodeWhenDraggingOffPin(NodeObj, OnlyStartPin.IsValid() ? OnlyStartPin.Get()->GetPinObj() : nullptr);
					
					const bool bCleanDiff = DiffResults.IsValid() && !NodeDiffResults.Contains(NodeObj);
					
					FWidgetStyle NodeStyleToUse = InWidgetStyle;
					if (bNodeIsNotUsableInCurrentContext)
					{
						NodeStyleToUse = FadedStyle;
					}
					else if (ZoomLevel <= 6 && bCleanDiff)
					{
						NodeStyleToUse = FadedStyle;
					}
					NodeStyleToUse.BlendColorAndOpacityTint(FLinearColor(1.0f, 1.0f, 1.0f, Alpha));

					// Draw the node.
					CurWidgetsMaxLayerId = CurWidget.Widget->Paint(NewArgs, CurWidget.Geometry, MyCullingRect, OutDrawElements, ChildLayerId, NodeStyleToUse, !DisplayAsReadOnly.Get() && ShouldBeEnabled( bParentEnabled ) );
				}

				// Draw the node's overlay, if it has one.
				{
					// Get its size
					const FVector2D WidgetSize = CurWidget.Geometry.Size;

					{
						TArray<FOverlayBrushInfo> OverlayBrushes;
						ChildNode->GetOverlayBrushes(bSelected, WidgetSize, /*out*/ OverlayBrushes);

						for (int32 BrushIndex = 0; BrushIndex < OverlayBrushes.Num(); ++BrushIndex)
						{
							FOverlayBrushInfo& OverlayInfo = OverlayBrushes[BrushIndex];
							const FSlateBrush* OverlayBrush = OverlayInfo.Brush;
							if (OverlayBrush != nullptr)
							{
								FPaintGeometry BouncedGeometry = CurWidget.Geometry.ToPaintGeometry(OverlayBrush->ImageSize, FSlateLayoutTransform(OverlayInfo.OverlayOffset));

								// Handle bouncing during PIE
								const float BounceValue = FMath::Sin(2.0f * PI * BounceCurve.GetLerp());
								BouncedGeometry.DrawPosition += FVector2f(OverlayInfo.AnimationEnvelope * BounceValue * ZoomFactor);

								FLinearColor FinalColorAndOpacity(InWidgetStyle.GetColorAndOpacityTint()* OverlayBrush->GetTint(InWidgetStyle));
								//FinalColorAndOpacity.A = Alpha;

								CurWidgetsMaxLayerId++;
								FSlateDrawElement::MakeBox(
									OutDrawElements,
									CurWidgetsMaxLayerId,
									BouncedGeometry,
									OverlayBrush,
									ESlateDrawEffect::None,
									FinalColorAndOpacity
									);
							}

						}
					}

					{
						TArray<FOverlayWidgetInfo> OverlayWidgets = ChildNode->GetOverlayWidgets(bSelected, WidgetSize);

						for (int32 WidgetIndex = 0; WidgetIndex < OverlayWidgets.Num(); ++WidgetIndex)
						{
							FOverlayWidgetInfo& OverlayInfo = OverlayWidgets[WidgetIndex];
							if (SWidget* Widget = OverlayInfo.Widget.Get())
							{
								FSlateAttributeMetaData::UpdateOnlyVisibilityAttributes(*Widget, FSlateAttributeMetaData::EInvalidationPermission::AllowInvalidationIfConstructed);
								if (Widget->GetVisibility() == EVisibility::Visible)
								{
									// call SlatePrepass as these widgets are not in the 'normal' child hierarchy
									Widget->SlatePrepass(AllottedGeometry.GetAccumulatedLayoutTransform().GetScale());

									const FGeometry WidgetGeometry = CurWidget.Geometry.MakeChild(Widget->GetDesiredSize(), FSlateLayoutTransform(OverlayInfo.OverlayOffset));

									Widget->Paint(NewArgs, WidgetGeometry, MyCullingRect, OutDrawElements, CurWidgetsMaxLayerId, InWidgetStyle, bParentEnabled);
								}
							}
						}
					}
				}

				MaxLayerId = FMath::Max( MaxLayerId, CurWidgetsMaxLayerId + 1 );
			}
		}
	}

	MaxLayerId += 1;

	// Draw connections between pins 
	if (Children.Num() > 0 )
	{
		FConnectionDrawingPolicy* ConnectionDrawingPolicy = nullptr;
		if (NodeFactory.IsValid())
		{
			ConnectionDrawingPolicy = NodeFactory->CreateConnectionPolicy(Schema, WireLayerId, MaxLayerId, ZoomFactor, MyCullingRect, OutDrawElements, GraphObj);
		}
		else
		{
			ConnectionDrawingPolicy = FNodeFactory::CreateConnectionPolicy(Schema, WireLayerId, MaxLayerId, ZoomFactor, MyCullingRect, OutDrawElements, GraphObj);
		}

		TArray<TSharedPtr<SGraphPin>> OverridePins;
		for (const FGraphPinHandle& Handle : PreviewConnectorFromPins)
		{
			TSharedPtr<SGraphPin> Pin = Handle.FindInGraphPanel(*this);
			if (Pin.IsValid() && Pin->GetPinObj())
			{
				OverridePins.Add(Pin);
			}
		}
		ConnectionDrawingPolicy->SetHoveredPins(CurrentHoveredPins, OverridePins, TimeWhenMouseEnteredPin);
		ConnectionDrawingPolicy->SetMarkedPin(MarkedPin);
		ConnectionDrawingPolicy->SetMousePosition(AllottedGeometry.LocalToAbsolute(SavedMousePosForOnPaintEventLocalSpace));

		if (IsRelinkingConnection())
		{
			ConnectionDrawingPolicy->SetRelinkConnections(RelinkConnections);
			ConnectionDrawingPolicy->SetSelectedNodes(GetSelectedGraphNodes());
		}

		// Get the set of pins for all children and synthesize geometry for culled out pins so lines can be drawn to them.
		TMap<TSharedRef<SWidget>, FArrangedWidget> PinGeometries;
		TSet< TSharedRef<SWidget> > VisiblePins;
		for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex)
		{
			TSharedRef<SGraphNode> ChildNode = StaticCastSharedRef<SGraphNode>(Children[ChildIndex]);

			// If this is a culled node, approximate the pin geometry to the corner of the node it is within
			if (IsNodeCulled(ChildNode, AllottedGeometry) || ChildNode->IsHidingPinWidgets())
			{
				TArray< TSharedRef<SWidget> > NodePins;
				ChildNode->GetPins(NodePins);

				const FVector2D NodeLoc = ChildNode->GetPosition();
				const FGeometry SynthesizedNodeGeometry(GraphCoordToPanelCoord(NodeLoc) * AllottedGeometry.Scale, FVector2D(AllottedGeometry.AbsolutePosition), FVector2D::ZeroVector, 1.f);

				for (TArray< TSharedRef<SWidget> >::TConstIterator NodePinIterator(NodePins); NodePinIterator; ++NodePinIterator)
				{
					const SGraphPin& PinWidget = static_cast<const SGraphPin&>((*NodePinIterator).Get());
					if (PinWidget.GetPinObj())
					{
						FVector2D PinLoc = NodeLoc + PinWidget.GetNodeOffset();

						const FGeometry SynthesizedPinGeometry(GraphCoordToPanelCoord(PinLoc) * AllottedGeometry.Scale, FVector2D(AllottedGeometry.AbsolutePosition), FVector2D::ZeroVector, 1.f);
						PinGeometries.Add(*NodePinIterator, FArrangedWidget(*NodePinIterator, SynthesizedPinGeometry));
					}
				}

				// Also add synthesized geometries for culled nodes
				ArrangedChildren.AddWidget( FArrangedWidget(ChildNode, SynthesizedNodeGeometry) );
			}
			else
			{
				ChildNode->GetPins(VisiblePins);
			}
		}

		// Now get the pin geometry for all visible children and append it to the PinGeometries map
		TMap<TSharedRef<SWidget>, FArrangedWidget> VisiblePinGeometries;
		{
			this->FindChildGeometries(AllottedGeometry, VisiblePins, VisiblePinGeometries);
			PinGeometries.Append(VisiblePinGeometries);
		}

		// Draw preview connections (only connected on one end)
		if (PreviewConnectorFromPins.Num() > 0)
		{
			for (const FGraphPinHandle& Handle : PreviewConnectorFromPins)
			{
				TSharedPtr< SGraphPin > CurrentStartPin = Handle.FindInGraphPanel(*this);
				if (!CurrentStartPin.IsValid() || !CurrentStartPin->GetPinObj())
				{
					continue;
				}
				const FArrangedWidget* PinGeometry = PinGeometries.Find( CurrentStartPin.ToSharedRef() );

				if (PinGeometry != nullptr)
				{
					FVector2D StartPoint;
					FVector2D EndPoint;

					if (CurrentStartPin->GetDirection() == EGPD_Input)
					{
						StartPoint = AllottedGeometry.LocalToAbsolute(PreviewConnectorEndpoint);
						EndPoint = FGeometryHelper::VerticalMiddleLeftOf( PinGeometry->Geometry ) - FVector2D(ConnectionDrawingPolicy->ArrowRadius.X, 0);
					}
					else
					{
						StartPoint = FGeometryHelper::VerticalMiddleRightOf( PinGeometry->Geometry );
						EndPoint = AllottedGeometry.LocalToAbsolute(PreviewConnectorEndpoint);
					}

					ConnectionDrawingPolicy->DrawPreviewConnector(PinGeometry->Geometry, StartPoint, EndPoint, CurrentStartPin.Get()->GetPinObj());
				}

				//@TODO: Re-evaluate this incompatible mojo; it's mutating every pin state every frame to accomplish a visual effect
				ConnectionDrawingPolicy->SetIncompatiblePinDrawState(CurrentStartPin, VisiblePins);
			}
		}
		else
		{
			//@TODO: Re-evaluate this incompatible mojo; it's mutating every pin state every frame to accomplish a visual effect
			ConnectionDrawingPolicy->ResetIncompatiblePinDrawState(VisiblePins);
		}

		// Draw all regular connections
		ConnectionDrawingPolicy->Draw(PinGeometries, ArrangedChildren);

		// Pull back data from the drawing policy
		{
			FGraphSplineOverlapResult OverlapData = ConnectionDrawingPolicy->SplineOverlapResult;

			if (OverlapData.IsValid())
			{
				OverlapData.ComputeBestPin();

				// Only allow spline overlaps when there is no node under the cursor (unless it is a comment box)
				const FVector2D PaintAbsoluteSpaceMousePos = AllottedGeometry.LocalToAbsolute(SavedMousePosForOnPaintEventLocalSpace);
				const int32 HoveredNodeIndex = SWidget::FindChildUnderPosition(ArrangedChildren, PaintAbsoluteSpaceMousePos);
				if (HoveredNodeIndex != INDEX_NONE)
				{
					TSharedRef<SGraphNode> HoveredNode = StaticCastSharedRef<SGraphNode>(ArrangedChildren[HoveredNodeIndex].Widget);
					UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(HoveredNode->GetObjectBeingDisplayed());
					if (CommentNode == nullptr)
					{
						// Wasn't a comment node, disallow the spline interaction
						OverlapData = FGraphSplineOverlapResult(OverlapData.GetCloseToSpline());
					}
				}
			}

			// Update the spline hover state
			const_cast<SGraphPanel*>(this)->OnSplineHoverStateChanged(OverlapData);
		}

		delete ConnectionDrawingPolicy;
	}

	// Draw a shadow overlay around the edges of the graph
	++MaxLayerId;
	PaintSurroundSunkenShadow(FAppStyle::GetBrush(TEXT("Graph.Shadow")), AllottedGeometry, MyCullingRect, OutDrawElements, MaxLayerId);

	if (ShowGraphStateOverlay.Get())
	{
		const FSlateBrush* BorderBrush = nullptr;
		if ((GEditor->bIsSimulatingInEditor || GEditor->PlayWorld != nullptr))
		{
			// Draw a surrounding indicator when PIE is active, to make it clear that the graph is read-only, etc...
			BorderBrush = FAppStyle::GetBrush(TEXT("Graph.PlayInEditor"));
		}
		else if (!IsEditable.Get())
		{
			// Draw a different border when we're not simulating but the graph is read-only
			BorderBrush = FAppStyle::GetBrush(TEXT("Graph.ReadOnlyBorder"));
		}

		if (BorderBrush != nullptr)
		{
			// Actually draw the border
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				MaxLayerId,
				AllottedGeometry.ToPaintGeometry(),
				BorderBrush
				);
		}
	}

	// Draw the marquee selection rectangle
	PaintMarquee(AllottedGeometry, MyCullingRect, OutDrawElements, MaxLayerId);

	// Draw the software cursor
	++MaxLayerId;
	PaintSoftwareCursor(AllottedGeometry, MyCullingRect, OutDrawElements, MaxLayerId);

	return MaxLayerId;
}

void SGraphPanel::OnSplineHoverStateChanged(const FGraphSplineOverlapResult& NewSplineHoverState)
{
	TSharedPtr<SGraphPin> OldPin1Widget;
	TSharedPtr<SGraphPin> OldPin2Widget;
	PreviousFrameSplineOverlap.GetPinWidgets(*this, OldPin1Widget, OldPin2Widget);

	PreviousFrameSplineOverlap = NewSplineHoverState;

	TSharedPtr<SGraphPin> NewPin1Widget;
	TSharedPtr<SGraphPin> NewPin2Widget;
	PreviousFrameSplineOverlap.GetPinWidgets(*this, NewPin1Widget, NewPin2Widget);

	PreviousFrameSavedMousePosForSplineOverlap = SavedMousePosForOnPaintEventLocalSpace;

	// Handle exiting hovering on the pins
	if (OldPin1Widget.IsValid() && OldPin1Widget != NewPin1Widget && OldPin1Widget != NewPin2Widget)
	{
		OldPin1Widget->OnMouseLeave(LastPointerEvent);
	}

	if (OldPin2Widget.IsValid() && OldPin2Widget != NewPin1Widget && OldPin2Widget != NewPin2Widget)
	{
		OldPin2Widget->OnMouseLeave(LastPointerEvent);
	}

	// Handle enter hovering on the pins
	bool bChangedHover = false;
	if (NewPin1Widget.IsValid() && NewPin1Widget != OldPin1Widget && NewPin1Widget != OldPin2Widget)
	{
		NewPin1Widget->OnMouseEnter(LastPointerGeometry, LastPointerEvent);
		bChangedHover = true;
	}

	if (NewPin2Widget.IsValid() && NewPin2Widget != OldPin1Widget && NewPin2Widget != OldPin2Widget)
	{
		NewPin2Widget->OnMouseEnter(LastPointerGeometry, LastPointerEvent);
		bChangedHover = true;
	}

	if (bChangedHover)
	{
		// Get the pin/wire glowing quicker, since it's a direct selection (this time was already set to 'now' as part of entering the pin)
		//@TODO: Source this parameter from the graph rendering settings once it is there (see code in ApplyHoverDeemphasis)
		TimeWhenMouseEnteredPin -= 0.75f;
	}
}

bool SGraphPanel::SupportsKeyboardFocus() const
{
	return true;
}

void SGraphPanel::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	SNodePanel::OnArrangeChildren(AllottedGeometry, ArrangedChildren);

	FArrangedChildren MyArrangedChildren(ArrangedChildren.GetFilter());
	for (int32 ChildIndex = 0; ChildIndex < ArrangedChildren.Num(); ++ChildIndex)
	{
		FArrangedWidget& CurWidget = ArrangedChildren[ChildIndex];
		TSharedRef<SGraphNode> ChildNode = StaticCastSharedRef<SGraphNode>(CurWidget.Widget);

		TArray<FOverlayWidgetInfo> OverlayWidgets = ChildNode->GetOverlayWidgets(false, CurWidget.Geometry.Size);

		for (int32 WidgetIndex = 0; WidgetIndex < OverlayWidgets.Num(); ++WidgetIndex)
		{
			FOverlayWidgetInfo& OverlayInfo = OverlayWidgets[WidgetIndex];

			MyArrangedChildren.AddWidget(AllottedGeometry.MakeChild( OverlayInfo.Widget.ToSharedRef(), FVector2D(CurWidget.Geometry.Position) + OverlayInfo.OverlayOffset, OverlayInfo.Widget->GetDesiredSize(), GetZoomAmount() ));
		}
	}

	ArrangedChildren.Append(MyArrangedChildren);
}

TSharedPtr<IToolTip> SGraphPanel::GetToolTip()
{
	if (SGraphPin* BestPinFromHoveredSpline = GetBestPinFromHoveredSpline())
	{
		return BestPinFromHoveredSpline->GetToolTip();
	}

	return SNodePanel::GetToolTip();
}

void SGraphPanel::UpdateSelectedNodesPositions(FVector2D PositionIncrement)
{
	FScopedTransaction Transaction(NSLOCTEXT("GraphEditor", "NudgeNodeAction", "Nudge Node"));
	for (FGraphPanelSelectionSet::TIterator NodeIt(SelectionManager.SelectedNodes); NodeIt; ++NodeIt)
	{
		TSharedRef<SNode>* pWidget = NodeToWidgetLookup.Find(*NodeIt);
		if (pWidget != nullptr)
		{
			SNode& Widget = pWidget->Get();
			SNode::FNodeSet NodeFilter;
			Widget.MoveTo(Widget.GetPosition() + PositionIncrement, NodeFilter);
		}
	}
}

FReply SGraphPanel::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	if( IsEditable.Get() )
	{
		const bool bIsModifierActive = InKeyEvent.IsCommandDown() || InKeyEvent.IsAltDown() || InKeyEvent.IsShiftDown() || InKeyEvent.IsControlDown();
		if (!bIsModifierActive)
		{
			if( InKeyEvent.GetKey() == EKeys::Up || InKeyEvent.GetKey() == EKeys::NumPadEight )
			{
				UpdateSelectedNodesPositions(FVector2D(0.0f,-1.0f * GetSnapGridSize()));
				return FReply::Handled();
			}
			if( InKeyEvent.GetKey() == EKeys::Down || InKeyEvent.GetKey() == EKeys::NumPadTwo )
			{
				UpdateSelectedNodesPositions(FVector2D(0.0f,GetSnapGridSize()));
				return FReply::Handled();
			}
			if( InKeyEvent.GetKey() == EKeys::Right || InKeyEvent.GetKey() == EKeys::NumPadSix )
			{
				UpdateSelectedNodesPositions(FVector2D(GetSnapGridSize(),0.0f));
				return FReply::Handled();
			}
			if( InKeyEvent.GetKey() == EKeys::Left || InKeyEvent.GetKey() == EKeys::NumPadFour )
			{
				UpdateSelectedNodesPositions(FVector2D(-1.0f * GetSnapGridSize(),0.0f));
				return FReply::Handled();
			}
		}
		bool bZoomOutKeyEvent = false;
		bool bZoomInKeyEvent = false;
		// Iterate through all key mappings to generate key event flags
		for (uint32 i = 0; i < static_cast<uint8>(EMultipleKeyBindingIndex::NumChords); ++i)
		{
			EMultipleKeyBindingIndex ChordIndex = static_cast<EMultipleKeyBindingIndex>(i);
			const FInputChord& ZoomOutChord = *FGraphEditorCommands::Get().ZoomOut->GetActiveChord(ChordIndex);
			const FInputChord& ZoomInChord = *FGraphEditorCommands::Get().ZoomIn->GetActiveChord(ChordIndex);
			bZoomOutKeyEvent |= ZoomOutChord.IsValidChord() && InKeyEvent.GetKey() == ZoomOutChord.Key;
			bZoomInKeyEvent |= ZoomInChord.IsValidChord() && InKeyEvent.GetKey() == ZoomInChord.Key;
		}

		if(bZoomOutKeyEvent)
		{
			ChangeZoomLevel(-1, CachedAllottedGeometryScaledSize / 2.f, InKeyEvent.IsControlDown());
			return FReply::Handled();
		}
		if( bZoomInKeyEvent)
		{
			ChangeZoomLevel(+1, CachedAllottedGeometryScaledSize / 2.f, InKeyEvent.IsControlDown());
			return FReply::Handled();
		}
	}

	return SNodePanel::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SGraphPanel::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ((MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) && (MouseEvent.IsAltDown() || MouseEvent.IsControlDown()))
	{
		// Intercept alt-left clicking on the hovered spline for targeted break link
		UEdGraphPin* Pin1;
		UEdGraphPin* Pin2;
		if (MouseEvent.IsAltDown() && PreviousFrameSplineOverlap.GetPins(*this, Pin1, Pin2))
		{
			const UEdGraphSchema* Schema = GraphObj->GetSchema();
			Schema->BreakSinglePinLink(Pin1, Pin2);
		}
		else if (SGraphPin* BestPinFromHoveredSpline = GetBestPinFromHoveredSpline())
		{
			return BestPinFromHoveredSpline->OnPinMouseDown(MyGeometry, MouseEvent);
		}
	}

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		FGraphPinHandle Pin1Handle = PreviousFrameSplineOverlap.GetPin1Handle();
		if (Pin1Handle.IsValid())
		{
			TSharedPtr<class SGraphPin> SourcePin = Pin1Handle.FindInGraphPanel(*this);
			if (SourcePin.IsValid())
			{
				const UEdGraphSchema* Schema = GraphObj->GetSchema();
				if (Schema->IsConnectionRelinkingAllowed(SourcePin->GetPinObj()))
				{
					return SourcePin->OnPinMouseDown(MyGeometry, MouseEvent);
				}
			}
		}
	}

	return SNodePanel::OnMouseButtonDown(MyGeometry, MouseEvent);
}

TArray<UEdGraphNode*> SGraphPanel::GetSelectedGraphNodes() const
{
	TArray<UEdGraphNode*> SelectedGraphNodes;
	SelectedGraphNodes.Reserve(SelectionManager.SelectedNodes.Num());

	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectionManager.SelectedNodes); NodeIt; ++NodeIt)
	{
		const TSharedRef<SNode>* SelectedNode = NodeToWidgetLookup.Find(*NodeIt);
		if (SelectedNode)
		{
			UEdGraphNode* SelectedGraphNode = Cast<UEdGraphNode>(SelectedNode->Get().GetObjectBeingDisplayed());
			if (SelectedGraphNode)
			{
				SelectedGraphNodes.Add(SelectedGraphNode);
			}
		}
	}

	return SelectedGraphNodes;
}

FReply SGraphPanel::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!NodeUnderMousePtr.IsValid() && !Marquee.IsValid() && (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) && (MouseEvent.IsShiftDown()))
	{
		if (SGraphPin* BestPinFromHoveredSpline = GetBestPinFromHoveredSpline())
		{
			return BestPinFromHoveredSpline->OnMouseButtonUp(MyGeometry, MouseEvent);
		}
	}

	return SNodePanel::OnMouseButtonUp(MyGeometry, MouseEvent);
}

FReply SGraphPanel::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	UEdGraphPin* Pin1;
	UEdGraphPin* Pin2;
	if (PreviousFrameSplineOverlap.GetPins(*this, /*out*/ Pin1, /*out*/ Pin2))
	{
		// Give the schema a chance to do something interesting with a double click on a proper spline (both ends are attached to a pin, i.e., not a preview/drag one)
		const FVector2D DoubleClickPositionInGraphSpace = PanelCoordToGraphCoord(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));

		const UEdGraphSchema* Schema = GraphObj->GetSchema();
		Schema->OnPinConnectionDoubleCicked(Pin1, Pin2, DoubleClickPositionInGraphSpace);
	}
	else if (!PreviousFrameSplineOverlap.GetCloseToSpline())
	{
		OnDoubleClicked.ExecuteIfBound();
	}

	return SNodePanel::OnMouseButtonDoubleClick(MyGeometry, MouseEvent);
}

class SGraphPin* SGraphPanel::GetBestPinFromHoveredSpline() const
{
	TSharedPtr<SGraphPin> BestPinWidget = PreviousFrameSplineOverlap.GetBestPinWidget(*this);
	return BestPinWidget.Get();
}

void SGraphPanel::GetAllPins(TSet< TSharedRef<SWidget> >& AllPins)
{
	// Get the set of pins for all children
	for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex)
	{
		TSharedRef<SGraphNode> ChildNode = StaticCastSharedRef<SGraphNode>(Children[ChildIndex]);
		ChildNode->GetPins(AllPins);
	}
}

void SGraphPanel::AddPinToHoverSet(UEdGraphPin* HoveredPin)
{
	CurrentHoveredPins.Add(HoveredPin);
	TimeWhenMouseEnteredPin = FSlateApplication::Get().GetCurrentTime();

	// About covers the fade in time when highlighting pins or splines.
	TimeLeftToInvalidatePerTick += 1.5f;

	// This handle should always be for this function
	if (!ActiveTimerHandleInvalidatePerTick.IsValid())
	{
		ActiveTimerHandleInvalidatePerTick = RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SGraphPanel::InvalidatePerTick));
	}
}

void SGraphPanel::RemovePinFromHoverSet(UEdGraphPin* UnhoveredPin)
{
	CurrentHoveredPins.Remove(UnhoveredPin);
	TimeWhenMouseLeftPin = FSlateApplication::Get().GetCurrentTime();
}

void SGraphPanel::ArrangeChildrenForContextMenuSummon(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	// First pass nodes
	for (int32 ChildIndex = 0; ChildIndex < VisibleChildren.Num(); ++ChildIndex)
	{
		const TSharedRef<SNode>& SomeChild = VisibleChildren[ChildIndex];
		if (!SomeChild->RequiresSecondPassLayout())
		{
			ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(SomeChild, SomeChild->GetPosition() - ViewOffset, SomeChild->GetDesiredSizeForMarquee(), GetZoomAmount()));
		}
	}

	// Second pass nodes
	for (int32 ChildIndex = 0; ChildIndex < VisibleChildren.Num(); ++ChildIndex)
	{
		const TSharedRef<SNode>& SomeChild = VisibleChildren[ChildIndex];
		if (SomeChild->RequiresSecondPassLayout())
		{
			SomeChild->PerformSecondPassLayout(NodeToWidgetLookup);
			ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(SomeChild, SomeChild->GetPosition() - ViewOffset, SomeChild->GetDesiredSizeForMarquee(), GetZoomAmount()));
		}
	}
}

TSharedPtr<SWidget> SGraphPanel::OnSummonContextMenu(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TSharedPtr<SGraphNode> NodeUnderMouse = GetGraphNodeUnderMouse(MyGeometry, MouseEvent);
	UEdGraphPin* PinUnderCursor = GetPinUnderMouse(MyGeometry, MouseEvent, NodeUnderMouse);
	UEdGraphNode* EdNodeUnderMouse = NodeUnderMouse.IsValid() ? NodeUnderMouse->GetNodeObj() : nullptr;
	TArray<UEdGraphPin*> NoSourcePins;

	const FVector2D NodeAddPosition = PanelCoordToGraphCoord(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
	return SummonContextMenu(MouseEvent.GetScreenSpacePosition(), NodeAddPosition, EdNodeUnderMouse, PinUnderCursor, NoSourcePins);
}


bool SGraphPanel::OnHandleLeftMouseRelease(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TSharedPtr<SGraphPin> PreviewConnectionPin = PreviewConnectorFromPins.Num() > 0 ? PreviewConnectorFromPins[0].FindInGraphPanel(*this) : nullptr;
	if (PreviewConnectionPin.IsValid() && IsEditable.Get())
	{
		TSet< TSharedRef<SWidget> > AllConnectors;
		for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex)
		{
			//@FINDME:
			TSharedRef<SGraphNode> ChildNode = StaticCastSharedRef<SGraphNode>(Children[ChildIndex]);
			ChildNode->GetPins(AllConnectors);
		}

		TMap<TSharedRef<SWidget>, FArrangedWidget> PinGeometries;
		this->FindChildGeometries(MyGeometry, AllConnectors, PinGeometries);

		bool bHandledDrop = false;
		TSet<UEdGraphNode*> NodeList;
		for ( TMap<TSharedRef<SWidget>, FArrangedWidget>::TIterator SomePinIt(PinGeometries); !bHandledDrop && SomePinIt; ++SomePinIt )
		{
			FArrangedWidget& PinWidgetGeometry = SomePinIt.Value();
			if( PinWidgetGeometry.Geometry.IsUnderLocation( MouseEvent.GetScreenSpacePosition() ) )
			{
				SGraphPin& TargetPin = static_cast<SGraphPin&>( PinWidgetGeometry.Widget.Get() );
				
				if (PreviewConnectionPin->TryHandlePinConnection(TargetPin))
				{
					// We have to do a second check on PinObjs here since TryHandlePinConnection, may invalidate them.
					UEdGraphPin* PreviewConnectionPinObj = PreviewConnectionPin->GetPinObj();
					UEdGraphPin* TargetPinObj = TargetPin.GetPinObj();
					if (TargetPinObj && PreviewConnectionPinObj)
					{
						NodeList.Add(TargetPinObj->GetOwningNode());
						NodeList.Add(PreviewConnectionPinObj->GetOwningNode());
					}
				}
				bHandledDrop = true;
			}
		}

		// No longer make a connection for a pin; we just connected or failed to connect.
		OnStopMakingConnection(/*bForceStop=*/ true);

		return true;
	}
	else
	{
		return false;
	}
}

FReply SGraphPanel::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	LastPointerEvent = MouseEvent;
	LastPointerGeometry = MyGeometry;

	// Save the mouse position to use in OnPaint for spline hit detection
	SavedMousePosForOnPaintEventLocalSpace = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	// Invalidate the spline results if we moved very far
	const FVector2D MouseDelta = SavedMousePosForOnPaintEventLocalSpace - PreviousFrameSavedMousePosForSplineOverlap;
	const float MouseDeltaLengthSquared = MouseDelta.SizeSquared();
	const bool bCursorInDeadZone = MouseDeltaLengthSquared <= FMath::Square(FSlateApplication::Get().GetDragTriggerDistance());

	if (!bCursorInDeadZone)
	{
		//@TODO: Should we do this or just rely on the next OnPaint?
		// Our frame-latent approximation is going to be totally junk if the mouse is moving quickly
		OnSplineHoverStateChanged(FGraphSplineOverlapResult());
	}

	return SNodePanel::OnMouseMove(MyGeometry, MouseEvent);
}

void SGraphPanel::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FGraphEditorDragDropAction> DragConnectionOp = DragDropEvent.GetOperationAs<FGraphEditorDragDropAction>();
	if (DragConnectionOp.IsValid())
	{
		DragConnectionOp->SetHoveredGraph( SharedThis(this) );
	}
}

void SGraphPanel::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FGraphEditorDragDropAction> Operation = DragDropEvent.GetOperationAs<FGraphEditorDragDropAction>();
	if( Operation.IsValid() )
	{
		Operation->SetHoveredGraph(TSharedPtr<SGraphPanel>(nullptr));
	}
	else
	{
		TSharedPtr<FDecoratedDragDropOp> AssetOp = DragDropEvent.GetOperationAs<FDecoratedDragDropOp>();
		if( AssetOp.IsValid()  )
		{
			AssetOp->ResetToDefaultToolTip();
		}
	}
}

FReply SGraphPanel::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	if (!Operation.IsValid())
	{
		return FReply::Unhandled();
	}

	// Handle Read only graphs
	if (!IsEditable.Get())
	{
		TSharedPtr<FGraphEditorDragDropAction> GraphDragDropOp = DragDropEvent.GetOperationAs<FGraphEditorDragDropAction>();

		if (GraphDragDropOp.IsValid())
		{
			GraphDragDropOp->SetDropTargetValid(false);
		}
		else
		{
			TSharedPtr<FDecoratedDragDropOp> AssetOp = DragDropEvent.GetOperationAs<FDecoratedDragDropOp>();
			if (AssetOp.IsValid())
			{
				FText Tooltip = AssetOp->GetHoverText();
				if (Tooltip.IsEmpty())
				{
					Tooltip = NSLOCTEXT( "GraphPanel", "DragDropOperation", "Graph is Read-Only" );
				}
				AssetOp->SetToolTip(Tooltip, FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error")));
			}
		}
		return FReply::Handled();
	}

	if (Operation->IsOfType<FGraphEditorDragDropAction>())
	{
		PreviewConnectorEndpoint = MyGeometry.AbsoluteToLocal( DragDropEvent.GetScreenSpacePosition() );
		return FReply::Handled();
	}
	else if (Operation->IsOfType<FExternalDragOperation>())
	{
		return AssetUtil::CanHandleAssetDrag(DragDropEvent);
	}
	else if (Operation->IsOfType<FAssetDragDropOp>())
	{
		if ((GraphObj != nullptr) && (GraphObj->GetSchema() != nullptr))
		{
			TSharedPtr<FAssetDragDropOp> AssetOp = StaticCastSharedPtr<FAssetDragDropOp>(Operation);
			bool bOkIcon = false;
			FText TooltipText;
			if (AssetOp->HasAssets())
			{
				const TArray<FAssetData>& HoveredAssetData = AssetOp->GetAssets();
				FText AssetReferenceFilterFailureReason;
				if (PassesAssetReferenceFilter(HoveredAssetData, &AssetReferenceFilterFailureReason))
				{
					FString TooltipTextString;
					GraphObj->GetSchema()->GetAssetsGraphHoverMessage(HoveredAssetData, GraphObj, /*out*/ TooltipTextString, /*out*/ bOkIcon);
					TooltipText = FText::FromString(TooltipTextString);
				}
				else
				{
					TooltipText = AssetReferenceFilterFailureReason;
					bOkIcon = false;
				}
			}
			const FSlateBrush* TooltipIcon = bOkIcon ? FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK")) : FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
			AssetOp->SetToolTip(TooltipText, TooltipIcon);
		}
		return FReply::Handled();
	} 
	else
	{
		return FReply::Unhandled();
	}
}

FReply SGraphPanel::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	const FVector2D NodeAddPosition = PanelCoordToGraphCoord( MyGeometry.AbsoluteToLocal( DragDropEvent.GetScreenSpacePosition() ) );

	FSlateApplication::Get().SetKeyboardFocus(AsShared(), EFocusCause::SetDirectly);

	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	if (!Operation.IsValid() || !IsEditable.Get())
	{
		return FReply::Unhandled();
	}

	if (Operation->IsOfType<FGraphEditorDragDropAction>())
	{
		check(GraphObj);
		TSharedPtr<FGraphEditorDragDropAction> DragConn = StaticCastSharedPtr<FGraphEditorDragDropAction>(Operation);
		if (DragConn.IsValid() && DragConn->IsSupportedBySchema(GraphObj->GetSchema()))
		{
			return DragConn->DroppedOnPanel(SharedThis(this), DragDropEvent.GetScreenSpacePosition(), NodeAddPosition, *GraphObj);
		}

		return FReply::Unhandled();
	}
	else if (Operation->IsOfType<FActorDragDropGraphEdOp>())
	{
		TSharedPtr<FActorDragDropGraphEdOp> ActorOp = StaticCastSharedPtr<FActorDragDropGraphEdOp>(Operation);
		OnDropActor.ExecuteIfBound(ActorOp->Actors, GraphObj, NodeAddPosition);
		return FReply::Handled();
	}

	else if (Operation->IsOfType<FLevelDragDropOp>())
	{
		TSharedPtr<FLevelDragDropOp> LevelOp = StaticCastSharedPtr<FLevelDragDropOp>(Operation);
		OnDropStreamingLevel.ExecuteIfBound(LevelOp->StreamingLevelsToDrop, GraphObj, NodeAddPosition);
		return FReply::Handled();
	}

	else if (Operation->IsOfType<FGraphNodeDragDropOp>())
	{
		TSharedPtr<FGraphNodeDragDropOp> NodeDropOp = StaticCastSharedPtr<FGraphNodeDragDropOp>(Operation);
		NodeDropOp->OnPerformDropToGraph.ExecuteIfBound(NodeDropOp, GraphObj, NodeAddPosition, DragDropEvent.GetScreenSpacePosition());
		return FReply::Handled();
	}
	else
	{
		if ((GraphObj != nullptr) && (GraphObj->GetSchema() != nullptr))
		{
			TArray< FAssetData > DroppedAssetData = AssetUtil::ExtractAssetDataFromDrag( DragDropEvent );

			if ( DroppedAssetData.Num() > 0 )
			{
				if (PassesAssetReferenceFilter(DroppedAssetData))
				{
					GraphObj->GetSchema()->DroppedAssetsOnGraph( DroppedAssetData, NodeAddPosition, GraphObj );
				}
				return FReply::Handled();
			}
		}

		return FReply::Unhandled();
	}
}

bool SGraphPanel::PassesAssetReferenceFilter(const TArray<FAssetData>& ReferencedAssets, FText* OutFailureReason) const
{
	if (GEditor)
	{
		FAssetReferenceFilterContext AssetReferenceFilterContext;
		UObject* GraphOuter = GraphObj ? GraphObj->GetOuter() : nullptr;
		if (GraphOuter)
		{
			AssetReferenceFilterContext.ReferencingAssets.Add(FAssetData(GraphOuter));
		}
		TSharedPtr<IAssetReferenceFilter> AssetReferenceFilter = GEditor->MakeAssetReferenceFilter(AssetReferenceFilterContext);
		if (AssetReferenceFilter.IsValid())
		{
			for (const FAssetData& Asset : ReferencedAssets)
			{
				if (!AssetReferenceFilter->PassesFilter(Asset, OutFailureReason))
				{
					return false;
				}
			}
		}
	}

	return true;
}

TSharedPtr<SGraphNode> SGraphPanel::GetGraphNodeUnderMouse(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TSharedPtr<SGraphNode> GraphNode;
	FArrangedChildren ArrangedNodes(EVisibility::Visible);
	this->ArrangeChildrenForContextMenuSummon(MyGeometry, ArrangedNodes);
	const int32 HoveredNodeIndex = SWidget::FindChildUnderMouse(ArrangedNodes, MouseEvent);
	if (HoveredNodeIndex != INDEX_NONE)
	{
		const FArrangedWidget& HoveredNode = ArrangedNodes[HoveredNodeIndex];
		GraphNode = StaticCastSharedRef<SGraphNode>(HoveredNode.Widget);
		TSharedPtr<SGraphNode> GraphSubNode = GraphNode->GetNodeUnderMouse(HoveredNode.Geometry, MouseEvent);
		GraphNode = GraphSubNode.IsValid() ? GraphSubNode.ToSharedRef() : GraphNode;

		// Selection should switch to this code if it isn't already selected.
		// When multiple nodes are selected, we do nothing, provided that the
		// node for which the context menu is being created is in the selection set.
		if (!SelectionManager.IsNodeSelected(GraphNode->GetObjectBeingDisplayed()))
		{
			SelectionManager.SelectSingleNode(GraphNode->GetObjectBeingDisplayed());
		}
	}

	return GraphNode;
}

UEdGraphPin* SGraphPanel::GetPinUnderMouse(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, TSharedPtr<SGraphNode> GraphNode) const
{
	UEdGraphPin* PinUnderCursor = nullptr;
	if (GraphNode.IsValid())
	{
		const TSharedPtr<SGraphPin> HoveredPin = GraphNode->GetHoveredPin(GraphNode->GetCachedGeometry(), MouseEvent);
		if (HoveredPin.IsValid())
		{
			PinUnderCursor = HoveredPin->GetPinObj();
		}
	}

	return PinUnderCursor;
}

void SGraphPanel::OnBeginMakingConnection(UEdGraphPin* InOriginatingPin)
{
	OnBeginMakingConnection(FGraphPinHandle(InOriginatingPin));
}

void SGraphPanel::OnBeginMakingConnection(FGraphPinHandle PinHandle)
{
	if (PinHandle.IsValid())
	{
		DismissContextMenu();

		PreviewConnectorFromPins.Add(PinHandle);
	}
}

void SGraphPanel::OnStopMakingConnection(bool bForceStop)
{
	if (bForceStop || !bPreservePinPreviewConnection)
	{
		PreviewConnectorFromPins.Reset();
		bPreservePinPreviewConnection = false;
	}
}

void SGraphPanel::OnBeginRelinkConnection(const FGraphPinHandle& InSourcePinHandle, const FGraphPinHandle& InTargetPinHandle)
{
	RelinkConnections.Add({ InSourcePinHandle.GetPinObj(*this), InTargetPinHandle.GetPinObj(*this) });
	OnBeginMakingConnection(InSourcePinHandle);
}

void SGraphPanel::OnEndRelinkConnection(bool bForceStop)
{
	OnStopMakingConnection(bForceStop);
	RelinkConnections.Empty();
}

bool SGraphPanel::IsRelinkingConnection() const
{
	return (RelinkConnections.IsEmpty() == false);
}

void SGraphPanel::PreservePinPreviewUntilForced()
{
	bPreservePinPreviewConnection = true;
}

/** Add a slot to the CanvasPanel dynamically */
void SGraphPanel::AddGraphNode( const TSharedRef<SNodePanel::SNode>& NodeToAdd )
{
	TSharedRef<SGraphNode> GraphNode = StaticCastSharedRef<SGraphNode>(NodeToAdd);
	GraphNode->SetOwner( SharedThis(this) );

	const UEdGraphNode* Node = GraphNode->GetNodeObj();
	if (Node)
	{
		NodeGuidMap.Add(Node->NodeGuid, GraphNode);
	}

	SNodePanel::AddGraphNode(NodeToAdd);
}

void SGraphPanel::RemoveAllNodes()
{
	NodeGuidMap.Empty();
	CurrentHoveredPins.Empty();
	
	for (int32 Iter = 0; Iter != Children.Num(); ++Iter)
	{
		GetChild(Iter)->InvalidateGraphData();
	}

	SNodePanel::RemoveAllNodes();
}

TSharedPtr<SWidget> SGraphPanel::SummonContextMenu(const FVector2D& WhereToSummon, const FVector2D& WhereToAddNode, UEdGraphNode* ForNode, UEdGraphPin* ForPin, const TArray<UEdGraphPin*>& DragFromPins)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SGraphPanel::SummonContextMenu);

	if (OnGetContextMenuFor.IsBound())
	{
		FGraphContextMenuArguments SpawnInfo;
		SpawnInfo.NodeAddPosition = WhereToAddNode;
		SpawnInfo.GraphNode = ForNode;
		SpawnInfo.GraphPin = ForPin;
		SpawnInfo.DragFromPins = DragFromPins;

		FActionMenuContent FocusedContent = OnGetContextMenuFor.Execute(SpawnInfo);

		TSharedRef<SWidget> MenuContent = FocusedContent.Content;
		
		TSharedPtr<IMenu> Menu = FSlateApplication::Get().PushMenu(
			AsShared(),
			FWidgetPath(),
			MenuContent,
			WhereToSummon,
			FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu )
			);

		if (Menu.IsValid() && Menu->GetOwnedWindow().IsValid() && FocusedContent.WidgetToFocus.IsValid())
		{
			Menu->GetOwnedWindow()->SetWidgetToFocusOnActivate(FocusedContent.WidgetToFocus);
		}

		if (Menu.IsValid())
		{
			Menu->GetOnMenuDismissed().AddLambda([DelegateList=FocusedContent.OnMenuDismissed](TSharedRef<IMenu>) { DelegateList.Broadcast(); });
		}
		else
		{
			FocusedContent.OnMenuDismissed.Broadcast();
		}

		ContextMenu = Menu;

		return FocusedContent.WidgetToFocus;
	}

	return TSharedPtr<SWidget>();
}

void SGraphPanel::SummonCreateNodeMenuFromUICommand(uint32 NumNodesAdded)
{
	FVector2D WhereToSummonMenu = LastPointerEvent.GetScreenSpacePosition();
	const float AdditionalOffset = 1.0f + (NumNodesAdded * GetDefault<UGraphEditorSettings>()->PaddingAutoCollateIncrement);
	FVector2D WhereToAddNode = PastePosition + AdditionalOffset;

	TSharedPtr<SGraphNode> NodeUnderMouse = GetGraphNodeUnderMouse(LastPointerGeometry, LastPointerEvent);

	// Do not open the context menu on top of non-empty graph area
	if (NodeUnderMouse.IsValid())
	{
		return;
	}
	
	TArray<UEdGraphPin*> DragFromPins;
	TSharedPtr<SWidget> CreateNodeMenuWidget = SummonContextMenu(WhereToSummonMenu, WhereToAddNode, nullptr, nullptr, DragFromPins);

	if (CreateNodeMenuWidget.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(CreateNodeMenuWidget);
		return;
	}
}

void SGraphPanel::DismissContextMenu()
{
	if (TSharedPtr<IMenu> ContextMenuPinned = ContextMenu.Pin())
	{
		ContextMenuPinned->Dismiss();
	}

	ContextMenu.Reset();
}

void SGraphPanel::AttachGraphEvents(TSharedPtr<SGraphNode> CreatedSubNode)
{
	check(CreatedSubNode.IsValid());
	CreatedSubNode->SetIsEditable(IsEditable);
	CreatedSubNode->SetDoubleClickEvent(OnNodeDoubleClicked);
	CreatedSubNode->SetVerifyTextCommitEvent(OnVerifyTextCommit);
	CreatedSubNode->SetTextCommittedEvent(OnTextCommitted);
}

bool SGraphPanel::GetBoundsForNode(const UObject* InNode, FVector2D& MinCorner, FVector2D& MaxCorner, float Padding) const
{
	return SNodePanel::GetBoundsForNode(InNode, MinCorner, MaxCorner, Padding);
}

class FConnectionAligner
{
public:
	void DefineConnection(UEdGraphNode* SourceNode, const TSharedPtr<SGraphPin>& SourcePin, UEdGraphNode* DestinationNode, const TSharedPtr<SGraphPin>& DestinationPin)
	{
		auto& Dependencies = Connections.FindOrAdd(SourceNode);
		if (SourcePin->GetPinObj()->Direction == EEdGraphPinDirection::EGPD_Output)
		{
			Dependencies.Outputs.FindOrAdd(DestinationNode).Add(FPinPair{ SourcePin, DestinationPin });
		}
		else
		{
			Dependencies.Inputs.FindOrAdd(DestinationNode).Add(FPinPair{ SourcePin, DestinationPin });
		}
	}

	/** Align all the connections */
	void Process()
	{
		struct FRankedNode
		{
			UEdGraphNode* Node;
			uint32 Rank;
		};

		TArray<FRankedNode> RankedNodes;
		RankedNodes.Reserve(Connections.Num());

		TMap<UEdGraphNode*, uint32> LongestChainCache;
		LongestChainCache.Reserve(Connections.Num());

		for (auto& Pair : Connections)
		{
			RankedNodes.Add(FRankedNode{ Pair.Key, CalculateNodeRank(Pair.Key, LongestChainCache) });
		}

		// Sort the nodes based on dependencies - highest is processed first
		RankedNodes.Sort([](const FRankedNode& A, const FRankedNode& B){
			return A.Rank > B.Rank;
		});

		TSet<UEdGraphNode*> VistedNodes;
		for (FRankedNode& RankedNode : RankedNodes)
		{
			StraightenConnectionsForNode(RankedNode.Node, VistedNodes, EEdGraphPinDirection::EGPD_Output);
			if (VistedNodes.Num() == RankedNodes.Num())
			{
				return;
			}

			StraightenConnectionsForNode(RankedNode.Node, VistedNodes, EEdGraphPinDirection::EGPD_Input);
			if (VistedNodes.Num() == RankedNodes.Num())
			{
				return;
			}
		}
	}

private:

	void StraightenConnectionsForNode(UEdGraphNode* Node, TSet<UEdGraphNode*>& VisitedNodes, EEdGraphPinDirection Direction)
	{
		FDependencyInfo* Info = Connections.Find(Node);
		if (!Info)
		{
			return;
		}

		for (auto& NodeToPins : Info->GetDirection(Direction))
		{
			if (NodeToPins.Value.Num() == 0 || VisitedNodes.Contains(NodeToPins.Key))
			{
				continue;
			}

			// Align the averages of all the pins
			float AlignmentDelta = 0.f;
			for (const FPinPair& Pins : NodeToPins.Value)
			{
				AlignmentDelta += (Node->NodePosY + Pins.SrcPin->GetNodeOffset().Y) - (NodeToPins.Key->NodePosY + Pins.DstPin->GetNodeOffset().Y);
			}

			UEdGraph* GraphObj = NodeToPins.Key->GetGraph();

			check(GraphObj);

			const UEdGraphSchema* Schema = GraphObj->GetSchema();

			float NewNodePosY = NodeToPins.Key->NodePosY + (AlignmentDelta / NodeToPins.Value.Num());
			Schema->SetNodePosition(NodeToPins.Key, FVector2D(NodeToPins.Key->NodePosX, NewNodePosY));
	
			VisitedNodes.Add(Node);
			VisitedNodes.Add(NodeToPins.Key);
			
			StraightenConnectionsForNode(NodeToPins.Key, VisitedNodes, Direction);
		}
	}

	/** Find the longest chain of single-connection nodes connected to the specified node */
	uint32 FindLongestUniqueChain(UEdGraphNode* Node, TMap<UEdGraphNode*, uint32>& LongestChainCache, EEdGraphPinDirection Direction)
	{
		if (uint32* Length = LongestChainCache.Find(Node))
		{
			// Already set, or circular dependency - ignore
			return *Length;
		}

		// Prevent reentrancy
		LongestChainCache.Add(Node, 0);

		uint32 ThisLength = 0;
		
		if (FDependencyInfo* Dependencies = Connections.Find(Node))
		{
			auto& ConnectedNodes = Dependencies->GetDirection(Direction);

			// We only follow unique (1-1) connections
			if (ConnectedNodes.Num() == 1)
			{
				for (auto& NodeToPins : ConnectedNodes)
				{
					ThisLength = FindLongestUniqueChain(NodeToPins.Key, LongestChainCache, Direction) + 1;
				}
			}
		}

		LongestChainCache[Node] = ThisLength;
		return ThisLength;
	};

	/** Calculate the depth of dependencies for the specified node */
	uint32 CalculateNodeRank(UEdGraphNode* Node, TMap<UEdGraphNode*, uint32>& LongestChainCache)
	{
		uint32 Rank = 0;
		if (FDependencyInfo* PinMap = Connections.Find(Node))
		{
			for (auto& NodeToPins : PinMap->Outputs)
			{
				Rank += FindLongestUniqueChain(NodeToPins.Key, LongestChainCache, EEdGraphPinDirection::EGPD_Output) + 1;
			}
			for (auto& NodeToPins : PinMap->Inputs)
			{
				Rank += FindLongestUniqueChain(NodeToPins.Key, LongestChainCache, EEdGraphPinDirection::EGPD_Input) + 1;
			}
		}
		return Rank;
	}

private:

	/** A pair of pins */
	struct FPinPair
	{
		TSharedPtr<SGraphPin> SrcPin, DstPin;
	};

	/** Map of nodes and pins that are connected to the owning pin */
	struct FDependencyInfo
	{
		TMap<UEdGraphNode*, TArray<FPinPair>> Outputs;
		TMap<UEdGraphNode*, TArray<FPinPair>> Inputs;
		uint32 Rank;

		TMap<UEdGraphNode*, TArray<FPinPair>>& GetDirection(EEdGraphPinDirection Direction)
		{
			return Direction == EEdGraphPinDirection::EGPD_Output ? Outputs : Inputs;
		}
	};
	typedef TMap<UEdGraphNode*, FDependencyInfo> FConnections;

	FConnections Connections;
};

void SGraphPanel::StraightenConnections()
{
	FConnectionAligner Aligner;
	for (auto* It : SelectionManager.SelectedNodes)
	{
		UEdGraphNode* SourceNode = Cast<UEdGraphNode>(It);
		if (!SourceNode)
		{
			continue;
		}

		TSharedRef<SNode>* ThisNodePtr = NodeToWidgetLookup.Find(SourceNode);
		if (!ThisNodePtr)
		{
			continue;
		}

		for (UEdGraphPin* SourcePin : SourceNode->Pins)
		{
			for (UEdGraphPin* LinkedTo : SourcePin->LinkedTo)
			{
				UEdGraphNode* DestNode = LinkedTo ? LinkedTo->GetOwningNode() : nullptr;
				if (DestNode && SelectionManager.SelectedNodes.Contains(DestNode))
				{
					TSharedRef<SNode>* DestGraphNodePtr = NodeToWidgetLookup.Find(DestNode);
					if (!DestGraphNodePtr)
					{
						continue;
					}

					TSharedPtr<SGraphPin> PinWidget = StaticCastSharedRef<SGraphNode>(*ThisNodePtr)->FindWidgetForPin(SourcePin);
					TSharedPtr<SGraphPin> LinkedPinWidget = StaticCastSharedRef<SGraphNode>(*DestGraphNodePtr)->FindWidgetForPin(LinkedTo);
					
					if (PinWidget.IsValid() && LinkedPinWidget.IsValid())
					{
						Aligner.DefineConnection(SourceNode, PinWidget, DestNode, LinkedPinWidget);
					}
				}
			}
		}
	}

	Aligner.Process();
}

void SGraphPanel::StraightenConnections(UEdGraphPin* SourcePin, UEdGraphPin* PinToAlign)
{
	UEdGraphNode* OwningNode = SourcePin->GetOwningNode();

	TSharedRef<SNode>* OwningNodeWidgetPtr = NodeToWidgetLookup.Find(OwningNode);
	if (!OwningNodeWidgetPtr)
	{
		return;
	}

	TSharedRef<SGraphNode> SourceGraphNode = StaticCastSharedRef<SGraphNode>(*OwningNodeWidgetPtr);

	FConnectionAligner Aligner;

	auto AddConnectedPin = [&](UEdGraphPin* ConnectedPin){
		UEdGraphNode* ConnectedNode = ConnectedPin ? ConnectedPin->GetOwningNode() : nullptr;
		if (!ConnectedNode)
		{
			return;
		}

		TSharedRef<SNode>* DestGraphNodePtr = NodeToWidgetLookup.Find(ConnectedNode);
		if (!DestGraphNodePtr)
		{
			return;
		}

		TSharedPtr<SGraphPin> PinWidget = SourceGraphNode->FindWidgetForPin(SourcePin);
		TSharedPtr<SGraphPin> LinkedPinWidget = StaticCastSharedRef<SGraphNode>(*DestGraphNodePtr)->FindWidgetForPin(ConnectedPin);
			
		if (PinWidget.IsValid() && LinkedPinWidget.IsValid())
		{
			Aligner.DefineConnection(OwningNode, PinWidget, ConnectedNode, LinkedPinWidget);
		}
	};

	if (PinToAlign)
	{
		// If we're only aligning a specific pin, do that
		AddConnectedPin(PinToAlign);
	}
	// Else add all the connected pins
	else for (UEdGraphPin* ConnectedPin : SourcePin->LinkedTo)
	{
		AddConnectedPin(ConnectedPin);
	}
	
	Aligner.Process();
}

void SGraphPanel::RefreshNode(UEdGraphNode& Node)
{
	TSharedPtr<SGraphNode> GraphNode = GetNodeWidgetFromGuid(Node.NodeGuid);
	if (GraphNode.IsValid())
	{
		GraphNode->UpdateGraphNode();
	}
}

const TSharedRef<SGraphNode> SGraphPanel::GetChild(int32 ChildIndex)
{
	return StaticCastSharedRef<SGraphNode>(Children[ChildIndex]);
}

void SGraphPanel::AddNode(UEdGraphNode* Node, AddNodeBehavior Behavior)
{
	TSharedPtr<SGraphNode> NewNode;
	if (NodeFactory.IsValid())
	{
		NewNode = NodeFactory->CreateNodeWidget(Node);
	}
	else
	{
		NewNode = FNodeFactory::CreateNodeWidget(Node);
	}

	check(NewNode.IsValid());

	const bool bWasUserAdded = 
		Behavior == WasUserAdded ? true : 
		Behavior == NotUserAdded ? false :
					(UserAddedNodes.Find(Node) != nullptr);

	NewNode->SetIsEditable(IsEditable);
	NewNode->SetDoubleClickEvent(OnNodeDoubleClicked);
	NewNode->SetVerifyTextCommitEvent(OnVerifyTextCommit);
	NewNode->SetTextCommittedEvent(OnTextCommitted);
	NewNode->SetDisallowedPinConnectionEvent(OnDisallowedPinConnection);

	this->AddGraphNode
	(
		NewNode.ToSharedRef()
	);

	if (bWasUserAdded)
	{
		// Add the node to visible children, this allows focus to occur on sub-widgets for naming purposes.
		VisibleChildren.Add(NewNode.ToSharedRef());

		NewNode->PlaySpawnEffect();
		NewNode->RequestRenameOnSpawn();
	}
}

void SGraphPanel::RemoveNode(const UEdGraphNode* Node)
{
	for (int32 Iter = 0; Iter != Children.Num(); ++Iter)
	{
		TSharedRef<SGraphNode> Child = GetChild(Iter);
		if (Child->GetNodeObj() == Node)
		{
			Child->InvalidateGraphData();
			Children.RemoveAt(Iter);
			break;
		}
	}
	for (int32 Iter = 0; Iter != VisibleChildren.Num(); ++Iter)
	{
		TSharedRef<SGraphNode> Child = StaticCastSharedRef<SGraphNode>(VisibleChildren[Iter]);
		if (Child->GetNodeObj() == Node)
		{
			VisibleChildren.RemoveAt(Iter);
			break;
		}
	}
}

TSharedPtr<SGraphNode> SGraphPanel::GetNodeWidgetFromGuid(FGuid Guid) const
{
	return NodeGuidMap.FindRef(Guid).Pin();
}

void SGraphPanel::Update()
{
	static bool bIsUpdating = false;
	if (bIsUpdating)
	{
		return;
	}
	TGuardValue<bool> ReentrancyGuard(bIsUpdating, true);

	// Add widgets for all the nodes that don't have one.
	if (GraphObj != nullptr)
	{
		// Scan for all missing nodes
		for (int32 NodeIndex = 0; NodeIndex < GraphObj->Nodes.Num(); ++NodeIndex)
		{
			UEdGraphNode* Node = GraphObj->Nodes[NodeIndex];
			if (Node)
			{
				// Helps detect cases of UE-26998 without causing a crash. Prevents the node from being rendered altogether and provides info on the state of the graph vs the node.
				// Because the editor won't crash, a GLEO can be expected if the node's outer is in the transient package.
				if (ensureMsgf(Node->GetOuter() == GraphObj, TEXT("Found %s ('%s') that does not belong to %s. Node Outer: %s, Node Outer Type: %s, Graph Outer: %s, Graph Outer Type: %s"),
					*Node->GetName(), *Node->GetClass()->GetName(),
					*GraphObj->GetName(),
					*Node->GetOuter()->GetName(), *Node->GetOuter()->GetClass()->GetName(),
					*GraphObj->GetOuter()->GetName(), *GraphObj->GetOuter()->GetClass()->GetName()
					))
 				{
					AddNode(Node, CheckUserAddedNodesList);
				}
				else
				{
					UE_LOG(LogGraphPanel, Error, TEXT("Found %s ('%s') that does not belong to %s. Node Outer: %s, Node Outer Type: %s, Graph Outer: %s, Graph Outer Type: %s"),
						*Node->GetName(), *Node->GetClass()->GetName(),
						*GraphObj->GetName(),
						*Node->GetOuter()->GetName(), *Node->GetOuter()->GetClass()->GetName(),
						*GraphObj->GetOuter()->GetName(), *GraphObj->GetOuter()->GetClass()->GetName()
					);
				}
			}
			else
			{
				UE_LOG(LogGraphPanel, Warning, TEXT("Found NULL Node in GraphObj array of a graph in asset '%s'. A node type has been deleted without creating an ActiveClassRedirector to K2Node_DeadClass."), *GraphObj->GetOutermost()->GetName());
			}
		}

		// find the last selection action, and execute it
		for (int32 ActionIndex = UserActions.Num() - 1; ActionIndex >= 0; --ActionIndex)
		{
			if (UserActions[ActionIndex].Action & GRAPHACTION_SelectNode)
			{
				DeferredSelectionTargetObjects.Empty();
				for (const UEdGraphNode* Node : UserActions[ActionIndex].Nodes)
				{
					DeferredSelectionTargetObjects.Add(Node);
				}
				break;
			}
		}
	}
	else
	{
		RemoveAllNodes();
	}

	// Clean out set of added nodes
	UserAddedNodes.Empty();
	UserActions.Empty();

	// Invoke any delegate methods
	OnUpdateGraphPanel.ExecuteIfBound();

	// Clear the update pending flag to allow deferred zoom commands to run.
	bVisualUpdatePending = false;
}

// Purges the existing visual representation (typically followed by an Update call in the next tick)
void SGraphPanel::PurgeVisualRepresentation()
{
	// No need to call OnSplineHoverStateChanged since we're about to destroy all the nodes and pins
	PreviousFrameSplineOverlap = FGraphSplineOverlapResult();

	// Clear all of the nodes and pins
	RemoveAllNodes();

	// Set a flag to know that an update is pending to prevent running pending commands like zoom to fit until widgets are generated.
	bVisualUpdatePending = true;
}

bool SGraphPanel::IsNodeTitleVisible(const class UEdGraphNode* Node, bool bRequestRename)
{
	bool bTitleVisible = false;
	TSharedRef<SNode>* pWidget = NodeToWidgetLookup.Find(Node);

	if (pWidget != nullptr)
	{
		TWeakPtr<SGraphNode> GraphNode = StaticCastSharedRef<SGraphNode>(*pWidget);
		if(GraphNode.IsValid() && !HasMouseCapture())
		{
			FSlateRect TitleRect = GraphNode.Pin()->GetTitleRect();
			const FVector2D TopLeft = FVector2D( TitleRect.Left, TitleRect.Top );
			const FVector2D BottomRight = FVector2D( TitleRect.Right, TitleRect.Bottom );

			if( IsRectVisible( TopLeft, BottomRight ))
			{
				bTitleVisible = true;
			}
			else if( bRequestRename )
			{
				bTitleVisible = JumpToRect( TopLeft, BottomRight );
			}

			if( bTitleVisible && bRequestRename )
			{
				GraphNode.Pin()->RequestRename();
				SelectAndCenterObject(Node, false);
			}
		}
	}
	return bTitleVisible;
}

bool SGraphPanel::IsRectVisible(const FVector2D &TopLeft, const FVector2D &BottomRight)
{
	return TopLeft.ComponentwiseAllGreaterOrEqual( PanelCoordToGraphCoord( FVector2D::ZeroVector )) && 
		BottomRight.ComponentwiseAllLessOrEqual( PanelCoordToGraphCoord( CachedAllottedGeometryScaledSize ) );
}

bool SGraphPanel::JumpToRect(const FVector2D &TopLeft, const FVector2D &BottomRight)
{
	ZoomToTarget(TopLeft, BottomRight);

	return true;
}

void SGraphPanel::JumpToNode(const UEdGraphNode* JumpToMe, bool bRequestRename, bool bSelectNode)
{
	if (JumpToMe != nullptr)
	{
		if (bRequestRename)
		{
			TSharedRef<SNode>* pWidget = NodeToWidgetLookup.Find(JumpToMe);
			if (pWidget != nullptr)
			{
				TSharedRef<SGraphNode> GraphNode = StaticCastSharedRef<SGraphNode>(*pWidget);
				GraphNode->RequestRename();
			}
		}

		if (bSelectNode)
		{
			// Select this node, and request that we jump to it.
			SelectAndCenterObject(JumpToMe, true);
		}
		else
		{
			// Jump to the node
			CenterObject(JumpToMe);
		}
	}
}

void SGraphPanel::JumpToPin(const UEdGraphPin* JumpToMe)
{
	if (JumpToMe != nullptr)
	{
		JumpToNode(JumpToMe->GetOwningNode(), false, true);
	}
}

void SGraphPanel::OnBeginPIE( const bool bIsSimulating )
{
	// Play the bounce curve on a continuous loop during PIE
	BounceCurve.Play( this->AsShared(), true );
}

void SGraphPanel::OnEndPIE( const bool bIsSimulating )
{
	// Stop the bounce curve
	BounceCurve.JumpToEnd();
}

void SGraphPanel::OnGraphChanged(const FEdGraphEditAction& EditAction)
{
	const bool bWillPurge = GraphObj->GetSchema()->ShouldAlwaysPurgeOnModification();
	if (bWillPurge)
	{
		if ((EditAction.Graph == GraphObj) &&
			(EditAction.Nodes.Num() > 0) &&
			EditAction.bUserInvoked)
		{
			int32 ActionIndex = UserActions.Num();
			if (EditAction.Action & GRAPHACTION_AddNode)
			{
				for (const UEdGraphNode* Node : EditAction.Nodes)
				{
					UserAddedNodes.Add(Node, ActionIndex);
				}
			}
			UserActions.Add(EditAction);
		}
	}
	else if ((EditAction.Graph == GraphObj) && (EditAction.Nodes.Num() > 0) )
	{
		// Remove action handled immediately by SGraphPanel::OnGraphChanged
		const bool bWasAddAction = (EditAction.Action & GRAPHACTION_AddNode) != 0;
		const bool bWasSelectAction = (EditAction.Action & GRAPHACTION_SelectNode) != 0;
		const bool bWasRemoveAction = (EditAction.Action & GRAPHACTION_RemoveNode) != 0;

		// The *only* reason we defer these actions is because code higher up the call stack
		// assumes that the node is created later (for example, GenerateBlueprintAPIUtils::AddNodeToGraph
		// calls AddNode (which calls this function) before calling AllocateDefaultPins, so if we create 
		// the widget immediately it won't be able to create its pins). There are lots of other examples, 
		// and I can't be sure that I've found them all.... 
		
		// Minor note, the ugly little lambdas are just to deal with the time values and return values 
		// that the timer system requires (and we don't leverage):
		if (bWasRemoveAction)
		{
			const auto RemoveNodeDelegateWrapper = [](double, float, SGraphPanel* Parent, TWeakObjectPtr<UEdGraphNode> NodePtr) -> EActiveTimerReturnType
			{
				if (NodePtr.IsValid())
				{
					UEdGraphNode* Node = NodePtr.Get();
					Parent->RemoveNode(Node);
				}
				return EActiveTimerReturnType::Stop;
			};

			for (const UEdGraphNode* Node : EditAction.Nodes)
			{
				TWeakObjectPtr<UEdGraphNode> NodePtr = MakeWeakObjectPtr(const_cast<UEdGraphNode*>(Node));
				RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateStatic(RemoveNodeDelegateWrapper, this, NodePtr));
			}
		}
		if (bWasAddAction)
		{
			const auto AddNodeDelegateWrapper = [](double, float, SGraphPanel* Parent, TWeakObjectPtr<UEdGraphNode> NodePtr, bool bForceUserAdded) -> EActiveTimerReturnType
			{
				if (NodePtr.IsValid())
				{
					UEdGraphNode* Node = NodePtr.Get();
					if(IsValid(Node))
					{
						Parent->RemoveNode(Node);
						Parent->AddNode(Node, bForceUserAdded ? WasUserAdded : NotUserAdded);
					}
				}
				return EActiveTimerReturnType::Stop;
			};

			for (const UEdGraphNode* Node : EditAction.Nodes)
			{
				TWeakObjectPtr<UEdGraphNode> NodePtr = MakeWeakObjectPtr(const_cast<UEdGraphNode*>(Node));
				RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateStatic(AddNodeDelegateWrapper, this, NodePtr, EditAction.bUserInvoked));
			}
		}
		if (bWasSelectAction)
		{
			const auto SelectNodeDelegateWrapper = [](double, float, SGraphPanel* Parent, TSet< TWeakObjectPtr<UEdGraphNode> > NodePtrs) -> EActiveTimerReturnType
			{
				Parent->DeferredSelectionTargetObjects.Empty();
				for (TWeakObjectPtr<UEdGraphNode>& NodePtr : NodePtrs)
				{
					if (NodePtr.IsValid())
					{
						UEdGraphNode* Node = NodePtr.Get();
						Parent->DeferredSelectionTargetObjects.Add(Node);
					}
				}
				return EActiveTimerReturnType::Stop;
			};

			TSet< TWeakObjectPtr<UEdGraphNode> > NodePtrSet;
			for (const UEdGraphNode* Node : EditAction.Nodes)
			{
				TWeakObjectPtr<UEdGraphNode> NodePtr = MakeWeakObjectPtr(const_cast<UEdGraphNode*>(Node));
				NodePtrSet.Add(NodePtr);
			}

			RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateStatic(SelectNodeDelegateWrapper, this, NodePtrSet));
		}
	}
}

void SGraphPanel::NotifyGraphChanged(const FEdGraphEditAction& EditAction)
{
	// Forward call
	OnGraphChanged(EditAction);
}

void SGraphPanel::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject( GraphObj );
}

FString SGraphPanel::GetReferencerName() const
{
	return TEXT("SGraphPanel");
}

EActiveTimerReturnType SGraphPanel::InvalidatePerTick(double InCurrentTime, float InDeltaTime)
{
	// Invalidate the layout so it will redraw.
	Invalidate(EInvalidateWidget::Layout);

	TimeLeftToInvalidatePerTick -= InDeltaTime;

	// When the time is done, stop the invalidation per tick because the UI will be static once more.
	if (TimeLeftToInvalidatePerTick <= 0.0f)
	{
		TimeLeftToInvalidatePerTick = 0.0f;
		return EActiveTimerReturnType::Stop;
	}
	return EActiveTimerReturnType::Continue;
}

void SGraphPanel::SetNodeFactory(const TSharedRef<class FGraphNodeFactory>& NewNodeFactory)
{
	NodeFactory = NewNodeFactory;
}
