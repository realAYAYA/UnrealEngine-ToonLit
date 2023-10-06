// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMBlueprintView.h"

#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MVVMBlueprintViewConversionFunction.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "WidgetBlueprint.h"
#include "UObject/FortniteMainBranchObjectVersion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMBlueprintView)


UMVVMBlueprintView::UMVVMBlueprintView()
{
	Settings = CreateDefaultSubobject<UMVVMBlueprintViewSettings>("Settings");
}

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

const FMVVMBlueprintViewBinding* UMVVMBlueprintView::FindBinding(const UWidget* Widget, const FProperty* Property) const
{
	return const_cast<UMVVMBlueprintView*>(this)->FindBinding(Widget, Property);
}

FMVVMBlueprintViewBinding* UMVVMBlueprintView::FindBinding(const UWidget* Widget, const FProperty* Property)
{
	FName WidgetName = Widget->GetFName();
	return Bindings.FindByPredicate([WidgetBlueprint = GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint(), WidgetName, Property](const FMVVMBlueprintViewBinding& Binding)
		{
			return Binding.DestinationPath.GetWidgetName() == WidgetName &&
				Binding.DestinationPath.PropertyPathContains(WidgetBlueprint->GeneratedClass, UE::MVVM::FMVVMConstFieldVariant(Property));
		});
}

void UMVVMBlueprintView::RemoveBindingAt(int32 Index)
{
	if (Bindings.IsValidIndex(Index))
	{
		if (Bindings[Index].Conversion.SourceToDestinationConversion)
		{
			Bindings[Index].Conversion.SourceToDestinationConversion->RemoveWrapperGraph(GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint());
		}
		if (Bindings[Index].Conversion.DestinationToSourceConversion)
		{
			Bindings[Index].Conversion.DestinationToSourceConversion->RemoveWrapperGraph(GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint());
		}

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
	NewBinding.DestinationPath.SetWidgetName(Widget->GetFName());
	NewBinding.DestinationPath.SetPropertyPath(GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint(), UE::MVVM::FMVVMConstFieldVariant(Property));
	NewBinding.BindingId = FGuid::NewGuid();

	OnBindingsAdded.Broadcast();
	OnBindingsUpdated.Broadcast();
	return NewBinding;
}

FMVVMBlueprintViewBinding& UMVVMBlueprintView::AddDefaultBinding()
{
	FMVVMBlueprintViewBinding& NewBinding = Bindings.AddDefaulted_GetRef();
	NewBinding.BindingId = FGuid::NewGuid();

	OnBindingsAdded.Broadcast();
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

FMVVMBlueprintViewBinding* UMVVMBlueprintView::GetBinding(FGuid Id)
{
	return Bindings.FindByPredicate([Id](const FMVVMBlueprintViewBinding& Binding){ return Id == Binding.BindingId; });
}

const FMVVMBlueprintViewBinding* UMVVMBlueprintView::GetBinding(FGuid Id) const
{
	return Bindings.FindByPredicate([Id](const FMVVMBlueprintViewBinding& Binding) { return Id == Binding.BindingId; });
}

TArray<FText> UMVVMBlueprintView::GetBindingMessages(FGuid Id, UE::MVVM::EBindingMessageType InMessageType) const
{
	TArray<FText> Results;

	if (BindingMessages.Contains(Id))
	{
		const TArray<UE::MVVM::FBindingMessage>& AllBindingMessages = BindingMessages[Id];
		for (const UE::MVVM::FBindingMessage& Message : AllBindingMessages)
		{
			if (Message.MessageType == InMessageType)
			{
				Results.Add(Message.MessageText);
			}
		}
	}
	return Results;
}

bool UMVVMBlueprintView::HasBindingMessage(FGuid Id, UE::MVVM::EBindingMessageType InMessageType) const
{
	if (const TArray<UE::MVVM::FBindingMessage>* FoundBindingMessages = BindingMessages.Find(Id))
	{
		for (const UE::MVVM::FBindingMessage& Message : *FoundBindingMessages)
		{
			if (Message.MessageType == InMessageType)
			{
				return true;
			}
		}
	}

	return false;
}

void UMVVMBlueprintView::AddMessageToBinding(FGuid Id, UE::MVVM::FBindingMessage MessageToAdd)
{
	TArray<UE::MVVM::FBindingMessage>& FoundBindingMessages = BindingMessages.FindOrAdd(Id);
	FoundBindingMessages.Add(MessageToAdd);
}

void UMVVMBlueprintView::ResetBindingMessages()
{
	BindingMessages.Reset();
}

void UMVVMBlueprintView::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);	
}

#if WITH_EDITOR
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

	GetOuterUMVVMWidgetBlueprintExtension_View()->ConditionalPostLoad();
	GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint()->ConditionalPostLoad();

	// Make sure all bindings uses the skeletal class
	if (GetLinkerCustomVersion(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::MVVMConvertPropertyPathToSkeletalClass)
	{
		const UWidgetBlueprint* ThisWidgetBlueprint = GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint();
		for (FMVVMBlueprintViewBinding& Binding : Bindings)
		{
			for (const FMVVMBlueprintFieldPath& FieldPath : Binding.SourcePath.GetFieldPaths())
			{
				const_cast<FMVVMBlueprintFieldPath&>(FieldPath).SetDeprecatedSelfReference(ThisWidgetBlueprint);
			}
			for (const FMVVMBlueprintFieldPath& FieldPath : Binding.DestinationPath.GetFieldPaths())
			{
				const_cast<FMVVMBlueprintFieldPath&>(FieldPath).SetDeprecatedSelfReference(ThisWidgetBlueprint);
			}
		}
	}

	for (FMVVMBlueprintViewBinding& Binding : Bindings)
	{
		Binding.Conversion.DeprecateViewConversionFunction(GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint());
	}
}

void UMVVMBlueprintView::PreSave(FObjectPreSaveContext Context)
{
	UWidgetBlueprint* WidgetBlueprint = GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint();
	for (FMVVMBlueprintViewBinding& Binding : Bindings)
	{
		Binding.Conversion.SavePinValues(WidgetBlueprint);
	}

	Super::PreSave(Context);
}

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

void UMVVMBlueprintView::AddAssetTags(TArray<FAssetRegistryTag>& OutTags) const
{
	if (AvailableViewModels.Num() > 0)
	{
		TStringBuilder<512> Builder;
		for (const FMVVMBlueprintViewModelContext& Context : AvailableViewModels)
		{
			if (Context.IsValid())
			{
				if (Builder.Len() > 0)
				{
					Builder << TEXT(',');
				}
				Builder << Context.GetViewModelClass()->GetPathName();
			}
		}

		if (Builder.Len() > 0)
		{
			OutTags.Emplace(FName("Viewmodels"), Builder.ToString(), FAssetRegistryTag::TT_Hidden);
		}
	}
}

void UMVVMBlueprintView::WidgetRenamed(FName OldObjectName, FName NewObjectName)
{
	bool bRenamed = false;
	for (FMVVMBlueprintViewBinding& Binding : Bindings)
	{
		if (Binding.DestinationPath.GetWidgetName() == OldObjectName)
		{
			Binding.DestinationPath.SetWidgetName(NewObjectName);
			bRenamed = true;
		}
	}

	if (bRenamed)
	{
		OnBindingsUpdated.Broadcast();
	}
}
#endif

