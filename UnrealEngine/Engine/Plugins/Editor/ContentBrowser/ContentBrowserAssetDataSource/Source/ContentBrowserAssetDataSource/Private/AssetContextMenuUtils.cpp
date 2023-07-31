// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetContextMenuUtils.h"

#include "Styling/CoreStyle.h"
#include "Styling/ISlateStyle.h"
#include "Styling/ToolBarStyle.h"
#include "ToolMenuDelegates.h"
#include "ToolMenuEntry.h"
#include "ToolMenuSection.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace UE::ContentBrowserAssetDataSource::Private
{
	FNewToolMenuCustomWidget MakeCustomWidgetDelegate(const TAttribute<FText>& Label, const FIsAsyncProcessingActive& IsAsyncProcessingActive)
	{
		return FNewToolMenuCustomWidget::CreateLambda([Label, IsAsyncProcessingActive](const FToolMenuContext& InContext, const FToolMenuCustomWidgetContext& WidgetContext)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(FMargin(2, 0, 0, 0))
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(Label)
					.TextStyle(WidgetContext.StyleSet, ISlateStyle::Join(WidgetContext.StyleName, ".Label"))
					// Should Figure out a way to highlight the searched text
				]
				+ SHorizontalBox::Slot()
				.Padding(FMargin(8, 0, 0, 0))
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SThrobber)
					.PieceImage(WidgetContext.StyleSet->GetBrush("Throbber.CircleChunk"))
					.Visibility_Lambda([IsAsyncProcessingActive]()
					{
						if (IsAsyncProcessingActive.Execute())
						{
							return EVisibility::Visible;
						}
						return EVisibility::Collapsed;
					})
				];
		});
	};


	FToolMenuEntry& AddAsyncMenuEntry(
		FToolMenuSection& Section,
		FName Name,
		const TAttribute<FText>& Label,
		const TAttribute<FText>& ToolTip,
		const FToolUIActionChoice& InAction,
		const FIsAsyncProcessingActive& IsAsyncProcessingActive
	)
	{
		// Should we expose this function to the rest of the engine?
		// Before that it should handle properly the search

		FToolMenuEntry MenuEntry = FToolMenuEntry::InitMenuEntry(
			Name,
			Label,
			ToolTip,
			FSlateIcon(),
			InAction
		);

		if (IsAsyncProcessingActive.IsBound())
		{
			MenuEntry.MakeCustomWidget = MakeCustomWidgetDelegate(Label, IsAsyncProcessingActive);
		}
		return Section.AddEntry(MenuEntry);
	}
}
