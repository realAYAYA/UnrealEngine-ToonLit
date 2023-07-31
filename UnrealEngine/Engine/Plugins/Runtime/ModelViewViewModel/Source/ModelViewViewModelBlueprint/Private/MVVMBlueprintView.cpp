// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMBlueprintView.h"

#include "MVVMWidgetBlueprintExtension_View.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMBlueprintView)


FMVVMBlueprintViewModelContext* UMVVMBlueprintView::FindViewModel(FGuid ViewModelId)
{
	return AvailableViewModels.FindByPredicate([ViewModelId](const FMVVMBlueprintViewModelContext& Other)
		{
			return Other.GetViewModelId() == ViewModelId;
		});
}

const FMVVMBlueprintViewModelContext* UMVVMBlueprintView::FindViewModel(FGuid ViewModelId) const
{
	return const_cast<UMVVMBlueprintView*>(this)->FindViewModel(ViewModelId);
}

const FMVVMBlueprintViewModelContext* UMVVMBlueprintView::FindViewModel(FName ViewModel) const
{
	return AvailableViewModels.FindByPredicate([ViewModel](const FMVVMBlueprintViewModelContext& Other)
		{
			return Other.GetViewModelName() == ViewModel;
		});
}

void UMVVMBlueprintView::AddViewModel(const FMVVMBlueprintViewModelContext& NewContext)
{
	AvailableViewModels.Add(NewContext);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint());
	OnViewModelsUpdated.Broadcast();
}


bool UMVVMBlueprintView::RemoveViewModel(FGuid ViewModelId)
{
	int32 Count = AvailableViewModels.RemoveAll([ViewModelId](const FMVVMBlueprintViewModelContext& VM)
		{
			return VM.GetViewModelId() == ViewModelId;
		});

	if (Count > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint());
		OnViewModelsUpdated.Broadcast();
	}
	return Count > 0;
}

int32 UMVVMBlueprintView::RemoveViewModels(const TArrayView<FGuid> ViewModelIds)
{
	int32 Count = 0;
	for (const FGuid& ViewModelId : ViewModelIds)
	{
		Count += AvailableViewModels.RemoveAll([ViewModelId](const FMVVMBlueprintViewModelContext& VM)
			{
				return VM.GetViewModelId() == ViewModelId;
			});
	}

	if (Count > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint());
		OnViewModelsUpdated.Broadcast();
	}
	return Count;
}

bool UMVVMBlueprintView::RenameViewModel(FName OldViewModelName, FName NewViewModelName)
{
	FMVVMBlueprintViewModelContext* ViewModelContext = AvailableViewModels.FindByPredicate([OldViewModelName](const FMVVMBlueprintViewModelContext& Other)
			{
				return Other.GetViewModelName() == OldViewModelName;
			});
	if (ViewModelContext)
	{
		ViewModelContext->ViewModelName = NewViewModelName;

		FBlueprintEditorUtils::ReplaceVariableReferences(GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint(), OldViewModelName, NewViewModelName);
		FBlueprintEditorUtils::ValidateBlueprintChildVariables(GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint(), NewViewModelName);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint());

		OnViewModelsUpdated.Broadcast();
	}
	return ViewModelContext != nullptr;
}

void UMVVMBlueprintView::SetViewModels(const TArray<FMVVMBlueprintViewModelContext>& ViewModelContexts)
{
	AvailableViewModels = ViewModelContexts;

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint());
	OnViewModelsUpdated.Broadcast();
}

const FMVVMBlueprintViewBinding* UMVVMBlueprintView::FindBinding(const UWidget* Widget, const FProperty* Property) const
{
	return const_cast<UMVVMBlueprintView*>(this)->FindBinding(Widget, Property);
}

FMVVMBlueprintViewBinding* UMVVMBlueprintView::FindBinding(const UWidget* Widget, const FProperty* Property)
{
	FName WidgetName = Widget->GetFName();
	return Bindings.FindByPredicate([WidgetName, Property](const FMVVMBlueprintViewBinding& Binding)
		{
			return Binding.WidgetPath.GetWidgetName() == WidgetName &&
				Binding.WidgetPath.BasePropertyPathContains(UE::MVVM::FMVVMConstFieldVariant(Property));
		});
}

void UMVVMBlueprintView::RemoveBindingAt(int32 Index)
{
	if (Bindings.IsValidIndex(Index))
	{
		Bindings.RemoveAt(Index);
		OnBindingsUpdated.Broadcast();
	}
}

void UMVVMBlueprintView::RemoveBinding(const FMVVMBlueprintViewBinding* Binding)
{
	int32 Index = 0;
	for (; Index < Bindings.Num(); ++Index)
	{
		if (&Bindings[Index] == Binding)
		{
			break;
		}
	}

	RemoveBindingAt(Index);
}

FMVVMBlueprintViewBinding& UMVVMBlueprintView::AddBinding(const UWidget* Widget, const FProperty* Property)
{
	FMVVMBlueprintViewBinding& NewBinding = Bindings.AddDefaulted_GetRef();
	NewBinding.WidgetPath.SetWidgetName(Widget->GetFName());
	NewBinding.WidgetPath.SetBasePropertyPath(UE::MVVM::FMVVMConstFieldVariant(Property));
	NewBinding.BindingId = FGuid::NewGuid();

	OnBindingsUpdated.Broadcast();
	return NewBinding;
}

FMVVMBlueprintViewBinding& UMVVMBlueprintView::AddDefaultBinding()
{
	FMVVMBlueprintViewBinding& NewBinding = Bindings.AddDefaulted_GetRef();
	NewBinding.BindingId = FGuid::NewGuid();

	OnBindingsUpdated.Broadcast();
	return NewBinding;
}

FMVVMBlueprintViewBinding* UMVVMBlueprintView::GetBindingAt(int32 Index)
{
	if (Bindings.IsValidIndex(Index))
	{
		return &Bindings[Index];
	}
	return nullptr;
}

const FMVVMBlueprintViewBinding* UMVVMBlueprintView::GetBindingAt(int32 Index) const
{
	if (Bindings.IsValidIndex(Index))
	{
		return &Bindings[Index];
	}
	return nullptr;
}

void UMVVMBlueprintView::PostLoad()
{
	Super::PostLoad();

	for (FMVVMBlueprintViewBinding& Binding : Bindings)
	{
		if (!Binding.BindingId.IsValid())
		{
			Binding.BindingId = FGuid::NewGuid();
		}
	}
}

#if WITH_EDITOR

/*
void UMVVMBlueprintView::PostEditUndo() 
{
	OnBindingsUpdated.Broadcast();
	OnViewModelsUpdated.Broadcast();
}*/

void UMVVMBlueprintView::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UMVVMBlueprintView, Bindings))
	{
		OnBindingsUpdated.Broadcast();
	}
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UMVVMBlueprintView, AvailableViewModels))
	{
		OnViewModelsUpdated.Broadcast();
	}
}

void UMVVMBlueprintView::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChainEvent)
{
	Super::PostEditChangeChainProperty(PropertyChainEvent);
	if (PropertyChainEvent.PropertyChain.Contains(UMVVMBlueprintView::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMVVMBlueprintView, Bindings))))
	{
		OnBindingsUpdated.Broadcast();
	}
	if (PropertyChainEvent.PropertyChain.Contains(UMVVMBlueprintView::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMVVMBlueprintView, AvailableViewModels))))
	{
		OnViewModelsUpdated.Broadcast();
	}
}

void UMVVMBlueprintView::WidgetRenamed(FName OldObjectName, FName NewObjectName)
{
	bool bRenamed = false;
	for (FMVVMBlueprintViewBinding& Binding : Bindings)
	{
		if (Binding.WidgetPath.GetWidgetName() == OldObjectName)
		{
			Binding.WidgetPath.SetWidgetName(NewObjectName);
			bRenamed = true;
		}
	}

	if (bRenamed)
	{
		OnBindingsUpdated.Broadcast();
	}
}
#endif

