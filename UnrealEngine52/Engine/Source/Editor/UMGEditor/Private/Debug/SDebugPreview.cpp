// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDebugPreview.h"
#include "Blueprint/UserWidget.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditor.h"

namespace UE::UMG
{

void SDebugPreview::Construct(const FArguments& Args, TSharedPtr<FWidgetBlueprintEditor> InWidgetBlueprintEditor)
{
	WidgetBlueprintEditor = InWidgetBlueprintEditor;

	TSharedPtr<SWidget> SlateWidget;

	if (UWidgetBlueprint* WidgetBlueprint = InWidgetBlueprintEditor->GetWidgetBlueprintObj())
	{
		if (UWorld* World = GEditor->GetEditorWorldContext().World())
		{
			if (UWidgetBlueprintGeneratedClass* WidgetClass = Cast<UWidgetBlueprintGeneratedClass>(WidgetBlueprint->GeneratedClass))
			{
				CreatedWidget = CreateWidget(World, WidgetClass, WidgetClass->GetFName());
				if (CreatedWidget != nullptr)
				{
					CreatedWidget->SetDesignerFlags(EWidgetDesignFlags::Previewing);
					SlateWidget = CreatedWidget->TakeWidget();
				}
			}
		}
	}

	ChildSlot
	[
		SlateWidget.IsValid() ? SlateWidget.ToSharedRef() : SNullWidget::NullWidget
	];
}

SDebugPreview::~SDebugPreview()
{
}

} // namespace UE::UMG
