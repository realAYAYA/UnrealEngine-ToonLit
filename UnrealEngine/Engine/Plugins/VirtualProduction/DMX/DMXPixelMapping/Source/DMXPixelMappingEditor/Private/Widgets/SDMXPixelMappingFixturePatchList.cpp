// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXPixelMappingFixturePatchList.h"

#include "Algo/MaxElement.h"
#include "DMXPixelMapping.h"
#include "DragDrop/DMXPixelMappingDragDropOp.h"
#include "Components/DMXPixelMappingBaseComponent.h"
#include "Components/DMXPixelMappingFixtureGroupItemComponent.h"
#include "Components/DMXPixelMappingMatrixComponent.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXLibrary.h"
#include "Templates/DMXPixelMappingComponentTemplate.h"
#include "Toolkits/DMXPixelMappingToolkit.h"
#include "ViewModels/DMXPixelMappingDMXLibraryViewModel.h"


void SDMXPixelMappingFixturePatchList::Construct(const FArguments& InArgs, const TSharedPtr<FDMXPixelMappingToolkit>& InToolkit, TWeakObjectPtr<UDMXPixelMappingDMXLibraryViewModel> InDMXLibraryModel)
{
	if (!InToolkit.IsValid() || !InDMXLibraryModel.IsValid())
	{
		return;
	}
	WeakToolkit = InToolkit;
	WeakDMXLibraryViewModel = InDMXLibraryModel;

	SDMXReadOnlyFixturePatchList::Construct(
		SDMXReadOnlyFixturePatchList::FArguments()
		.ListDescriptor(InArgs._ListDescriptor)
		.OnContextMenuOpening(InArgs._OnContextMenuOpening)
		.OnRowDragDetected(this, &SDMXPixelMappingFixturePatchList::OnRowDragDetected)
	);
}

FReply SDMXPixelMappingFixturePatchList::OnRowDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const TSharedPtr<FDMXPixelMappingToolkit> Toolkit = WeakToolkit.Pin();
	UDMXPixelMappingFixtureGroupComponent* FixtureGroup = WeakDMXLibraryViewModel.IsValid() ? WeakDMXLibraryViewModel->GetFixtureGroupComponent() : nullptr;
	if (!Toolkit.IsValid() || !FixtureGroup)
	{
		return FReply::Unhandled();
	}

	TArray<TSharedPtr<FDMXPixelMappingComponentTemplate>> Templates;
	for (const TSharedPtr<FDMXEntityFixturePatchRef>& FixturePatchRef : GetSelectedFixturePatchRefs())
	{
		UDMXEntityFixturePatch* FixturePatch = FixturePatchRef.IsValid() ? FixturePatchRef->GetFixturePatch() : nullptr;
		UDMXEntityFixtureType* FixtureType = FixturePatch ? FixturePatch->GetFixtureType() : nullptr;
		if (!FixturePatch || !FixtureType)
		{
			continue;
		}

		const FDMXEntityFixturePatchRef FixturePatchReference(FixturePatch);
		if (FixtureType->bFixtureMatrixEnabled)
		{
			TSharedRef<FDMXPixelMappingComponentTemplate> FixturePatchMatrixTemplate = MakeShared<FDMXPixelMappingComponentTemplate>(UDMXPixelMappingMatrixComponent::StaticClass(), FixturePatchReference);
			Templates.Add(FixturePatchMatrixTemplate);
		}
		else
		{
			TSharedRef<FDMXPixelMappingComponentTemplate> FixturePatchItemTemplate = MakeShared<FDMXPixelMappingComponentTemplate>(UDMXPixelMappingFixtureGroupItemComponent::StaticClass(), FixturePatchReference);
			Templates.Add(FixturePatchItemTemplate);
		}
	}

	if (Templates.IsEmpty())
	{
		return FReply::Handled();
	}
	else
	{
		return FReply::Handled().BeginDragDrop(FDMXPixelMappingDragDropOp::New(FVector2D::ZeroVector, Templates, FixtureGroup));
	}
}

void SDMXPixelMappingFixturePatchList::SelectAfter(const TArray<TSharedPtr<FDMXEntityFixturePatchRef>>& FixturePatches)
{
	const TSharedPtr<FDMXEntityFixturePatchRef>* MaxFixturePatchPtr = Algo::MaxElementBy(FixturePatches, [](const TSharedPtr<FDMXEntityFixturePatchRef>& FixturePatchRef)
		{
			const UDMXEntityFixturePatch* FixturePatch = FixturePatchRef.IsValid() ? FixturePatchRef->GetFixturePatch() : nullptr;
			return FixturePatch ? FixturePatch->GetUniverseID() * DMX_UNIVERSE_SIZE + FixturePatch->GetStartingChannel() : -1;
		});

	const TArray<TSharedPtr<FDMXEntityFixturePatchRef>> Items = GetListItems();
	if (MaxFixturePatchPtr)
	{
		int32 IndexOfPatch = Items.IndexOfByPredicate([MaxFixturePatchPtr](const TSharedPtr<FDMXEntityFixturePatchRef>& Item)
			{			
				const UDMXEntityFixturePatch* FixturePatch = (*MaxFixturePatchPtr).IsValid() ? (*MaxFixturePatchPtr)->GetFixturePatch() : nullptr;
				return Item->GetFixturePatch() == FixturePatch;
			});

		if (Items.IsValidIndex(IndexOfPatch + 1))
		{
			const TArray<TSharedPtr<FDMXEntityFixturePatchRef>> NewSelection{ Items[IndexOfPatch + 1] };
			SelectItems(NewSelection);
		}
		else if (Items.IsValidIndex(IndexOfPatch - 1))
		{
			const TArray<TSharedPtr<FDMXEntityFixturePatchRef>> NewSelection{ Items[IndexOfPatch - 1] };
			SelectItems(NewSelection);
		}
	}
}

void SDMXPixelMappingFixturePatchList::RefreshList() 
{
	SDMXReadOnlyFixturePatchList::RefreshList();

	// Always make a selection if possible
	if (GetSelectedFixturePatchRefs().IsEmpty() && !ListItems.IsEmpty())
	{
		SelectItems(TArray<TSharedPtr<FDMXEntityFixturePatchRef>>({ ListItems[0] }));
	}
}
