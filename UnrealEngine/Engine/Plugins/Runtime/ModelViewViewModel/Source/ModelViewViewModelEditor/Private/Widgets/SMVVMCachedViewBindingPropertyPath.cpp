// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMVVMCachedViewBindingPropertyPath.h"

#include "Editor.h"
#include "MVVMBlueprintView.h"
#include "MVVMEditorSubsystem.h"
#include "Styling/MVVMEditorStyle.h"
#include "WidgetBlueprint.h"
#include "Widgets/SMVVMPropertyPath.h"

#define LOCTEXT_NAMESPACE "SCachedViewBindingPropertyPath"

namespace UE::MVVM
{

void SCachedViewBindingPropertyPath::Construct(const FArguments& InArgs, const UWidgetBlueprint* InWidgetBlueprint)
{
	SetCanTick(true);

	WidgetBlueprint = InWidgetBlueprint;
	OnGetPropertyPath = InArgs._OnGetPropertyPath;
	CachedPropertyPath = OnGetPropertyPath.Execute();

	ChildSlot
	[
		SAssignNew(PropertyPathWidget, SPropertyPath, InWidgetBlueprint)
		.TextStyle(InArgs._TextStyle)
		.PropertyPath(CachedPropertyPath)
		.ShowContext(InArgs._ShowContext)
		.ShowOnlyLastPath(InArgs._ShowOnlyLastPath)
	];
}

void SCachedViewBindingPropertyPath::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	Super::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	FMVVMBlueprintPropertyPath NewPropertyPath = OnGetPropertyPath.Execute();
	if (NewPropertyPath != CachedPropertyPath)
	{
		PropertyPathWidget->SetPropertyPath(NewPropertyPath);
		CachedPropertyPath = MoveTemp(NewPropertyPath);
	}
}


} // namespace UE::MVVM

#undef LOCTEXT_NAMESPACE
