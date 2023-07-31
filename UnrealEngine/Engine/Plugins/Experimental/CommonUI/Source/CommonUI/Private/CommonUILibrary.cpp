// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommonUILibrary.h"
#include "Components/PanelWidget.h"
#include "Blueprint/WidgetTree.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonUILibrary)

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UCommonUILibrary

UCommonUILibrary::UCommonUILibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UWidget* UCommonUILibrary::FindParentWidgetOfType(UWidget* StartingWidget, TSubclassOf<UWidget> Type)
{
	while ( StartingWidget )
	{
		UWidget* LocalRoot = StartingWidget;
		UWidget* LocalParent = LocalRoot->GetParent();
		while (LocalParent)
		{
			if (LocalParent->IsA(Type))
			{
				return LocalParent;
			}
			LocalRoot = LocalParent;
			LocalParent = LocalParent->GetParent();
		}

		UWidgetTree* WidgetTree = Cast<UWidgetTree>(LocalRoot->GetOuter());
		if ( WidgetTree == nullptr )
		{
			break;
		}

		StartingWidget = Cast<UUserWidget>(WidgetTree->GetOuter());
		if ( StartingWidget && StartingWidget->IsA(Type) )
		{
			return StartingWidget;
		}
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE

