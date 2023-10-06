// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMVVMCachedViewBindingConversionFunction.h"

#include "Editor.h"
#include "MVVMBlueprintView.h"
#include "MVVMEditorSubsystem.h"
#include "Styling/MVVMEditorStyle.h"
#include "WidgetBlueprint.h"
#include "Widgets/SMVVMPropertyPath.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SCachedViewBindingConverisonFunction"

namespace UE::MVVM
{

void SCachedViewBindingConversionFunction::Construct(const FArguments& InArgs, const UWidgetBlueprint* InWidgetBlueprint)
{
	WidgetBlueprint = InWidgetBlueprint;
	OnGetConversionFunction = InArgs._OnGetConversionFunction;

	ChildSlot
	[
		SNew(SHorizontalBox)
		.ToolTipText_Lambda([this]() 
		{
			const UFunction* FoundFunction = OnGetConversionFunction.Execute();
			return FoundFunction != nullptr ? FoundFunction->GetToolTipText() : FText::GetEmpty();
		})
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(0, 0, 4.0f, 0)
		.AutoWidth()
		[
			SNew(SImage)
			.DesiredSizeOverride(FVector2D(16.0f, 16.0f))
			.Image(FAppStyle::Get().GetBrush("GraphEditor.Function_16x"))
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text_Lambda([this]() 
			{ 
				const UFunction* FoundFunction = OnGetConversionFunction.Execute();
				return FoundFunction != nullptr ? FoundFunction->GetDisplayNameText() : FText::GetEmpty();
			})
		]
	];
}


} // namespace UE::MVVM

#undef LOCTEXT_NAMESPACE
