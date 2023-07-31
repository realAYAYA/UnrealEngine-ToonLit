// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SDMXPixelMappingTransformHandle.h"

#include "DMXPixelMappingComponentReference.h"
#include "DMXPixelMappingComponentWidget.h"
#include "DMXPixelMappingLayoutSettings.h"
#include "Components/DMXPixelMappingOutputComponent.h"
#include "Components/DMXPixelMappingMatrixCellComponent.h"
#include "Toolkits/DMXPixelMappingToolkit.h"
#include "Views/SDMXPixelMappingDesignerView.h"

#include "Widgets/Images/SImage.h"
#include "Styling/AppStyle.h"
#include "ScopedTransaction.h"


#define LOCTEXT_NAMESPACE "SDMXPixelMappingTransformHandle"

void SDMXPixelMappingTransformHandle::Construct(const FArguments& InArgs, TSharedPtr<SDMXPixelMappingDesignerView> InDesignerView, EDMXPixelMappingTransformDirection InTransformDirection, TAttribute<FVector2D> InOffset)
{
	TransformDirection = InTransformDirection;
	DesignerViewWeakPtr = InDesignerView;
	Offset = InOffset;

	Action = EDMXPixelMappingTransformAction::None;
	ScopedTransaction = nullptr;

	DragDirection = ComputeDragDirection(InTransformDirection);
	DragOrigin = ComputeOrigin(InTransformDirection);

	ChildSlot
	[
		SNew(SImage)
		.Visibility(this, &SDMXPixelMappingTransformHandle::GetHandleVisibility)
		.Image(FAppStyle::Get().GetBrush("UMGEditor.TransformHandle"))
	];
}

EVisibility SDMXPixelMappingTransformHandle::GetHandleVisibility() const
{
	return EVisibility::Visible;
}

FReply SDMXPixelMappingTransformHandle::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		Action = ComputeActionAtLocation(MyGeometry, MouseEvent);

		if (TSharedPtr<FDMXPixelMappingToolkit> Toolkit = DesignerViewWeakPtr.Pin()->GetToolkit())
		{
			const TSet<FDMXPixelMappingComponentReference> SelectedComponentReferences = Toolkit->GetSelectedComponents();
			for (const FDMXPixelMappingComponentReference& ComponentReference : SelectedComponentReferences)
			{
				UDMXPixelMappingBaseComponent* Preview = ComponentReference.GetComponent();
				UDMXPixelMappingBaseComponent* Component = ComponentReference.GetComponent();

				if (UDMXPixelMappingOutputComponent* OutputComponent = Cast<UDMXPixelMappingOutputComponent>(Preview))
				{
					FMargin Offsets;
					FVector2D Size = OutputComponent->GetSize();
					Offsets.Right = Size.X;
					Offsets.Bottom = Size.Y;
					StartingOffsets = Offsets;
				}

				MouseDownPosition = MouseEvent.GetScreenSpacePosition();

				ScopedTransaction = MakeShareable<FScopedTransaction>(new FScopedTransaction(LOCTEXT("ResizePixelMappingComponent", "PixelMapping: Resize Component")));
				Component->Modify();

				return FReply::Handled().CaptureMouse(SharedThis(this));
			}
		}
	}

	return FReply::Unhandled();
}

FReply SDMXPixelMappingTransformHandle::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( HasMouseCapture() && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		Action = EDMXPixelMappingTransformAction::None;
		
		if (TSharedPtr<FDMXPixelMappingToolkit> Toolkit = DesignerViewWeakPtr.Pin()->GetToolkit())
		{		
			const TSet<FDMXPixelMappingComponentReference> SelectedComponentReferences = Toolkit->GetSelectedComponents();
			for (const FDMXPixelMappingComponentReference& ComponentReference : SelectedComponentReferences)
			{
				if (UDMXPixelMappingOutputComponent* ResizedComponent = Cast<UDMXPixelMappingOutputComponent>(ComponentReference.GetComponent()))
				{
					// Set the final size transacted
					const FVector2D Delta = MouseEvent.GetScreenSpacePosition() - MouseDownPosition;
					const FVector2D TranslateAmount = Delta * (1.0f / (DesignerViewWeakPtr.Pin()->GetPreviewScale() * MyGeometry.Scale));

					ResizedComponent->Modify();
					Resize(ResizedComponent, DragDirection, TranslateAmount);

					ScopedTransaction.Reset();

					return FReply::Handled().ReleaseMouseCapture();
				}
			}
		}
	}

	return FReply::Unhandled();
}

FReply SDMXPixelMappingTransformHandle::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( Action != EDMXPixelMappingTransformAction::None )
	{
		if (TSharedPtr<FDMXPixelMappingToolkit> Toolkit = DesignerViewWeakPtr.Pin()->GetToolkit())
		{
			const TSet<FDMXPixelMappingComponentReference> SelectedComponentReferences = Toolkit->GetSelectedComponents();
			for (const FDMXPixelMappingComponentReference& ComponentReference : SelectedComponentReferences)
			{
				if (UDMXPixelMappingOutputComponent* ResizedComponent = Cast<UDMXPixelMappingOutputComponent>(ComponentReference.GetComponent()))
				{
					const FVector2D Delta = MouseEvent.GetScreenSpacePosition() - MouseDownPosition;
					const FVector2D TranslateAmount = Delta * (1.0f / (DesignerViewWeakPtr.Pin()->GetPreviewScale() * MyGeometry.Scale));

					Resize(ResizedComponent, DragDirection, TranslateAmount);
				}
			}
		}
	}

	return FReply::Unhandled();
}

void SDMXPixelMappingTransformHandle::Resize(UDMXPixelMappingBaseComponent* BaseComponent, const FVector2D& Direction, const FVector2D& Amount)
{
	if (UDMXPixelMappingOutputComponent* OutputComponent = Cast<UDMXPixelMappingOutputComponent>(BaseComponent))
	{
		FVector2D ComponentSize = OutputComponent->GetSize();

		FMargin Offsets = StartingOffsets;

		const FVector2D Movement = Amount * Direction;
		const FVector2D PositionMovement = Movement * (FVector2D(1.0f, 1.0f));
		const FVector2D SizeMovement = Movement;

		if (Direction.X < 0)
		{
			Offsets.Left -= PositionMovement.X;
			Offsets.Right += SizeMovement.X;
		}

		if (Direction.Y < 0)
		{
			Offsets.Top -= PositionMovement.Y;
			Offsets.Bottom += SizeMovement.Y;
		}

		if (Direction.X > 0)
		{
			Offsets.Left += Movement.X;
			Offsets.Right += Amount.X * Direction.X;
		}

		if (Direction.Y > 0)
		{
			Offsets.Top += Movement.Y;
			Offsets.Bottom += Amount.Y * Direction.Y;
		}

		const FVector2D OldSize = OutputComponent->GetSize();
		const FVector2D NewSize = FVector2D(Offsets.Right, Offsets.Bottom);
		if (OldSize == NewSize)
		{
			// No unchanged values
			return;
		}

		OutputComponent->SetSize(NewSize);

		// Scale children if desired, no division by zero
		const UDMXPixelMappingLayoutSettings* LayoutSettings = GetDefault<UDMXPixelMappingLayoutSettings>();
		if (!LayoutSettings ||
			!LayoutSettings->bScaleChildrenWithParent ||
			NewSize == FVector2D::ZeroVector)
		{
			return;
		}

		const FVector2D RatioVector = NewSize / OldSize;
		for (UDMXPixelMappingBaseComponent* BaseChild : OutputComponent->GetChildren())
		{
			if (UDMXPixelMappingOutputComponent* Child = Cast<UDMXPixelMappingOutputComponent>(BaseChild))
			{
				if (BaseChild->GetClass() == UDMXPixelMappingMatrixCellComponent::StaticClass())
				{
					// Don't scale matrix cells, the matrix component already cares for this
					if (Child->IsLockInDesigner())
					{
						continue;
					}
				}

				Child->Modify();

				// Scale size (Note, SetSize already clamps)
				Child->SetSize(Child->GetSize() * RatioVector);

				// Scale position
				const FVector2D ChildPosition = Child->GetPosition();
				const FVector2D NewPositionRelative = (ChildPosition - OutputComponent->GetPosition()) * RatioVector;
				Child->SetPosition(OutputComponent->GetPosition() + NewPositionRelative);
			}
		}
	}
}

FCursorReply SDMXPixelMappingTransformHandle::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const
{
	EDMXPixelMappingTransformAction CurrentAction = Action;
	if ( CurrentAction == EDMXPixelMappingTransformAction::None )
	{
		CurrentAction = ComputeActionAtLocation(MyGeometry, MouseEvent);
	}

	switch ( TransformDirection )
	{
		case EDMXPixelMappingTransformDirection::BottomRight:
			return FCursorReply::Cursor(EMouseCursor::ResizeSouthEast);
		case EDMXPixelMappingTransformDirection::BottomLeft:
			return FCursorReply::Cursor(EMouseCursor::ResizeSouthWest);
		case EDMXPixelMappingTransformDirection::BottomCenter:
			return FCursorReply::Cursor(EMouseCursor::ResizeUpDown);
		case EDMXPixelMappingTransformDirection::CenterRight:
			return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
	}

	return FCursorReply::Unhandled();
}

FVector2D SDMXPixelMappingTransformHandle::ComputeDragDirection(EDMXPixelMappingTransformDirection InTransformDirection) const
{
	switch ( InTransformDirection )
	{
	case EDMXPixelMappingTransformDirection::CenterRight:
		return FVector2D(1, 0);

	case EDMXPixelMappingTransformDirection::BottomLeft:
		return FVector2D(-1, 1);
	case EDMXPixelMappingTransformDirection::BottomCenter:
		return FVector2D(0, 1);
	case EDMXPixelMappingTransformDirection::BottomRight:
		return FVector2D(1, 1);
	}

	return FVector2D(0, 0);
}

FVector2D SDMXPixelMappingTransformHandle::ComputeOrigin(EDMXPixelMappingTransformDirection InTransformDirection) const
{
	FVector2D Size(10, 10);

	switch ( InTransformDirection )
	{
	case EDMXPixelMappingTransformDirection::CenterRight:
		return Size * FVector2D(0, 0.5);

	case EDMXPixelMappingTransformDirection::BottomLeft:
		return Size * FVector2D(1, 0);
	case EDMXPixelMappingTransformDirection::BottomCenter:
		return Size * FVector2D(0.5, 0);
	case EDMXPixelMappingTransformDirection::BottomRight:
		return Size * FVector2D(0, 0);
	}

	return FVector2D(0, 0);
}

EDMXPixelMappingTransformAction SDMXPixelMappingTransformHandle::ComputeActionAtLocation(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const
{
	FVector2D LocalPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	FVector2D GrabOriginOffset = LocalPosition - DragOrigin;
	if ( GrabOriginOffset.SizeSquared() < 36.f )
	{
		return EDMXPixelMappingTransformAction::Primary;
	}
	else
	{
		return EDMXPixelMappingTransformAction::Secondary;
	}
}

#undef LOCTEXT_NAMESPACE
