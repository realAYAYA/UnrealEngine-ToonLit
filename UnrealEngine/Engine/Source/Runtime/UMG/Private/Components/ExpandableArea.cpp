// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/ExpandableArea.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/UMGCoreStyle.h"

#include "Widgets/Layout/SExpandableArea.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ExpandableArea)

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UExpandableArea

static const FName HeaderName(TEXT("Header"));
static const FName BodyName(TEXT("Body"));

static FExpandableAreaStyle* DefaultExpandableAreaStyle = nullptr;
static FSlateBrush* DefaultExpandableAreaBorderBrush = nullptr;

#if WITH_EDITOR
static FExpandableAreaStyle* EditorExpandableAreaStyle = nullptr;
static FSlateBrush* EditorExpandableAreaBorderBrush = nullptr;
#endif 

UExpandableArea::UExpandableArea(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bIsExpanded(false)
{
	bIsVariable = true;

	if (DefaultExpandableAreaStyle == nullptr)
	{
		DefaultExpandableAreaStyle = new FExpandableAreaStyle(FUMGCoreStyle::Get().GetWidgetStyle<FExpandableAreaStyle>("ExpandableArea"));

		// Unlink UMG default colors.
		DefaultExpandableAreaStyle->UnlinkColors();
	}

	if (DefaultExpandableAreaBorderBrush == nullptr)
	{
		DefaultExpandableAreaBorderBrush = new FSlateBrush(*FUMGCoreStyle::Get().GetBrush("ExpandableArea.Border"));

		// Unlink UMG default colors.
		DefaultExpandableAreaBorderBrush->UnlinkColors();
	}

	Style = *DefaultExpandableAreaStyle;
	BorderBrush = *DefaultExpandableAreaBorderBrush;

#if WITH_EDITOR 
	if (EditorExpandableAreaStyle == nullptr)
	{
		EditorExpandableAreaStyle = new FExpandableAreaStyle(FCoreStyle::Get().GetWidgetStyle<FExpandableAreaStyle>("ExpandableArea"));

		// Unlink UMG Editor colors from the editor settings colors.
		EditorExpandableAreaStyle->UnlinkColors();
	}

	if (EditorExpandableAreaBorderBrush == nullptr)
	{
		EditorExpandableAreaBorderBrush = new FSlateBrush(*FCoreStyle::Get().GetBrush("ExpandableArea.Border"));

		// Unlink UMG Editor colors from the editor settings colors.
		EditorExpandableAreaBorderBrush->UnlinkColors();
	}

	if (IsEditorWidget())
	{
		Style = *EditorExpandableAreaStyle;
		BorderBrush = *EditorExpandableAreaBorderBrush;

		// The CDO isn't an editor widget and thus won't use the editor style, call post edit change to mark difference from CDO
		PostEditChange();
	}
#endif // WITH_EDITOR

	BorderColor = FLinearColor::White;
	AreaPadding = FMargin(1);
	HeaderPadding = FMargin(4.0f, 2.0f);
}

bool UExpandableArea::GetIsExpanded() const
{
	if ( MyExpandableArea.IsValid() )
	{
		return MyExpandableArea->IsExpanded();
	}

	return bIsExpanded;
}

void UExpandableArea::SetIsExpanded(bool IsExpanded)
{
	bIsExpanded = IsExpanded;
	if ( MyExpandableArea.IsValid() )
	{
		MyExpandableArea->SetExpanded(IsExpanded);
	}
}

void UExpandableArea::SetIsExpanded_Animated(bool IsExpanded)
{
	bIsExpanded = IsExpanded;
	if (MyExpandableArea.IsValid())
	{
		MyExpandableArea->SetExpanded_Animated(IsExpanded);
	}
}

void UExpandableArea::ReleaseSlateResources( bool bReleaseChildren )
{
	Super::ReleaseSlateResources(bReleaseChildren);
	ReleaseNamedSlotSlateResources(bReleaseChildren);

	MyExpandableArea.Reset();
}

void UExpandableArea::GetSlotNames(TArray<FName>& SlotNames) const
{
	SlotNames.Add(HeaderName);
	SlotNames.Add(BodyName);
}

UWidget* UExpandableArea::GetContentForSlot(FName SlotName) const
{
	if ( SlotName == HeaderName )
	{
		return HeaderContent;
	}
	else if ( SlotName == BodyName )
	{
		return BodyContent;
	}

	return nullptr;
}

void UExpandableArea::SetContentForSlot(FName SlotName, UWidget* Content)
{
	if ( SlotName == HeaderName )
	{
		if ( HeaderContent )
		{
			const bool bReleaseChildren = true;
			HeaderContent->ReleaseSlateResources(bReleaseChildren);
		}

		HeaderContent = Content;
	}
	else if ( SlotName == BodyName )
	{
		if ( BodyContent )
		{
			const bool bReleaseChildren = true;
			BodyContent->ReleaseSlateResources(bReleaseChildren);
		}

		BodyContent = Content;
	}
}

TSharedRef<SWidget> UExpandableArea::RebuildWidget()
{
	TSharedRef<SWidget> HeaderWidget = HeaderContent ? HeaderContent->TakeWidget() : SNullWidget::NullWidget;
	TSharedRef<SWidget> BodyWidget = BodyContent ? BodyContent->TakeWidget() : SNullWidget::NullWidget;

	MyExpandableArea = SNew(SExpandableArea)
		.Style(&Style)
		.BorderImage(&BorderBrush)
		.BorderBackgroundColor(BorderColor)
		.MaxHeight(MaxHeight)
		.Padding(AreaPadding)
		.HeaderPadding(HeaderPadding)
		.OnAreaExpansionChanged(BIND_UOBJECT_DELEGATE(FOnBooleanValueChanged, SlateExpansionChanged))
		.HeaderContent()
		[
			HeaderWidget
		]
		.BodyContent()
		[
			BodyWidget
		];

	return MyExpandableArea.ToSharedRef();
}

void UExpandableArea::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	MyExpandableArea->SetExpanded(bIsExpanded);
}

void UExpandableArea::SlateExpansionChanged(bool NewState)
{
	bIsExpanded = NewState;

	if ( OnExpansionChanged.IsBound() )
	{
		OnExpansionChanged.Broadcast(this, NewState);
	}
}

#if WITH_EDITOR

const FText UExpandableArea::GetPaletteCategory()
{
	return LOCTEXT("Misc", "Misc");
}

void UExpandableArea::OnDescendantSelectedByDesigner(UWidget* DescendantWidget)
{
	// Temporarily sets the active child to the selected child to make
	// dragging and dropping easier in the editor.
	UWidget* SelectedChild = UWidget::FindChildContainingDescendant(BodyContent, DescendantWidget);
	if ( SelectedChild )
	{
		MyExpandableArea->SetExpanded(true);
	}
}

void UExpandableArea::OnDescendantDeselectedByDesigner(UWidget* DescendantWidget)
{
	if ( MyExpandableArea.IsValid() )
	{
		MyExpandableArea->SetExpanded(bIsExpanded);
	}
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE

