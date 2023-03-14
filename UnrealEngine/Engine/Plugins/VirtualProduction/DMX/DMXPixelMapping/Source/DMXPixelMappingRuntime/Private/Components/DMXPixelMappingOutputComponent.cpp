// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DMXPixelMappingOutputComponent.h"

#include "DMXPixelMappingRuntimeObjectVersion.h"
#include "Components/DMXPixelMappingRendererComponent.h"
#include "Components/DMXPixelMappingRootComponent.h"
#include "Library/DMXEntityFixturePatch.h"
#include "UObject/UObjectGlobals.h"

#if WITH_EDITOR
#include "DMXPixelMappingComponentWidget.h"
#endif // WITH_EDITOR


#define LOCTEXT_NAMESPACE "DMXPixelMappingOutputComponent"


UDMXPixelMappingOutputComponent::UDMXPixelMappingOutputComponent()
	: CellBlendingQuality(EDMXPixelBlendingQuality::Low)
	, PositionX(0.f)
	, PositionY(0.f)
	, SizeX(1.f)
	, SizeY(1.f)
{
#if WITH_EDITOR
	bLockInDesigner = false;
	bVisibleInDesigner = true;
	ZOrder = 0;
#endif // WITH_EDITOR
}

#if WITH_EDITOR
void UDMXPixelMappingOutputComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Call the parent at the first place
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	// Handle deprecated component widget changes
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UDMXPixelMappingOutputComponent, bVisibleInDesigner))
	{
		const EVisibility NewVisiblity = IsVisible() ? EVisibility::HitTestInvisible : EVisibility::Hidden;

		if (ComponentWidget_DEPRECATED.IsValid())
		{
			ComponentWidget_DEPRECATED->SetVisibility(NewVisiblity);
		}

		constexpr bool bSetVisibilityRecursive = true;
		ForEachChildOfClass<UDMXPixelMappingOutputComponent>([NewVisiblity](UDMXPixelMappingOutputComponent* ChildComponent)
			{
				if (TSharedPtr<FDMXPixelMappingComponentWidget> ChildComponentWidget = ChildComponent->GetComponentWidget())
				{
					ChildComponentWidget->SetVisibility(NewVisiblity);
				}
			}, bSetVisibilityRecursive);
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Apply cell belending quality
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UDMXPixelMappingOutputComponent, CellBlendingQuality))
	{
		// Propagonate to children
		constexpr bool bSetCellBlendingQualityToChildsRecursive = true;
		ForEachChildOfClass<UDMXPixelMappingOutputComponent>([this](UDMXPixelMappingOutputComponent* ChildComponent)
			{
				ChildComponent->CellBlendingQuality = CellBlendingQuality;
			}, bSetCellBlendingQualityToChildsRecursive);
	}
}
#endif // WITH_EDITOR

#if WITH_EDITOR
void UDMXPixelMappingOutputComponent::PreEditUndo()
{
	Super::PreEditUndo();

	PreEditUndoChildren = Children;
}
#endif // WITH_EDITOR

#if WITH_EDITOR
void UDMXPixelMappingOutputComponent::PostEditUndo()
{
	Super::PostEditUndo();

	EVisibility NewVisibility = IsVisible() ? EVisibility::HitTestInvisible : EVisibility::Hidden;

	UpdateComponentWidget(NewVisibility);

	if (Children.Num() < PreEditUndoChildren.Num())
	{	
		// Undo Add Children (remove them again)
		for (UDMXPixelMappingBaseComponent* PreEditUndoChild : PreEditUndoChildren)
		{
			if (UDMXPixelMappingOutputComponent* PreEditUndoOutputComponent = Cast<UDMXPixelMappingOutputComponent>(PreEditUndoChild))
			{
				const EVisibility NewChildVisibility = PreEditUndoOutputComponent->IsVisible() ? EVisibility::HitTestInvisible : EVisibility::Hidden;

				constexpr bool bWithChildrenRecursive = false;
				PreEditUndoOutputComponent->UpdateComponentWidget(NewChildVisibility, bWithChildrenRecursive);
			}	
		}
	}
	else if (Children.Num() > PreEditUndoChildren.Num())
	{
		// Undo Remove Children (add them again)
		for (UDMXPixelMappingBaseComponent* PreEditUndoChild : PreEditUndoChildren)
		{
			if (UDMXPixelMappingOutputComponent* PreEditUndoOutputComponent = Cast<UDMXPixelMappingOutputComponent>(PreEditUndoChild))
			{
				const EVisibility NewChildVisibility = IsVisible() ? EVisibility::HitTestInvisible : EVisibility::Hidden;

				constexpr bool bWithChildrenRecursive = false;
				PreEditUndoOutputComponent->UpdateComponentWidget(NewChildVisibility, bWithChildrenRecursive);
			}
		}
	}

	PreEditUndoChildren.Reset();
}
#endif // WITH_EDITOR

void UDMXPixelMappingOutputComponent::BeginDestroy()
{
	Super::BeginDestroy();

#if WITH_EDITOR
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (ComponentWidget_DEPRECATED.IsValid())
	{
		ComponentWidget_DEPRECATED->RemoveFromCanvas();

		// Should have released all references by now
		ensureMsgf(ComponentWidget_DEPRECATED.GetSharedReferenceCount() == 1, TEXT("Detected Reference to Component Widget the component is destroyed."));
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
}

bool UDMXPixelMappingOutputComponent::CanBeMovedTo(const UDMXPixelMappingBaseComponent* Component) const
{
	return false;
}

void UDMXPixelMappingOutputComponent::NotifyAddedToParent()
{
	Super::NotifyAddedToParent();

#if WITH_EDITOR
	UpdateComponentWidget(EVisibility::HitTestInvisible);
#endif // WITH_EDITOR
}

void UDMXPixelMappingOutputComponent::NotifyRemovedFromParent()
{
	Super::NotifyRemovedFromParent();

#if WITH_EDITOR
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (ComponentWidget_DEPRECATED.IsValid())
	{
		ComponentWidget_DEPRECATED->RemoveFromCanvas();

		// Should have released all references by now
		ensureMsgf(ComponentWidget_DEPRECATED.GetSharedReferenceCount() == 1, TEXT("Detected Reference to Component Widget the component is destroyed."));

		ComponentWidget_DEPRECATED.Reset();
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITOR
}

#if WITH_EDITOR
const FText UDMXPixelMappingOutputComponent::GetPaletteCategory()
{
	ensureMsgf(false, TEXT("You must implement GetPaletteCategory() in your child class"));

	return LOCTEXT("Uncategorized", "Uncategorized");
}
#endif // WITH_EDITOR

#if WITH_EDITOR
PRAGMA_DISABLE_DEPRECATION_WARNINGS
TSharedRef<FDMXPixelMappingComponentWidget> UDMXPixelMappingOutputComponent::BuildSlot(TSharedRef<SConstraintCanvas> InCanvas)
{
	ComponentWidget_DEPRECATED = MakeShared<FDMXPixelMappingComponentWidget>();
	ComponentWidget_DEPRECATED->AddToCanvas(InCanvas, ZOrder);

	EVisibility NewVisibility = IsVisible() ? EVisibility::HitTestInvisible : EVisibility::Hidden;

	UpdateComponentWidget(NewVisibility);

	return ComponentWidget_DEPRECATED.ToSharedRef();
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITOR

#if WITH_EDITOR
bool UDMXPixelMappingOutputComponent::IsVisible() const
{
	const bool bAssignedToParent = GetParent() && GetParent()->Children.Contains(this);

	return bVisibleInDesigner && bAssignedToParent;
}
#endif // WITH_EDITOR

#if WITH_EDITOR
void UDMXPixelMappingOutputComponent::SetZOrder(int32 NewZOrder)
{
	const int32 DeltaZOrder = NewZOrder - ZOrder;
	
	ZOrder = NewZOrder;


PRAGMA_DISABLE_DEPRECATION_WARNINGS
	// Handle deprecated component widget changes
	if (ComponentWidget_DEPRECATED.IsValid())
	{
		ComponentWidget_DEPRECATED->SetZOrder(ZOrder);
	}

	constexpr bool bRecursive = true;
	ForEachChild([DeltaZOrder, this](UDMXPixelMappingBaseComponent* ChildComponent)
		{
			if (UDMXPixelMappingOutputComponent* ChildOutputComponent = Cast<UDMXPixelMappingOutputComponent>(ChildComponent))
			{
				const int32 NewChildZOrder = ChildOutputComponent->GetZOrder() + DeltaZOrder;
				if (TSharedPtr<FDMXPixelMappingComponentWidget> ChildComponentWidget_DEPRECATED = ChildOutputComponent->GetComponentWidget())
				{
					ChildOutputComponent->SetZOrder(NewChildZOrder);

					// Apply to the UI
					if (ChildOutputComponent->ComponentWidget_DEPRECATED.IsValid())
					{
						ChildOutputComponent->ComponentWidget_DEPRECATED->SetZOrder(ZOrder);
					}
				}
			}
		}, bRecursive);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
#endif // WITH_EDITOR

bool UDMXPixelMappingOutputComponent::IsOverParent() const
{
	// By default all components are over their parent.
	// E.g. Renderer is always over the root, group is always over the renderer.
	return true;
}

bool UDMXPixelMappingOutputComponent::IsOverPosition(const FVector2D& OtherPosition) const
{
	return
		PositionX <= OtherPosition.X &&
		PositionY <= OtherPosition.Y &&
		PositionX + SizeX >= OtherPosition.X &&
		PositionY + SizeY >= OtherPosition.Y;
}

bool UDMXPixelMappingOutputComponent::OverlapsComponent(UDMXPixelMappingOutputComponent* Other) const
{
	if (Other)
	{
		FVector2D ThisPosition = GetPosition();
		FVector2D OtherPosition = Other->GetPosition();

		FBox2D ThisBox = FBox2D(ThisPosition, ThisPosition + GetSize());
		FBox2D OtherBox = FBox2D(OtherPosition, OtherPosition + Other->GetSize());

		return ThisBox.Intersect(OtherBox);
	}

	return false;
}

void UDMXPixelMappingOutputComponent::SetPosition(const FVector2D& Position) 
{
	if (Position.X != PositionX || Position.Y != PositionY)
	{
		PositionX = Position.X;
		PositionY = Position.Y;
	}
}

void UDMXPixelMappingOutputComponent::SetSize(const FVector2D& Size) 
{
	SizeX = Size.X;
	SizeY = Size.Y;

	SizeX = FMath::Max(SizeX, 1.f);
	SizeY = FMath::Max(SizeY, 1.f);
}

UDMXPixelMappingRendererComponent* UDMXPixelMappingOutputComponent::FindRendererComponent() const
{
	UDMXPixelMappingRendererComponent* ParentRendererComponent = nullptr;

	if (GetParent())
	{
		for (UDMXPixelMappingBaseComponent* ParentComponent = GetParent(); ParentComponent; ParentComponent = ParentComponent->GetParent())
		{
			ParentRendererComponent = Cast<UDMXPixelMappingRendererComponent>(ParentComponent);
			if (ParentRendererComponent)
			{
				break;
			}
		}
	}

	return ParentRendererComponent;
}

#if WITH_EDITOR
void UDMXPixelMappingOutputComponent::MakeHighestZOrderInComponentRect()
{
	if (UDMXPixelMappingRendererComponent* RendererComponent = FindRendererComponent())
	{
		constexpr bool bRecursive = true;
		RendererComponent->ForEachChildOfClass<UDMXPixelMappingOutputComponent>([this](UDMXPixelMappingOutputComponent* InComponent)
			{
				if (InComponent == this)
				{
					return;
				}

				// Exclude children, they're updated when SetZOrder is called below
				for (UDMXPixelMappingBaseComponent* OtherParent = InComponent->GetParent(); OtherParent; OtherParent = OtherParent->GetParent())
				{
					if (OtherParent == this)
					{
						return;
					}
				}

				if (this->OverlapsComponent(InComponent))
				{
					if (InComponent->ZOrder + 1 > ZOrder)
					{
						SetZOrder(InComponent->ZOrder + 1);
					}
				}
			}, bRecursive);
	}
}
#endif // WITH_EDITOR

#if WITH_EDITOR
void UDMXPixelMappingOutputComponent::UpdateComponentWidget(EVisibility NewVisibility, bool bWithChildrenRecursive)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (ComponentWidget_DEPRECATED.IsValid())
	{
		ComponentWidget_DEPRECATED->SetPosition(FVector2D(PositionX, PositionY));
		ComponentWidget_DEPRECATED->SetSize(FVector2D(SizeX, SizeY));
		ComponentWidget_DEPRECATED->SetVisibility(NewVisibility);
		ComponentWidget_DEPRECATED->SetColor(GetEditorColor());
		ComponentWidget_DEPRECATED->SetLabelText(FText::FromString(GetUserFriendlyName()));
	}

	if (bWithChildrenRecursive)
	{
		for (UDMXPixelMappingBaseComponent* ChildComponent : Children)
		{
			if (UDMXPixelMappingOutputComponent* ChildOutputComponent = Cast<UDMXPixelMappingOutputComponent>(ChildComponent))
			{
				// Recursive for all
				ChildOutputComponent->UpdateComponentWidget(NewVisibility, bWithChildrenRecursive);
			}
		}
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
