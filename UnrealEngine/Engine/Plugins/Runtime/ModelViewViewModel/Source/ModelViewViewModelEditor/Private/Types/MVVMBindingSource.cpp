// Copyright Epic Games, Inc. All Rights Reserved.

#include "Types/MVVMBindingSource.h"
#include "Blueprint/WidgetTree.h"
#include "Editor.h"
#include "MVVMBlueprintView.h"
#include "MVVMPropertyPath.h"
#include "MVVMEditorSubsystem.h"
#include "Types/MVVMBindingName.h"
#include "WidgetBlueprint.h"

FMVVMBindingName UE::MVVM::FBindingSource::ToBindingName(const UWidgetBlueprint* WidgetBlueprint) const
{
	if (ViewModelId.IsValid())
	{
		if (UMVVMBlueprintView* View = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>()->GetView(WidgetBlueprint))
		{
			if (const FMVVMBlueprintViewModelContext* ViewModel = View->FindViewModel(ViewModelId))
			{
				return FMVVMBindingName(ViewModel->GetViewModelName());
			}
		}
		return FMVVMBindingName();
	}
	else
	{
		return FMVVMBindingName(Name);
	}
}

UE::MVVM::FBindingSource UE::MVVM::FBindingSource::CreateForWidget(const UWidgetBlueprint* WidgetBlueprint, FName WidgetName)
{
	UE::MVVM::FBindingSource Source;

	Source.Name = WidgetName;

	if (Source.Name == WidgetBlueprint->GetFName())
	{
		Source.DisplayName = FText::FromString(WidgetBlueprint->GetName());
		Source.Class = WidgetBlueprint->GeneratedClass;
	}
	else if (UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(Source.Name))
	{
		Source.DisplayName = Widget->GetLabelText();
		Source.Class = Widget->GetClass();
	}

	return Source;
}

UE::MVVM::FBindingSource UE::MVVM::FBindingSource::CreateForViewModel(const UWidgetBlueprint* WidgetBlueprint, FGuid ViewModelId)
{
	UE::MVVM::FBindingSource Source;
	Source.ViewModelId = ViewModelId;

	UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
	if (UMVVMBlueprintView* View = Subsystem->GetView(WidgetBlueprint))
	{
		if (const FMVVMBlueprintViewModelContext* ViewModel = View->FindViewModel(Source.ViewModelId))
		{
			Source.DisplayName = ViewModel->GetDisplayName();
			Source.Class = ViewModel->GetViewModelClass();
		}
	}

	return Source;
}

UE::MVVM::FBindingSource UE::MVVM::FBindingSource::CreateForViewModel(const UWidgetBlueprint* WidgetBlueprint, FName ViewModelName)
{
	UE::MVVM::FBindingSource Source;

	UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
	if (UMVVMBlueprintView* View = Subsystem->GetView(WidgetBlueprint))
	{
		if (const FMVVMBlueprintViewModelContext* ViewModel = View->FindViewModel(ViewModelName))
		{
			Source.ViewModelId = ViewModel->GetViewModelId();
			Source.DisplayName = ViewModel->GetDisplayName();
			Source.Class = ViewModel->GetViewModelClass();
		}
	}

	return Source;
}


UE::MVVM::FBindingSource UE::MVVM::FBindingSource::CreateFromPropertyPath(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintPropertyPath& Path)
{
	if (Path.IsFromViewModel())
	{
		return FBindingSource::CreateForViewModel(WidgetBlueprint, Path.GetViewModelId());
	}
	else if (Path.IsFromWidget())
	{
		return FBindingSource::CreateForWidget(WidgetBlueprint, Path.GetWidgetName());
	}
	return FBindingSource();
}
