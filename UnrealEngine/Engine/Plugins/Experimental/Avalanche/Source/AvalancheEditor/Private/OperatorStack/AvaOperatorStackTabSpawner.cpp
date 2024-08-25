// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaOperatorStackTabSpawner.h"
#include "DetailView/AvaDetailsExtension.h"
#include "OperatorStack/Widgets/SAvaOperatorStackTab.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateIconFinder.h"

#define LOCTEXT_NAMESPACE "AvaOperatorStackTabSpawner"

FName FAvaOperatorStackTabSpawner::GetTabID()
{
	return TEXT("AvalancheOperatorStack");
}

FAvaOperatorStackTabSpawner::FAvaOperatorStackTabSpawner(const TSharedRef<IAvaEditor>& InEditor)
	: FAvaTabSpawner(InEditor, FAvaOperatorStackTabSpawner::GetTabID())
{
	TabLabel       = LOCTEXT("TabLabel", "Operator Stack");
	TabTooltipText = LOCTEXT("TabTooltip", "Operator Stack");
	TabIcon        = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.UserDefinedStruct");
}

TSharedRef<SWidget> FAvaOperatorStackTabSpawner::CreateTabBody()
{
	const TSharedPtr<IAvaEditor> Editor = EditorWeak.Pin();
	if (!Editor.IsValid())
	{
		return GetNullWidget();
	}

	const TSharedPtr<FAvaDetailsExtension> DetailsExtension = Editor->FindExtension<FAvaDetailsExtension>();
	if (!DetailsExtension.IsValid())
	{
		return GetNullWidget();
	}

	return SNew(SAvaOperatorStackTab, DetailsExtension.ToSharedRef());
}

#undef LOCTEXT_NAMESPACE