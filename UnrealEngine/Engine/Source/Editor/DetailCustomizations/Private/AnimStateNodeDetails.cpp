// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimStateNodeDetails.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Fonts/SlateFontInfo.h"
#include "HAL/Platform.h"
#include "Internationalization/Internationalization.h"
#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SToolTip.h"

class IDetailCustomization;

#define LOCTEXT_NAMESPACE "FAnimStateNodeDetails"

/////////////////////////////////////////////////////////////////////////


TSharedRef<IDetailCustomization> FAnimStateNodeDetails::MakeInstance()
{
	return MakeShareable( new FAnimStateNodeDetails );
}

void FAnimStateNodeDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	IDetailCategoryBuilder& SegmentCategory = DetailBuilder.EditCategory("Animation State", LOCTEXT("AnimationStateCategoryTitle", "Animation State") );
	
	GenerateAnimationStateEventRow(SegmentCategory, LOCTEXT("EnteredAnimationStateEventLabel", "Entered State Event"), TEXT("StateEntered"));
	GenerateAnimationStateEventRow(SegmentCategory, LOCTEXT("ExitedAnimationStateEventLabel", "Left State Event"), TEXT("StateLeft"));
	GenerateAnimationStateEventRow(SegmentCategory, LOCTEXT("FullyBlendedAnimationStateEventLabel", "Fully Blended State Event"), TEXT("StateFullyBlended"));

	DetailBuilder.HideProperty("StateEntered");
	DetailBuilder.HideProperty("StateLeft");
	DetailBuilder.HideProperty("StateFullyBlended");
}

void FAnimStateNodeDetails::GenerateAnimationStateEventRow(IDetailCategoryBuilder& SegmentCategory,const FText& StateEventLabel, const FString& TransitionName)
{
	const FSlateBrush* WarningIcon = FAppStyle::GetBrush("Icons.WarningWithColor");

	SegmentCategory.AddCustomRow( StateEventLabel)
		[
			SNew(SHorizontalBox)
			.ToolTipText(LOCTEXT("AnimStateEventDeprecatedTooltip", "This is no longer recommended. The recommended approach is to use the anim node function versions."))
			.Visibility(EVisibility::Visible)
			
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew( STextBlock )
				.Text( StateEventLabel )
				.Font( IDetailLayoutBuilder::GetDetailFontBold() )
			]

			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.HAlign(HAlign_Right)
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[	SNew(SImage)
				.Image(WarningIcon)
			]
		];
		
	CreateTransitionEventPropertyWidgets(SegmentCategory, TransitionName);
}

#undef LOCTEXT_NAMESPACE
