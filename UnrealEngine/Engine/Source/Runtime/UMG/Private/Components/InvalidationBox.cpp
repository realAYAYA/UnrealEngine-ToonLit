// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/InvalidationBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SInvalidationPanel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InvalidationBox)

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UInvalidationBox

UInvalidationBox::UInvalidationBox(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCanCache = true;
	SetVisibilityInternal(ESlateVisibility::SelfHitTestInvisible);
}

void UInvalidationBox::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyInvalidationPanel.Reset();
}

TSharedRef<SWidget> UInvalidationBox::RebuildWidget()
{
	MyInvalidationPanel =
		SNew(SInvalidationPanel)
#if !UE_BUILD_SHIPPING
		.DebugName(GetPathName())
#endif
		;

	MyInvalidationPanel->SetCanCache(IsDesignTime() ? false : bCanCache);

	if ( GetChildrenCount() > 0 )
	{
		MyInvalidationPanel->SetContent(GetContentSlot()->Content ? GetContentSlot()->Content->TakeWidget() : SNullWidget::NullWidget);
	}
	
	return MyInvalidationPanel.ToSharedRef();
}

void UInvalidationBox::OnSlotAdded(UPanelSlot* InSlot)
{
	// Add the child to the live slot if it already exists
	if ( MyInvalidationPanel.IsValid() )
	{
		MyInvalidationPanel->SetContent(InSlot->Content ? InSlot->Content->TakeWidget() : SNullWidget::NullWidget);
	}
}

void UInvalidationBox::OnSlotRemoved(UPanelSlot* InSlot)
{
	// Remove the widget from the live slot if it exists.
	if ( MyInvalidationPanel.IsValid() )
	{
		MyInvalidationPanel->SetContent(SNullWidget::NullWidget);
	}
}

void UInvalidationBox::InvalidateCache()
{
}

bool UInvalidationBox::GetCanCache() const
{
	if ( MyInvalidationPanel.IsValid() )
	{
		return MyInvalidationPanel->GetCanCache();
	}

	return bCanCache;
}

void UInvalidationBox::SetCanCache(bool CanCache)
{
	bCanCache = CanCache;
	if ( MyInvalidationPanel.IsValid() )
	{
		return MyInvalidationPanel->SetCanCache(bCanCache);
	}
}

#if WITH_EDITOR

const FText UInvalidationBox::GetPaletteCategory()
{
	return LOCTEXT("Optimization", "Optimization");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE

