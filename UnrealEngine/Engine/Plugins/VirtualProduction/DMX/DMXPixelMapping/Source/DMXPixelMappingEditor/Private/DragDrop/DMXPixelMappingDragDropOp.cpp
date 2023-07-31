// Copyright Epic Games, Inc. All Rights Reserved.

#include "DragDrop/DMXPixelMappingDragDropOp.h"

#include "DMXPixelMappingComponentWidget.h"
#include "DragDrop/DMXPixelMappingDragDropOp.h"
#include "DragDrop/DMXPixelMappingGroupChildDragDropHelper.h"
#include "Templates/DMXPixelMappingComponentTemplate.h"
#include "Components/DMXPixelMappingFixtureGroupItemComponent.h"
#include "Components/DMXPixelMappingMatrixComponent.h"
#include "Components/DMXPixelMappingMatrixCellComponent.h"

#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Layout/WidgetPath.h"
#include "Widgets/Notifications/SNotificationList.h"


#define LOCTEXT_NAMESPACE "FDMXPixelMappingDragDropOp"

FDMXPixelMappingDragDropOp::~FDMXPixelMappingDragDropOp()
{
	// Newly added components are highest Z-Order
	TArray<UDMXPixelMappingOutputComponent*> ChildrenNotOverTheirParent;
	for (const TWeakObjectPtr<UDMXPixelMappingBaseComponent>& DraggedComponent : DraggedComponents)
	{
		if (UDMXPixelMappingOutputComponent* DraggedOutputComponent = Cast<UDMXPixelMappingOutputComponent>(DraggedComponent.Get()))
		{
			DraggedOutputComponent->MakeHighestZOrderInComponentRect();
		}
	}

	// End the transaction in any case
	GEditor->EndTransaction();
}

TSharedRef<FDMXPixelMappingDragDropOp> FDMXPixelMappingDragDropOp::New(const FVector2D& InGraphSpaceDragOffset, const TArray<TSharedPtr<FDMXPixelMappingComponentTemplate>>& InTemplates, UDMXPixelMappingBaseComponent* InParent)
{
	TSharedRef<FDMXPixelMappingDragDropOp> Operation = MakeShared<FDMXPixelMappingDragDropOp>();

	Operation->Templates = InTemplates;
	Operation->bWasCreatedAsTemplate = true;
	Operation->Parent = InParent;
	Operation->GraphSpaceDragOffset = InGraphSpaceDragOffset;
		
	Operation->Construct();
	Operation->SetDecoratorVisibility(false);

	// Create a transaction for dragged templates
	Operation->TransactionIndex = GEditor->BeginTransaction(FText::Format(LOCTEXT("DragDropTemplateTransaction", "PixelMapping: Add {0}|plural(one=Component, other=Components)"), InTemplates.Num()));

	return Operation;
}

TSharedRef<FDMXPixelMappingDragDropOp> FDMXPixelMappingDragDropOp::New(const FVector2D& InGraphSpaceDragOffset, const TArray<TWeakObjectPtr<UDMXPixelMappingBaseComponent>>& InDraggedComponents)
{
	TSharedRef<FDMXPixelMappingDragDropOp> Operation = MakeShared<FDMXPixelMappingDragDropOp>();

	Operation->bWasCreatedAsTemplate = false;
	Operation->SetDraggedComponents(InDraggedComponents);
	Operation->GraphSpaceDragOffset = InGraphSpaceDragOffset;
	Operation->GroupChildDragDropHelper = FDMXPixelMappingGroupChildDragDropHelper::Create(Operation); // After setting dragged components

	Operation->Construct();
	Operation->SetDecoratorVisibility(false);

	// Create a transaction for dragged components
	Operation->TransactionIndex = GEditor->BeginTransaction(FText::Format(LOCTEXT("DragDropComponentTransaction", "PixelMapping: Drag {0}|plural(one=Component, other=Components)"), InDraggedComponents.Num()));

	return Operation;
}

void FDMXPixelMappingDragDropOp::SetDraggedComponents(const TArray<TWeakObjectPtr<UDMXPixelMappingBaseComponent>>& InDraggedComponents)
{
	DraggedComponents = InDraggedComponents;
	Templates.Reset();

	// Rebuild the group child drag drop helper in case that's what was set
	GroupChildDragDropHelper = FDMXPixelMappingGroupChildDragDropHelper::Create(AsShared());
}

void FDMXPixelMappingDragDropOp::LayoutOutputComponents(const FVector2D& GraphSpacePosition)
{
	if (DraggedComponents.Num() > 0)
	{
		if (UDMXPixelMappingOutputComponent* FirstComponent = Cast<UDMXPixelMappingOutputComponent>(DraggedComponents[0]))
		{
			const FVector2D Anchor = FirstComponent->GetPosition();

			// Move all to new position
			for (const TWeakObjectPtr<UDMXPixelMappingBaseComponent>& Component : DraggedComponents)
			{
				if (UDMXPixelMappingOutputComponent* OutputComponent = Cast<UDMXPixelMappingOutputComponent>(Component.Get()))
				{
					OutputComponent->Modify();

					constexpr bool bModifyChildrenRecursively = true;
					Component->ForEachChild([](UDMXPixelMappingBaseComponent* Component)
				{
							Component->Modify();
						}, bModifyChildrenRecursively);

					if (ensureMsgf(OutputComponent->GetClass() != UDMXPixelMappingMatrixComponent::StaticClass(),
						TEXT("Matrix Cells are not supported. Use the GroupChildDragDropHelper from this class instead")))
					{
						FVector2D AnchorOffset = Anchor - OutputComponent->GetPosition();

						const FVector2D NewPosition = GraphSpacePosition - AnchorOffset - GraphSpaceDragOffset;
						OutputComponent->SetPosition(NewPosition.RoundToVector());
					}
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
