// Copyright Epic Games, Inc. All Rights Reserved.

#include "Views/SDMXPixelMappingDesignerView.h"

#include "DMXPixelMapping.h"
#include "DMXPixelMappingComponentWidget.h"
#include "DMXPixelMappingEditorUtils.h"
#include "DMXPixelMappingEditorStyle.h"
#include "DMXPixelMappingLayoutSettings.h"
#include "SDMXPixelMappingComponentBox.h"
#include "Components/DMXPixelMappingOutputComponent.h"
#include "Components/DMXPixelMappingRendererComponent.h"
#include "Components/DMXPixelMappingRootComponent.h"
#include "Components/DMXPixelMappingFixtureGroupComponent.h"
#include "Components/DMXPixelMappingFixtureGroupItemComponent.h"
#include "Components/DMXPixelMappingMatrixComponent.h"
#include "Components/DMXPixelMappingMatrixCellComponent.h"
#include "Components/DMXPixelMappingScreenComponent.h"
#include "DragDrop/DMXPixelMappingDragDropOp.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXLibrary.h"
#include "Widgets/SDMXPixelMappingDesignerCanvas.h"
#include "Widgets/SDMXPixelMappingOutputComponent.h"
#include "Widgets/SDMXPixelMappingRuler.h"
#include "Widgets/SDMXPixelMappingSourceTextureViewport.h"
#include "Widgets/SDMXPixelMappingTransformHandle.h"
#include "Widgets/SDMXPixelMappingZoomPan.h"
#include "Templates/DMXPixelMappingComponentTemplate.h"
#include "Toolkits/DMXPixelMappingToolkit.h"

#include "ScopedTransaction.h"
#include "Widgets/SCanvas.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/SOverlay.h"
#include "Input/HittestGrid.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "SDMXPixelMappingDesignerView"

SDMXPixelMappingDesignerView::~SDMXPixelMappingDesignerView()
{
	GEditor->UnregisterForUndo(this);
}

void SDMXPixelMappingDesignerView::Construct(const FArguments& InArgs, const TSharedPtr<FDMXPixelMappingToolkit>& InToolkit)
{
	SDMXPixelMappingSurface::Construct(SDMXPixelMappingSurface::FArguments()
		.AllowContinousZoomInterpolation(false)
		.Content()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush(TEXT("Graph.TitleBackground")))
				.HAlign(HAlign_Fill)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(this, &SDMXPixelMappingDesignerView::GetHoveredComponentParentNameText)
							.TextStyle(FAppStyle::Get(), TEXT("GraphBreadcrumbButtonText"))
						]
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
							.Image(FAppStyle::GetBrush("BreadcrumbTrail.Delimiter"))
						]
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(this, &SDMXPixelMappingDesignerView::GetHoveredComponentNameText)
							.TextStyle(FAppStyle::Get(), TEXT("GraphBreadcrumbButtonText"))
						]
					]
				]
			]
			+SVerticalBox::Slot()
			[
				SNew(SGridPanel)
				.FillColumn(1, 1.0f)
				.FillRow(1, 1.0f)

				// Corner
				+ SGridPanel::Slot(0, 0)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
					.BorderBackgroundColor(FLinearColor(FColor(48, 48, 48)))
				]

				// Top Ruler
				+ SGridPanel::Slot(1, 0)
				[
					SAssignNew(TopRuler, SDMXPixelMappingRuler)
					.Orientation(Orient_Horizontal)
					.Visibility(this, &SDMXPixelMappingDesignerView::GetRulerVisibility)
				]

				// Side Ruler
				+ SGridPanel::Slot(0, 1)
				[
					SAssignNew(SideRuler, SDMXPixelMappingRuler)
					.Orientation(Orient_Vertical)
					.Visibility(this, &SDMXPixelMappingDesignerView::GetRulerVisibility)
				]

				+ SGridPanel::Slot(1, 1)
				[
					SAssignNew(PreviewHitTestRoot, SOverlay)
					.Visibility(EVisibility::Visible)
					.Clipping(EWidgetClipping::ClipToBoundsAlways)
						
					+ SOverlay::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					[
						SNew(SDMXPixelMappingZoomPan)
						.ZoomAmount(this, &SDMXPixelMappingDesignerView::GetZoomAmount)
						.ViewOffset(this, &SDMXPixelMappingDesignerView::GetViewOffset)
						.Visibility(this, &SDMXPixelMappingDesignerView::GetZoomPanVisibility)

						[
							SNew(SOverlay)

							+ SOverlay::Slot()
							[
								SAssignNew(SourceTextureViewport, SDMXPixelMappingSourceTextureViewport, InToolkit)
							]

							+ SOverlay::Slot()
							[
								SAssignNew(PreviewSizeConstraint, SBox)
							]

							+ SOverlay::Slot()
							[
								SAssignNew(DesignCanvasBorder, SBorder)
								.HAlign(HAlign_Fill)
								.VAlign(VAlign_Fill)
								.BorderImage(FAppStyle::GetBrush("NoBorder"))
								.Padding(0.0f)
								[
									SAssignNew(DesignCanvas, SConstraintCanvas)
									.Visibility(EVisibility::HitTestInvisible)
								]
							]
						]
					]

					// A layer in the overlay where we put all the tools for the user
					+ SOverlay::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					[
						SAssignNew(ExtensionWidgetCanvas, SCanvas)
						.Visibility(this, &SDMXPixelMappingDesignerView::GetExtensionCanvasVisibility)
					]

					// Designer overlay UI, toolbar, status messages, zoom level...etc
					+ SOverlay::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					[
						 SNew(SOverlay)
						+ SOverlay::Slot()
						.Padding(0)
						+SOverlay::Slot()
						.HAlign(HAlign_Fill)
						.VAlign(VAlign_Top)
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(6, 2, 0, 0)
							[
								SNew(STextBlock)
								.TextStyle(FAppStyle::Get(), "Graph.ZoomText")
								.Text(this, &SDMXPixelMappingDesignerView::GetZoomText)
								.ColorAndOpacity(this, &SDMXPixelMappingDesignerView::GetZoomTextColorAndOpacity)
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(40, 2, 0, 0)
							[
								SNew(STextBlock)
								.TextStyle(FAppStyle::Get(), "Graph.ZoomText")
								.Font(FCoreStyle::GetDefaultFontStyle(TEXT("BoldCondensed"), 14))
								.Text(this, &SDMXPixelMappingDesignerView::GetCursorPositionText)
								.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.25f))
								.Visibility(this, &SDMXPixelMappingDesignerView::GetCursorPositionTextVisibility)
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							[
								SNew(SSpacer)
								.Size(FVector2D(1, 1))
							]

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(SButton)
								.ButtonColorAndOpacity(FLinearColor::Transparent)
								.ButtonStyle(FAppStyle::Get(), "ViewportMenu.Button")
								.ToolTipText(LOCTEXT("ZoomToFit_ToolTip", "Zoom To Fit"))
								.OnClicked(this, &SDMXPixelMappingDesignerView::OnZoomToFitClicked)
								.ContentPadding(FAppStyle::Get().GetMargin("ViewportMenu.SToolBarButtonBlock.Button.Padding"))
								[
									SNew(SImage)
									.Image(FDMXPixelMappingEditorStyle::Get().GetBrush("Icons.ZoomToFit"))
								]
							]
						]
					]
				]
			]
		]
	, InToolkit);

	ZoomToFit(true);

	HittestGrid = MakeShared<FHittestGrid>();

	// Bind to selection changes
	InToolkit->GetOnSelectedComponentsChangedDelegate().AddSP(this, &SDMXPixelMappingDesignerView::OnSelectedComponentsChanged);

	// Bind to component changes
	UDMXPixelMappingBaseComponent::GetOnComponentAdded().AddSP(this, &SDMXPixelMappingDesignerView::OnComponentAdded);
	UDMXPixelMappingBaseComponent::GetOnComponentRemoved().AddSP(this, &SDMXPixelMappingDesignerView::OnComponentRemoved);

	GEditor->RegisterForUndo(this);
}

FOptionalSize SDMXPixelMappingDesignerView::GetPreviewAreaWidth() const
{
	FVector2D Area;
	FVector2D Size;
	GetPreviewAreaAndSize(Area, Size);

	return Area.X;
}

FOptionalSize SDMXPixelMappingDesignerView::GetPreviewAreaHeight() const
{
	FVector2D Area;
	FVector2D Size;
	GetPreviewAreaAndSize(Area, Size);

	return Area.Y;
}

float SDMXPixelMappingDesignerView::GetPreviewScale() const
{
	return GetZoomAmount();
}

FReply SDMXPixelMappingDesignerView::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SDMXPixelMappingSurface::OnMouseButtonDown(MyGeometry, MouseEvent);

	const bool bLeftMouseButton = MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
	UDMXPixelMappingOutputComponent* ClickedComponent = GetComponentUnderCursor();
	if (bLeftMouseButton && ClickedComponent)
	{
		PendingSelectedComponent = ClickedComponent;

		// Handle reclicking a selected component depending on type
		const bool bClickedSelectedComponent = [ClickedComponent, this]()
		{
			for (const FDMXPixelMappingComponentReference& SelectedComponentRef : GetSelectedComponents())
			{
				if (ClickedComponent == SelectedComponentRef.GetComponent())
				{
					return true;
				}
				else if (UDMXPixelMappingMatrixCellComponent* ClickedMatrixCellComponent = Cast<UDMXPixelMappingMatrixCellComponent>(ClickedComponent))
				{
					if (ClickedMatrixCellComponent->GetParent() == SelectedComponentRef.GetComponent())
					{
						return true;
					}
				}
				else if (UDMXPixelMappingFixtureGroupItemComponent* ClickedGroupItemComponent = Cast<UDMXPixelMappingFixtureGroupItemComponent>(ClickedComponent))
				{
					if (ClickedGroupItemComponent->GetParent() == SelectedComponentRef.GetComponent() &&
						ClickedGroupItemComponent->IsLockInDesigner())
					{
						return true;
					}
				}
			}

			return false;
		}();

		const bool bClearPreviousSelection = !MouseEvent.IsShiftDown() && !MouseEvent.IsControlDown() && !bClickedSelectedComponent;
		ResolvePendingSelectedComponents(bClearPreviousSelection);

		FVector2D GraphSpaceCursorPosition;
		if (GetGraphSpaceCursorPosition(GraphSpaceCursorPosition))
		{
			DragAnchor = GraphSpaceCursorPosition;

			return
				FReply::Handled()
				.PreventThrottling()
				.SetUserFocus(AsShared(), EFocusCause::Mouse)
				.CaptureMouse(AsShared())
				.DetectDrag(AsShared(), EKeys::LeftMouseButton);
		}
	}

	// Capture mouse for the drag handle and general mouse actions
	return 
		FReply::Handled()
		.PreventThrottling()
		.SetUserFocus(AsShared(), EFocusCause::Mouse)
		.CaptureMouse(AsShared());
}

FReply SDMXPixelMappingDesignerView::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SDMXPixelMappingSurface::OnMouseButtonUp(MyGeometry, MouseEvent);

	// Select the output component under the mouse
	PendingSelectedComponent = GetComponentUnderCursor();

	// Select the Renderer if no Output Component was under the mouse
	if (!PendingSelectedComponent.IsValid())
	{
		if (TSharedPtr<FDMXPixelMappingToolkit> ToolkitPtr = ToolkitWeakPtr.Pin())
		{
			if (UDMXPixelMappingRendererComponent* RendererComponent = ToolkitPtr->GetActiveRendererComponent())
			{
				PendingSelectedComponent = RendererComponent;
			}
		}
	}

	const bool bClearPreviousSelection = !MouseEvent.IsShiftDown() && !MouseEvent.IsControlDown();
	ResolvePendingSelectedComponents(bClearPreviousSelection);

	return FReply::Handled()
		.EndDragDrop()
		.ReleaseMouseCapture()
		.SetUserFocus(AsShared());
}

FReply SDMXPixelMappingDesignerView::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetCursorDelta().IsZero())
	{
		return FReply::Unhandled();
	}

	FReply SurfaceHandled = SDMXPixelMappingSurface::OnMouseMove(MyGeometry, MouseEvent);
	if (SurfaceHandled.IsEventHandled())
	{
		return SurfaceHandled;
	}

	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && HasMouseCapture())
	{
		bool bIsRootWidgetSelected = false;
		for (const FDMXPixelMappingComponentReference& ComponentReference : GetSelectedComponents())
		{
			UDMXPixelMappingBaseComponent* Component = ComponentReference.GetComponent();
			if (Component && Component->GetParent())
			{
				bIsRootWidgetSelected = true;
				break;
			}
		}

		if (!bIsRootWidgetSelected)
		{
			//Drag selected widgets
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

FReply SDMXPixelMappingDesignerView::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Delete)
	{
		if (const TSharedPtr<FDMXPixelMappingToolkit> ToolkitPtr = ToolkitWeakPtr.Pin())
		{
			const FScopedTransaction Transaction(FText::Format(LOCTEXT("DMXPixelMapping.RemoveComponents", "Remove {0}|plural(one=Component, other=Components)"), ToolkitPtr->GetSelectedComponents().Num()));

			ToolkitPtr->DeleteSelectedComponents();

			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

void SDMXPixelMappingDesignerView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SDMXPixelMappingSurface::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (const TSharedPtr<FDMXPixelMappingToolkit> Toolkit = ToolkitWeakPtr.Pin())
	{
		// Handle drag over on tick
		if (PendingDragDropOp.IsValid())
		{
			FVector2D GraphSpaceCursorPosition;
			if (GetGraphSpaceCursorPosition(GraphSpaceCursorPosition))
			{
				// Layout childs with the group item drag drop helper if possible, else use the default layout method
				if (TSharedPtr<FDMXPixelMappingGroupChildDragDropHelper> GroupChildDragDropHelper = PendingDragDropOp->GetGroupChildDragDropHelper())
				{
					// If it was created from template it was dragged in from details or palette. In this case align the components.
					const bool bAlignComponents = PendingDragDropOp->WasCreatedAsTemplate();

					GroupChildDragDropHelper->LayoutComponents(GraphSpaceCursorPosition, bAlignComponents);
				}
				else
				{
					PendingDragDropOp->LayoutOutputComponents(GraphSpaceCursorPosition);
				}
			}

			PendingDragDropOp.Reset();
		}

		// Update test if the renderer changed, and if so, rebuild the designer
		UDMXPixelMappingRendererComponent* RendererComponent = Toolkit->GetActiveRendererComponent();
		if (RendererComponent != CachedRendererComponent)
		{
			CachedRendererComponent = RendererComponent;
			RebuildDesigner();
		}

		// Cache geometry
		CachedWidgetGeometry.Reset();
		FArrangedWidget WindowWidgetGeometry(PreviewHitTestRoot.ToSharedRef(), GetDesignerGeometry());
		PopulateWidgetGeometryCache(WindowWidgetGeometry);

		// Compute the origin in absolute space.
		FGeometry RootGeometry = CachedWidgetGeometry.FindChecked(PreviewSizeConstraint.ToSharedRef()).Geometry;
		FVector2D AbsoluteOrigin = MakeGeometryWindowLocal(RootGeometry).LocalToAbsolute(FVector2D::ZeroVector);

		GridOrigin = AbsoluteOrigin;

		// Roler position
		TopRuler->SetRuling(AbsoluteOrigin, 1.0f / GetPreviewScale());
		SideRuler->SetRuling(AbsoluteOrigin, 1.0f / GetPreviewScale());

		if (IsHovered())
		{
			// Get cursor in absolute window space.
			FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();
			CursorPos = MakeGeometryWindowLocal(RootGeometry).LocalToAbsolute(RootGeometry.AbsoluteToLocal(CursorPos));

			TopRuler->SetCursor(CursorPos);
			SideRuler->SetCursor(CursorPos);
		}
		else
		{
			TopRuler->SetCursor(TOptional<FVector2D>());
			SideRuler->SetCursor(TOptional<FVector2D>());
		}
	}
}

FReply SDMXPixelMappingDesignerView::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SDMXPixelMappingSurface::OnDragDetected(MyGeometry, MouseEvent);

	const TSet<FDMXPixelMappingComponentReference>& SelectedComponents = GetSelectedComponents();

	if (SelectedComponents.Num() > 0)
	{
		TArray<UDMXPixelMappingOutputComponent*> DraggedComponentCandidates;

		for (const FDMXPixelMappingComponentReference& SelectedComponent : SelectedComponents)
		{
			if (UDMXPixelMappingOutputComponent* OutputComponent = Cast<UDMXPixelMappingOutputComponent>(SelectedComponent.GetComponent()))
			{
				DraggedComponentCandidates.Add(OutputComponent);
			}
		}

		TArray<TWeakObjectPtr<UDMXPixelMappingBaseComponent>> DraggedComponents;
		for (UDMXPixelMappingOutputComponent* Candidate : DraggedComponentCandidates)
		{
			// Check the parent chain of each dragged component and ignore those that are children of other dragged components
			bool bIsChild = false;
			for (TWeakObjectPtr<UDMXPixelMappingBaseComponent> Parent = Candidate->GetParent(); Parent.IsValid(); Parent = Parent->GetParent())
			{
				bIsChild = DraggedComponentCandidates.ContainsByPredicate([Parent](const UDMXPixelMappingOutputComponent* BaseComponent)
					{
						return BaseComponent == Parent.Get();
					});

				if (bIsChild)
				{
					break;
				}
			}

			if (!bIsChild && !Candidate->IsLockInDesigner())
			{
				DraggedComponents.Add(Candidate);
			}
		}

		// We know the first element is the one that was selected via mouse
		if (DraggedComponents.Num() > 0)
		{
			// We rely on the first being the clicked component. There's an according comment in the DragDropOp and the detected drag is raised here.
			UDMXPixelMappingOutputComponent* ClickedComponent = CastChecked<UDMXPixelMappingOutputComponent>(DraggedComponents[0]);

			const FVector2D GraphSpaceDragOffset = DragAnchor - ClickedComponent->GetPosition();

			TSharedRef<FDMXPixelMappingDragDropOp> DragDropOp = FDMXPixelMappingDragDropOp::New(GraphSpaceDragOffset, DraggedComponents);
			DragDropOp->SetDecoratorVisibility(false);

			// Clear any pending selected widgets
			PendingSelectedComponent = nullptr;

			return FReply::Handled().BeginDragDrop(DragDropOp);
		}
	}

	return FReply::Handled();
}

void SDMXPixelMappingDesignerView::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	const TSharedPtr<FDMXPixelMappingDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FDMXPixelMappingDragDropOp>();
	if (DragDropOp.IsValid())
	{
		const FGeometry WidgetUnderCursorGeometry = PreviewSizeConstraint->GetTickSpaceGeometry();
		const FVector2D ScreenSpacePosition = DragDropEvent.GetScreenSpacePosition();
		const FVector2D LocalPosition = WidgetUnderCursorGeometry.AbsoluteToLocal(ScreenSpacePosition);

		// If this was dragged into designer, but has no components yet, it was dragged in from Palette or Details
		if (DragDropOp->WasCreatedAsTemplate())
		{
			HandleDragEnterFromDetailsOrPalette(DragDropOp);
		}
	}

	SDMXPixelMappingSurface::OnDragEnter(MyGeometry, DragDropEvent);
}

void SDMXPixelMappingDesignerView::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	SDMXPixelMappingSurface::OnDragLeave(DragDropEvent);

	const TSharedPtr<FDMXPixelMappingDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FDMXPixelMappingDragDropOp>();
	if (DragDropOp.IsValid())
	{
		if (TSharedPtr<FDMXPixelMappingToolkit> ToolkitPtr = ToolkitWeakPtr.Pin())
		{
			FScopedRestoreSelection(ToolkitPtr.ToSharedRef(), StaticCastSharedRef<SDMXPixelMappingDesignerView>(AsShared()));

			// If the drag drop op was dragged in from details or palette, remove the components
			if (DragDropOp->WasCreatedAsTemplate())
			{
				TSet<FDMXPixelMappingComponentReference> Parents;
				for (const TWeakObjectPtr<UDMXPixelMappingBaseComponent>& Component : DragDropOp->GetDraggedComponents())
				{
					if (Component.IsValid() && Component->GetParent())
					{
						Parents.Add(ToolkitPtr->GetReferenceFromComponent(Component->GetParent()));

						Component->GetParent()->RemoveChild(Component.Get());
					}
				}

				// Select parents instead
				ToolkitPtr->SelectComponents(Parents);
			}

			RebuildDesigner();
		}
	}
}

FReply SDMXPixelMappingDesignerView::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	SDMXPixelMappingSurface::OnDragOver(MyGeometry, DragDropEvent);

	const TSharedPtr<FDMXPixelMappingDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FDMXPixelMappingDragDropOp>();
	if (DragDropOp.IsValid())
	{
		// Handle the drag drop op on tick
		PendingDragDropOp = DragDropOp;
	}

	return FReply::Handled();
}

FReply SDMXPixelMappingDesignerView::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	SDMXPixelMappingSurface::OnDrop(MyGeometry, DragDropEvent);

	return FReply::Handled().EndDragDrop().SetUserFocus(AsShared());
}

FSlateRect SDMXPixelMappingDesignerView::ComputeAreaBounds() const
{
	FSlateRect Bounds = FSlateRect(0.f, 0.f, 0.f, 0.f);

	const TSet<FDMXPixelMappingComponentReference> SelectedComponentReferences = GetSelectedComponents();
	for (const FDMXPixelMappingComponentReference& ComponentReference : SelectedComponentReferences)
	{
		if (UDMXPixelMappingBaseComponent* Component = ComponentReference.GetComponent())
		{
			if (UDMXPixelMappingRendererComponent* RendererComponent = Component->GetRendererComponent())
			{
				Bounds.Left = FMath::Min(RendererComponent->GetPosition().X, Bounds.Left);
				Bounds.Top = FMath::Min(RendererComponent->GetPosition().Y, Bounds.Top);

				Bounds.Right = FMath::Max(RendererComponent->GetPosition().X + RendererComponent->GetSize().X, Bounds.Right);
				Bounds.Bottom = FMath::Max(RendererComponent->GetPosition().Y + RendererComponent->GetSize().Y, Bounds.Bottom);

				break;
			}
		}
	}

	return Bounds;
}

int32 SDMXPixelMappingDesignerView::GetGraphRulePeriod() const
{
	return 10; // Parent override
}

float SDMXPixelMappingDesignerView::GetGridScaleAmount() const
{
	return 1.0f; // Parent override
}

int32 SDMXPixelMappingDesignerView::GetSnapGridSize() const
{
	return 4; // Parent override
}

void SDMXPixelMappingDesignerView::PostUndo(bool bSuccess)
{
	CachedRendererComponent = nullptr;
}

void SDMXPixelMappingDesignerView::PostRedo(bool bSuccess)
{
	CachedRendererComponent = nullptr;
}

void SDMXPixelMappingDesignerView::RebuildDesigner()
{
	const TSharedPtr<FDMXPixelMappingToolkit> Toolkit = ToolkitWeakPtr.Pin();
	if (!Toolkit.IsValid())
	{
		return;
	}

	if (DesignCanvas.IsValid())
	{
		DesignCanvas->ClearChildren();
	}

	DesignCanvasBorder->SetContent
	(
		SAssignNew(DesignCanvas, SDMXPixelMappingDesignerCanvas)
	);

	OutputComponentWidgets.Reset();
	if (CachedRendererComponent.IsValid())
	{
		const TSharedRef<SConstraintCanvas> ComponentCanvas = SNew(SConstraintCanvas);
		DesignCanvas->AddSlot()
			[
				ComponentCanvas
			];

		CachedRendererComponent->ForEachChild([this, &Toolkit, &ComponentCanvas](UDMXPixelMappingBaseComponent* Component)
			{
				if (UDMXPixelMappingScreenComponent* ScreenComponent = Cast<UDMXPixelMappingScreenComponent>(Component))
				{
					const TSharedRef<SDMXPixelMappingScreenComponent> ScreenComponentWidget = SNew(SDMXPixelMappingScreenComponent, Toolkit.ToSharedRef(), ScreenComponent);
					ScreenComponentWidget->AddToCanvas(ComponentCanvas);

					OutputComponentWidgets.Add(ScreenComponentWidget);
				}
				else if (UDMXPixelMappingOutputComponent* OutputComponent = Cast<UDMXPixelMappingOutputComponent>(Component))
				{
					const TSharedRef<SDMXPixelMappingOutputComponent> ComponentWidget = SNew(SDMXPixelMappingOutputComponent, Toolkit.ToSharedRef(), OutputComponent);
					ComponentWidget->AddToCanvas(ComponentCanvas);

					OutputComponentWidgets.Add(ComponentWidget);
				}
			}, true);
	}
}

void SDMXPixelMappingDesignerView::CreateExtensionWidgetsForSelection()
{
	// Remove all the current extension widgets
	ClearExtensionWidgets();

	// Get the selected widgets as an array
	const TArray<FDMXPixelMappingComponentReference>& SelectedComponents = GetSelectedComponents().Array();

	if (SelectedComponents.Num() == 1)
	{
		if (UDMXPixelMappingOutputComponent* OutputComponent = Cast<UDMXPixelMappingOutputComponent>(SelectedComponents[0].GetComponent()))
		{
			if (OutputComponent->IsVisible() && !OutputComponent->IsLockInDesigner())
			{
				// Add transform handles
				constexpr float Offset = 10.f;
				const TSharedPtr<SDMXPixelMappingDesignerView> Self = SharedThis(this);
				TransformHandles.Add(SNew(SDMXPixelMappingTransformHandle, Self, EDMXPixelMappingTransformDirection::CenterRight, FVector2D(Offset, 0.f)));
				TransformHandles.Add(SNew(SDMXPixelMappingTransformHandle, Self, EDMXPixelMappingTransformDirection::BottomCenter, FVector2D(0.f, Offset)));
				TransformHandles.Add(SNew(SDMXPixelMappingTransformHandle, Self, EDMXPixelMappingTransformDirection::BottomRight, FVector2D(Offset, Offset)));

				// Add Widgets to designer surface
				for (TSharedPtr<SDMXPixelMappingTransformHandle>& Handle : TransformHandles)
				{
					ExtensionWidgetCanvas->AddSlot()
						.Position(TAttribute<FVector2D>::Create(TAttribute<FVector2D>::FGetter::CreateSP(this, &SDMXPixelMappingDesignerView::GetExtensionPosition, Handle)))
						.Size(TAttribute<FVector2D>::Create(TAttribute<FVector2D>::FGetter::CreateSP(this, &SDMXPixelMappingDesignerView::GetExtensionSize, Handle)))
						[
							Handle.ToSharedRef()
						];
				}
			}
		}
	}
}

void SDMXPixelMappingDesignerView::ClearExtensionWidgets()
{
	ExtensionWidgetCanvas->ClearChildren();
}

EVisibility SDMXPixelMappingDesignerView::GetExtensionCanvasVisibility() const
{
	for (const FDMXPixelMappingComponentReference& Component : GetSelectedComponents())
	{
		UDMXPixelMappingOutputComponent* OutputComponent = Cast<UDMXPixelMappingOutputComponent>(Component.GetComponent());

		if (!OutputComponent ||
			!OutputComponent->IsVisible() ||
			OutputComponent->IsLockInDesigner())
		{
			return EVisibility::Hidden;
		}
	}
	return EVisibility::SelfHitTestInvisible;
}

FVector2D SDMXPixelMappingDesignerView::GetExtensionPosition(TSharedPtr<SDMXPixelMappingTransformHandle> Handle)
{
	const FDMXPixelMappingComponentReference& SelectedComponent = GetSelectedComponent();

	if (SelectedComponent.IsValid())
	{
		FGeometry SelectedComponentGeometry;
		FGeometry SelectedComponentParentGeometry;

		if (GetComponentGeometry(SelectedComponent, SelectedComponentGeometry))
		{
			const FVector2D WidgetPosition = [Handle, &SelectedComponentGeometry]()
			{
				// Get the initial offset based on the location around the selected object.
				switch (Handle->GetTransformDirection())
				{
				case EDMXPixelMappingTransformDirection::CenterRight:
					return FVector2D(SelectedComponentGeometry.GetLocalSize().X, SelectedComponentGeometry.GetLocalSize().Y * 0.5f);
				case EDMXPixelMappingTransformDirection::BottomLeft:
					return FVector2D(0, SelectedComponentGeometry.GetLocalSize().Y);
				case EDMXPixelMappingTransformDirection::BottomCenter:
					return FVector2D(SelectedComponentGeometry.GetLocalSize().X * 0.5f, SelectedComponentGeometry.GetLocalSize().Y);
				case EDMXPixelMappingTransformDirection::BottomRight:
					return SelectedComponentGeometry.GetLocalSize();
				default:
					checkNoEntry(); // Unhandled enum value
				}
				return FVector2D::ZeroVector;
			}();

			const FVector2D SelectedWidgetScale = FVector2D(SelectedComponentGeometry.GetAccumulatedRenderTransform().GetMatrix().GetScale().GetVector());

			const FVector2D ApplicationScaledOffset = Handle->GetOffset() * GetDesignerGeometry().Scale;

			const FVector2D LocalOffsetFull = ApplicationScaledOffset / SelectedWidgetScale;
			const FVector2D PositionFullOffset = GetDesignerGeometry().AbsoluteToLocal(SelectedComponentGeometry.LocalToAbsolute(WidgetPosition + LocalOffsetFull));
			const FVector2D LocalOffsetHalf = (ApplicationScaledOffset / 2.0f) / SelectedWidgetScale;
			const FVector2D PositionHalfOffset = GetDesignerGeometry().AbsoluteToLocal(SelectedComponentGeometry.LocalToAbsolute(WidgetPosition + LocalOffsetHalf));

			FVector2D PivotCorrection = PositionHalfOffset - (PositionFullOffset + FVector2D(5.0f, 5.0f));

			const FVector2D FinalPosition = PositionFullOffset + PivotCorrection;

			return FinalPosition;
		}
	}

	return FVector2D(0.f, 0.f);
}

FVector2D SDMXPixelMappingDesignerView::GetExtensionSize(TSharedPtr<SDMXPixelMappingTransformHandle> Handle)
{
	return Handle->GetDesiredSize();
}

FText SDMXPixelMappingDesignerView::GetHoveredComponentNameText() const
{
	if (UDMXPixelMappingOutputComponent* ComponentUnderCursor = GetComponentUnderCursor())
	{
		return FText::FromString(ComponentUnderCursor->GetUserFriendlyName());
	}

	return FText();
}

FText SDMXPixelMappingDesignerView::GetHoveredComponentParentNameText() const
{
	if (UDMXPixelMappingOutputComponent* ComponentUnderCursor = GetComponentUnderCursor())
	{
		if (UDMXPixelMappingBaseComponent* ParentOfComponentUnderCursor = ComponentUnderCursor->GetParent())
		{
			return FText::FromString(ParentOfComponentUnderCursor->GetUserFriendlyName());
		}
	}

	return FText();
}

EVisibility SDMXPixelMappingDesignerView::GetRulerVisibility() const
{
	return EVisibility::Visible;
}

FText SDMXPixelMappingDesignerView::GetCursorPositionText() const
{
	FVector2D CursorPosition;
	if (GetGraphSpaceCursorPosition(CursorPosition))
	{
		return FText::Format(LOCTEXT("CursorPositionFormat", "{0} x {1}"), FText::AsNumber(FMath::RoundToInt(CursorPosition.X)), FText::AsNumber(FMath::RoundToInt(CursorPosition.Y)));
	}
	return FText();
}

EVisibility SDMXPixelMappingDesignerView::GetCursorPositionTextVisibility() const
{
	return IsHovered() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
}

EVisibility SDMXPixelMappingDesignerView::GetZoomPanVisibility() const
{
	if (UDMXPixelMappingRendererComponent* RendererComponent = CachedRendererComponent.Get())
	{
		if (RendererComponent->GetRendererInputTexture())
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Collapsed;
}

FReply SDMXPixelMappingDesignerView::OnZoomToFitClicked()
{
	ZoomToFit(/*bInstantZoom*/ false);
	return FReply::Handled();
}

void SDMXPixelMappingDesignerView::OnComponentAdded(UDMXPixelMapping* PixelMapping, UDMXPixelMappingBaseComponent* Component)
{
	CachedRendererComponent = nullptr;
}

void SDMXPixelMappingDesignerView::OnComponentRemoved(UDMXPixelMapping* PixelMapping, UDMXPixelMappingBaseComponent* Component)
{
	CachedRendererComponent = nullptr;
}

void SDMXPixelMappingDesignerView::OnSelectedComponentsChanged()
{
	CreateExtensionWidgetsForSelection();
}

void SDMXPixelMappingDesignerView::ResolvePendingSelectedComponents(bool bClearPreviousSelection)
{
	const TSharedPtr<FDMXPixelMappingToolkit> Toolkit = ToolkitWeakPtr.Pin();
	const bool bHasPendingSelectedComponent = PendingSelectedComponent.IsValid();
	if (Toolkit.IsValid() && bHasPendingSelectedComponent)
	{
		// Never select matrix cells
		if (UDMXPixelMappingMatrixCellComponent* MatrixCellComponent = Cast<UDMXPixelMappingMatrixCellComponent>(PendingSelectedComponent))
		{
			PendingSelectedComponent = MatrixCellComponent->GetParent();
		}

		// Select group if desired
		const UDMXPixelMappingLayoutSettings* LayoutSettings = GetDefault<UDMXPixelMappingLayoutSettings>();
		if (LayoutSettings && LayoutSettings->bAlwaysSelectGroup)
		{
			if (UDMXPixelMappingFixtureGroupItemComponent* GroupItemComponent = Cast<UDMXPixelMappingFixtureGroupItemComponent>(PendingSelectedComponent))
			{
				PendingSelectedComponent = GroupItemComponent->GetParent();
			}
			else if (UDMXPixelMappingMatrixComponent* MatrixComponent = Cast<UDMXPixelMappingMatrixComponent>(PendingSelectedComponent))
			{
				PendingSelectedComponent = MatrixComponent->GetParent();
			}
		}

		// Add the newly selected component first. This is important, e.g. for drag drop when iterating and using this as base
		TSet<FDMXPixelMappingComponentReference> SelectedComponents;
		SelectedComponents.Add(Toolkit->GetReferenceFromComponent(PendingSelectedComponent.Get()));

		if (!bClearPreviousSelection)
		{
			SelectedComponents.Append(ToolkitWeakPtr.Pin()->GetSelectedComponents());
		}
		Toolkit->SelectComponents(SelectedComponents);

		PendingSelectedComponent = nullptr;
	}
}

void SDMXPixelMappingDesignerView::PopulateWidgetGeometryCache(FArrangedWidget& Root)
{
	const FSlateRect Rect = PreviewHitTestRoot->GetTickSpaceGeometry().GetLayoutBoundingRect();
	const FSlateRect PaintRect = PreviewHitTestRoot->GetPaintSpaceGeometry().GetLayoutBoundingRect();
	HittestGrid->SetHittestArea(Rect.GetTopLeft(), Rect.GetSize(), PaintRect.GetTopLeft());
	HittestGrid->Clear();

	PopulateWidgetGeometryCache_Loop(Root);
}

void SDMXPixelMappingDesignerView::PopulateWidgetGeometryCache_Loop(FArrangedWidget& CurrentWidget)
{
	bool bIncludeInHitTestGrid = true;

	if (bIncludeInHitTestGrid)
	{
		HittestGrid->AddWidget(&(CurrentWidget.Widget.Get()), 0, 0, FSlateInvalidationWidgetSortOrder());
	}

	FArrangedChildren ArrangedChildren(EVisibility::All);
	CurrentWidget.Widget->ArrangeChildren(CurrentWidget.Geometry, ArrangedChildren);

	CachedWidgetGeometry.Add(CurrentWidget.Widget, CurrentWidget);

	for (int32 ChildIndex = 0; ChildIndex < ArrangedChildren.Num(); ++ChildIndex)
	{
		FArrangedWidget& SomeChild = ArrangedChildren[ChildIndex];
		PopulateWidgetGeometryCache_Loop(SomeChild);
	}
}

UDMXPixelMappingOutputComponent* SDMXPixelMappingDesignerView::GetComponentUnderCursor() const
{
	UDMXPixelMappingOutputComponent* ComponentUnderCursor = nullptr;
	if (UDMXPixelMappingRendererComponent* RendererComponent = CachedRendererComponent.Get())
	{
		FVector2D GraphSpaceCursorPosition;
		if (GetGraphSpaceCursorPosition(GraphSpaceCursorPosition))
		{
			// Widgets are offset by 0.5f to reside over the pixel
			GraphSpaceCursorPosition += FVector2D(.5f, .5f);
			RendererComponent->ForEachChild([&ComponentUnderCursor, &GraphSpaceCursorPosition](UDMXPixelMappingBaseComponent* InComponent)
				{
					if (UDMXPixelMappingOutputComponent* OutputComponent = Cast<UDMXPixelMappingOutputComponent>(InComponent))
					{
						if (!ComponentUnderCursor ||
							ComponentUnderCursor->GetZOrder() <= OutputComponent->GetZOrder())
						{
							if (OutputComponent->IsVisible() &&
								OutputComponent->IsOverPosition(GraphSpaceCursorPosition))
							{
								ComponentUnderCursor = OutputComponent;
							}
						}
					}
				}, true);
		}
	}

	return ComponentUnderCursor;
}

TSet<FDMXPixelMappingComponentReference> SDMXPixelMappingDesignerView::GetSelectedComponents() const
{
	if (TSharedPtr<FDMXPixelMappingToolkit> ToolkitPtr = ToolkitWeakPtr.Pin())
	{
		return ToolkitPtr->GetSelectedComponents();
	}

	return TSet<FDMXPixelMappingComponentReference>();
}

FDMXPixelMappingComponentReference SDMXPixelMappingDesignerView::GetSelectedComponent() const
{
	const TSet<FDMXPixelMappingComponentReference>& SelectedComponents = GetSelectedComponents();

	// Only return a selected widget when we have only a single item selected.
	if (SelectedComponents.Num() == 1)
	{
		for (TSet<FDMXPixelMappingComponentReference>::TConstIterator SetIt(SelectedComponents); SetIt; ++SetIt)
		{
			return *SetIt;
		}
	}

	return FDMXPixelMappingComponentReference();
}

bool SDMXPixelMappingDesignerView::GetComponentGeometry(FDMXPixelMappingComponentReference InComponentReference, FGeometry& OutGeometry)
{
	if (UDMXPixelMappingBaseComponent* ComponentPreview = InComponentReference.GetComponent())
	{
		return GetComponentGeometry(ComponentPreview, OutGeometry);
	}

	return false;
}

bool SDMXPixelMappingDesignerView::GetComponentGeometry(UDMXPixelMappingBaseComponent* InBaseComponent, FGeometry& OutGeometry)
{
	if (UDMXPixelMappingOutputComponent* OutputComponent = Cast<UDMXPixelMappingOutputComponent>(InBaseComponent))
	{
		const TSharedRef<IDMXPixelMappingOutputComponentWidgetInterface>* ComponentViewPtr = Algo::FindByPredicate(OutputComponentWidgets, [InBaseComponent](const TSharedRef<IDMXPixelMappingOutputComponentWidgetInterface>& ComponentWidget)
			{
				return ComponentWidget->Equals(InBaseComponent);
			});

		if (ComponentViewPtr)
		{
			const FArrangedWidget* ArrangedWidget = CachedWidgetGeometry.Find((*ComponentViewPtr)->AsWidget());
			if (ArrangedWidget)
			{
				OutGeometry = ArrangedWidget->Geometry;
				return true;
			}
		}
	}

	return false;
}

bool SDMXPixelMappingDesignerView::GetGraphSpaceCursorPosition(FVector2D& OutGraphSpaceCursorPosition) const
{
	if (const FArrangedWidget* CachedPreviewSurface = CachedWidgetGeometry.Find(PreviewSizeConstraint.ToSharedRef()))
	{
		const FGeometry& RootGeometry = CachedPreviewSurface->Geometry;
		OutGraphSpaceCursorPosition = RootGeometry.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());

		return true;
	}
	return false;
}

void SDMXPixelMappingDesignerView::GetPreviewAreaAndSize(FVector2D& Area, FVector2D& Size) const
{
	check(SourceTextureViewport.IsValid());

	Area = FVector2D(SourceTextureViewport->GetPreviewAreaWidth().Get(), SourceTextureViewport->GetPreviewAreaHeight().Get());
	Size = Area;
}

FGeometry SDMXPixelMappingDesignerView::GetDesignerGeometry() const
{
	return PreviewHitTestRoot->GetTickSpaceGeometry();
}

FGeometry SDMXPixelMappingDesignerView::MakeGeometryWindowLocal(const FGeometry& WidgetGeometry) const
{
	FGeometry NewGeometry = WidgetGeometry;

	TSharedPtr<SWindow> WidgetWindow = FSlateApplication::Get().FindWidgetWindow(SharedThis(this));
	if (WidgetWindow.IsValid())
	{
		TSharedRef<SWindow> CurrentWindowRef = WidgetWindow.ToSharedRef();

		NewGeometry.AppendTransform(FSlateLayoutTransform(Inverse(CurrentWindowRef->GetPositionInScreen())));
	}

	return NewGeometry;
}

void SDMXPixelMappingDesignerView::HandleDragEnterFromDetailsOrPalette(const TSharedPtr<FDMXPixelMappingDragDropOp>& TemplateDragDropOp)
{
	if (TSharedPtr<FDMXPixelMappingToolkit> ToolkitPtr = ToolkitWeakPtr.Pin())
	{
		if (TemplateDragDropOp.IsValid())
		{
			if (UDMXPixelMapping* PixelMapping = ToolkitPtr->GetDMXPixelMapping())
			{
				// Find the target on which the component should be created from its template
				UDMXPixelMappingBaseComponent* Target = (TemplateDragDropOp->Parent.IsValid()) ? TemplateDragDropOp->Parent.Get() : ToolkitPtr->GetActiveRendererComponent();

				if (Target && PixelMapping->RootComponent)
				{
					TArray<UDMXPixelMappingBaseComponent*> NewComponents;
					if (TemplateDragDropOp->GetDraggedComponents().Num() == 0)
					{
						// Create new components if they were first dragged in
						NewComponents = ToolkitPtr->CreateComponentsFromTemplates(PixelMapping->GetRootComponent(), Target, TemplateDragDropOp->GetTemplates());
					}
					else
					{
						// Use the existing components if the components are reentering
						for (const TWeakObjectPtr<UDMXPixelMappingBaseComponent>& BaseComponent : TemplateDragDropOp->GetDraggedComponents())
						{
							if (BaseComponent.IsValid())
							{
								NewComponents.Add(BaseComponent.Get());
							}
						}
					}

					// Build an array of all new componets for dragging
					TArray<TWeakObjectPtr<UDMXPixelMappingBaseComponent>> DraggedComponents;
					for (UDMXPixelMappingBaseComponent* Component : NewComponents)
					{
						DraggedComponents.Add(Component);
					}

					TemplateDragDropOp->SetDraggedComponents(DraggedComponents);

					// Find if only matrix components and its childs are dragged
					TArray<UDMXPixelMappingBaseComponent*> NewMatrices;
					for (UDMXPixelMappingBaseComponent* NewComponent : NewComponents)
					{
						if (UDMXPixelMappingMatrixComponent* NewMatrix = Cast<UDMXPixelMappingMatrixComponent>(NewComponent))
						{
							NewMatrices.Add(NewMatrix);
						}
						else if (UDMXPixelMappingMatrixCellComponent* NewMatrixCell = Cast<UDMXPixelMappingMatrixCellComponent>(NewComponent))
						{
							continue;
						}
						else
						{
							NewMatrices.Reset();
							break;
						}
					}

					// Update selection depending on types
					TSet<FDMXPixelMappingComponentReference> NewSelection;
					if (NewMatrices.Num() > 0)
					{
						// When only matrices and its childs are dragged, just select the matrix
						for (UDMXPixelMappingBaseComponent* NewMatrix : NewMatrices)
						{
							NewSelection.Add(ToolkitPtr->GetReferenceFromComponent(NewMatrix));
						}
					}
					else
					{
						// Select all new components
						for (UDMXPixelMappingBaseComponent* NewComponent : NewComponents)
						{
							NewSelection.Add(ToolkitPtr->GetReferenceFromComponent(NewComponent));
						}
					}

					ToolkitPtr->SelectComponents(NewSelection);
				}
			}
		}
	}
}

SDMXPixelMappingDesignerView::FScopedRestoreSelection::FScopedRestoreSelection(TSharedRef<FDMXPixelMappingToolkit> ToolkitPtr, TSharedRef<SDMXPixelMappingDesignerView> DesignerView)
	: WeakToolkit(ToolkitPtr)
	, WeakDesignerView(DesignerView)
{
	if (TSharedPtr<FDMXPixelMappingToolkit> PinnedToolkit = WeakToolkit.Pin())
	{
		for (const FDMXPixelMappingComponentReference& ComponentReference : PinnedToolkit->GetSelectedComponents())
		{
			CachedSelectedComponents.Add(ComponentReference.GetComponent());
		}
	}
}

SDMXPixelMappingDesignerView::FScopedRestoreSelection::~FScopedRestoreSelection()
{
	if (TSharedPtr<FDMXPixelMappingToolkit> PinnedToolkit = WeakToolkit.Pin())
	{
		TArray<UDMXPixelMappingBaseComponent*> RemovedComponents;
		TSet<FDMXPixelMappingComponentReference> ValidComponents;

		if (UDMXPixelMapping* PixelMapping = PinnedToolkit->GetDMXPixelMapping())
		{
			TArray<UDMXPixelMappingBaseComponent*> ComponentsInPixelMapping;
			PixelMapping->GetAllComponentsOfClass<UDMXPixelMappingBaseComponent>(ComponentsInPixelMapping);

			for (UDMXPixelMappingBaseComponent* Component : ComponentsInPixelMapping)
			{
				const bool bComponentStillExists =
					CachedSelectedComponents.ContainsByPredicate([Component](const TWeakObjectPtr<UDMXPixelMappingBaseComponent>& WeakCachedComponent)
						{
							return
								WeakCachedComponent.IsValid() &&
								Component == WeakCachedComponent.Get();
						});

				if (bComponentStillExists)
				{
					ValidComponents.Add(PinnedToolkit->GetReferenceFromComponent(Component));
				}
				else
				{
					RemovedComponents.Add(Component);
				}
			}

			if (ValidComponents.Num() == 0)
			{
				// All were removed, select the the parent if possible or the renderer
				UDMXPixelMappingBaseComponent** ComponentWithParentPtr = RemovedComponents.FindByPredicate([&ComponentsInPixelMapping](UDMXPixelMappingBaseComponent* Component)
					{
						return
							Component &&
							Component->GetParent() &&
							ComponentsInPixelMapping.Contains(Component->GetParent());
					});

				if (ComponentWithParentPtr)
				{
					ValidComponents.Add(PinnedToolkit->GetReferenceFromComponent((*ComponentWithParentPtr)->GetParent()));
				}
				else
				{
					// Select the renderer
					UDMXPixelMappingRendererComponent* RendererComponent = PinnedToolkit->GetActiveRendererComponent();
					if (RendererComponent)
					{
						ValidComponents.Add(PinnedToolkit->GetReferenceFromComponent(RendererComponent));
					}
				}

				PinnedToolkit->SelectComponents(ValidComponents);
			}
		}
	}

	if (TSharedPtr<SDMXPixelMappingDesignerView> PinnedDesignerView = WeakDesignerView.Pin())
	{
		PinnedDesignerView->CreateExtensionWidgetsForSelection();
	}
}

#undef LOCTEXT_NAMESPACE
