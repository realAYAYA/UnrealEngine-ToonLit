// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMVVMPropertyPath.h"

#include "Blueprint/WidgetTree.h"
#include "Editor.h"
#include "MVVMBlueprintView.h"
#include "MVVMEditorSubsystem.h"
#include "Styling/MVVMEditorStyle.h"
#include "Types/MVVMBindingSource.h"
#include "WidgetBlueprint.h"
#include "Widgets/SMVVMFieldEntry.h"
#include "Widgets/SMVVMFieldIcon.h"
#include "Widgets/SMVVMSourceEntry.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SPropertyPath"

namespace UE::MVVM
{

void SPropertyPath::Construct(const FArguments& InArgs, const UWidgetBlueprint* InWidgetBlueprint)
{
	WidgetBlueprint = InWidgetBlueprint;
	TextStyle = InArgs._TextStyle;
	bShowContext = InArgs._ShowContext;
	bShowOnlyLastPath = InArgs._ShowOnlyLastPath;

	ChildSlot
	[
		SAssignNew(FieldBox, SHorizontalBox)
	];
	SetPropertyPath(InArgs._PropertyPath);
}


void SPropertyPath::SetPropertyPath(const FMVVMBlueprintPropertyPath& InPropertyPath)
{
	FieldBox->ClearChildren();

	bool bHasSource = InPropertyPath.IsFromWidget() || InPropertyPath.IsFromViewModel();
	if (bShowContext && bHasSource)
	{
		FieldBox->AddSlot()
			.Padding(8, 0, 0, 0)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SBindingContextEntry)
				.TextStyle(TextStyle)
				.BindingContext(FBindingSource::CreateFromPropertyPath(WidgetBlueprint.Get(), InPropertyPath))
			];

		if (InPropertyPath.HasPaths())
		{
			FieldBox->AddSlot()
				.Padding(6, 0)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.ChevronRight"))
				];
		}
	}

	if (InPropertyPath.HasPaths())
	{
		FieldBox->AddSlot()
			.Padding(0, 0, 8, 0)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SFieldPaths)
				.TextStyle(TextStyle)
				.FieldPaths(InPropertyPath.GetFields(WidgetBlueprint.Get()->SkeletonGeneratedClass))
				.ShowOnlyLast(bShowOnlyLastPath)
			];
	}
}


} // namespace UE::MVVM

#undef LOCTEXT_NAMESPACE
