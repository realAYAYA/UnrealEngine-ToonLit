// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMBlueprintView.h"

#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MVVMBlueprintInstancedViewModel.h"
#include "MVVMBlueprintViewConversionFunction.h"
#include "MVVMBlueprintViewEvent.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "WidgetBlueprint.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "UObject/FortniteMainBranchObjectVersion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMBlueprintView)


UMVVMBlueprintView::UMVVMBlueprintView()
{
	Settings = CreateDefaultSubobject<UMVVMBlueprintViewSettings>("Settings");
	CompiledBindingLibraryId = FGuid::NewGuid();
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

namespace UE::MVVM::Private
{
	bool RemoveViewModelInternal(FGuid ViewModelId, TArray<FMVVMBlueprintViewModelContext>& ViewModelContexts)
	{
		bool bResult = false;
		for (int32 Index = ViewModelContexts.Num() - 1; Index >= 0; --Index)
		{
			if (ViewModelContexts[Index].GetViewModelId() == ViewModelId)
			{
				UMVVMBlueprintInstancedViewModelBase* InstancedViewModel = ViewModelContexts[Index].InstancedViewModel;
				if (InstancedViewModel)
				{
					auto RenameToTransient = [](UObject* ObjectToRename)
					{
						FName TrashName = MakeUniqueObjectName(GetTransientPackage(), ObjectToRename->GetClass(), *FString::Printf(TEXT("TRASH_%s"), *ObjectToRename->GetName()));
						ObjectToRename->Rename(*TrashName.ToString(), GetTransientPackage());
					};
					if (InstancedViewModel->GetGeneratedClass())
					{
						RenameToTransient(InstancedViewModel->GetGeneratedClass());
					}
					RenameToTransient(InstancedViewModel);
				}

				ViewModelContexts.RemoveAt(Index);
				bResult = true;
			}
		}
		return bResult;
	}
}

bool UMVVMBlueprintView::RemoveViewModel(FGuid ViewModelId)
{
	bool bRemoved = UE::MVVM::Private::RemoveViewModelInternal(ViewModelId, AvailableViewModels);
	if (bRemoved)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint());
		OnViewModelsUpdated.Broadcast();
	}
	return bRemoved;
}

int32 UMVVMBlueprintView::RemoveViewModels(const TArrayView<FGuid> ViewModelIds)
{
	int32 Count = 0;
	for (const FGuid& ViewModelId : ViewModelIds)
	{
		if (UE::MVVM::Private::RemoveViewModelInternal(ViewModelId, AvailableViewModels))
		{
			++Count;
		}
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

bool UMVVMBlueprintView::ReparentViewModel(FGuid ViewModelId, const UClass* ViewModelClass)
{
	if (ViewModelClass && ViewModelClass->ImplementsInterface(UNotifyFieldValueChanged::StaticClass()))
	{
		FMVVMBlueprintViewModelContext* ViewModelContext = AvailableViewModels.FindByPredicate([ViewModelId](const FMVVMBlueprintViewModelContext& Other)
			{
				return Other.GetViewModelId() == ViewModelId;
			});
		if (ViewModelContext)
		{
			ViewModelContext->NotifyFieldValueClass = const_cast<UClass*>(ViewModelClass);
			TArray<EMVVMBlueprintViewModelContextCreationType> ValidCreationTypes = UE::MVVM::GetAllowedContextCreationType(ViewModelClass);
			if (!ValidCreationTypes.Contains(ViewModelContext->CreationType))
			{
				if (ensureMsgf(ValidCreationTypes.Num() > 0, TEXT("There is no valid creation type for this class.")))
				{
					ViewModelContext->CreationType = ValidCreationTypes[0];
				}
			}

			FBlueprintEditorUtils::MarkBlueprintAsModified(GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint());

			OnViewModelsUpdated.Broadcast();
			return true;
		}
	}
	return false;
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

		FBlueprintEditorUtils::MarkBlueprintAsModified(GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint());
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

FMVVMBlueprintViewBinding& UMVVMBlueprintView::AddDefaultBinding()
{
	FMVVMBlueprintViewBinding& NewBinding = Bindings.AddDefaulted_GetRef();
	NewBinding.BindingId = FGuid::NewGuid();

	OnBindingsAdded.Broadcast();
	OnBindingsUpdated.Broadcast();

	FBlueprintEditorUtils::MarkBlueprintAsModified(GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint());

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

UMVVMBlueprintViewEvent* UMVVMBlueprintView::AddDefaultEvent()
{
	UMVVMBlueprintViewEvent* Event = NewObject<UMVVMBlueprintViewEvent>(this);
	Events.Add(Event);

	OnBindingsAdded.Broadcast();
	OnBindingsUpdated.Broadcast();
	return Event;
}

void UMVVMBlueprintView::RemoveEvent(UMVVMBlueprintViewEvent* Event)
{
	if (Events.RemoveAll([Event](TObjectPtr<UMVVMBlueprintViewEvent>& Other){ return Other == Event; }) > 0)
	{
		Event->RemoveWrapperGraph();
		OnBindingsUpdated.Broadcast();
	}
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
namespace UE::MVVM::Private
{
	template<typename Predicate>
	void ForEachPropertyPath_Update(UMVVMBlueprintView* BlueprintView, Predicate Pred, bool bGenerateGraph)
	{
		UWidgetBlueprint* WidgetBlueprint = BlueprintView->GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint();
		auto PredPin = [&Pred](const FMVVMBlueprintPin& Pin) -> TOptional<FMVVMBlueprintPropertyPath>
		{
			if (Pin.UsedPathAsValue())
			{
				FMVVMBlueprintPropertyPath NewPinPath = Pin.GetPath();
				if (Pred(NewPinPath))
				{
					return NewPinPath;
				}
			}
			return TOptional<FMVVMBlueprintPropertyPath>();
		};

		for (FMVVMBlueprintViewBinding& Binding : BlueprintView->GetBindings())
		{
			Pred(Binding.SourcePath);
			Pred(Binding.DestinationPath);
			if (Binding.Conversion.DestinationToSourceConversion)
			{
				Binding.Conversion.DestinationToSourceConversion->ConditionalPostLoad();
				for (const FMVVMBlueprintPin& Pin : Binding.Conversion.DestinationToSourceConversion->GetPins())
				{
					TOptional<FMVVMBlueprintPropertyPath> NewPath = PredPin(Pin);
					if (NewPath.IsSet())
					{
						if (bGenerateGraph)
						{
							Binding.Conversion.DestinationToSourceConversion->SetGraphPin(WidgetBlueprint, Pin.GetId(), NewPath.GetValue());
						}
						else
						{
							const_cast<FMVVMBlueprintPin&>(Pin).SetPath(NewPath.GetValue());
						}
					}
				}
			}
			if (Binding.Conversion.SourceToDestinationConversion)
			{
				Binding.Conversion.SourceToDestinationConversion->ConditionalPostLoad();
				for (const FMVVMBlueprintPin& Pin : Binding.Conversion.SourceToDestinationConversion->GetPins())
				{
					TOptional<FMVVMBlueprintPropertyPath> NewPath = PredPin(Pin);
					if (NewPath.IsSet())
					{
						if (bGenerateGraph)
						{
							Binding.Conversion.SourceToDestinationConversion->SetGraphPin(WidgetBlueprint, Pin.GetId(), NewPath.GetValue());
						}
						else
						{
							const_cast<FMVVMBlueprintPin&>(Pin).SetPath(NewPath.GetValue());
						}
					}
				}
			}
		}

		for (UMVVMBlueprintViewEvent* Event : BlueprintView->GetEvents())
		{
			if (Event)
			{
				auto PredEventPath = [Event, &Pred, bGenerateGraph](const FMVVMBlueprintPropertyPath& PropertyPath, bool bEventPath)
				{
					FMVVMBlueprintPropertyPath NewPropertyPath = PropertyPath;
					if (Pred(NewPropertyPath))
					{
						if (bGenerateGraph)
						{
							if (bEventPath)
							{
								Event->SetEventPath(NewPropertyPath);
							}
							else
							{
								Event->SetDestinationPath(NewPropertyPath);
							}
						}
						else
						{
							const_cast<FMVVMBlueprintPropertyPath&>(PropertyPath) = NewPropertyPath;
						}
					}
				};

				TArray<TTuple<FMVVMBlueprintPinId, FMVVMBlueprintPropertyPath>> NewPins;
				for (const FMVVMBlueprintPin& Pin : Event->GetPins())
				{
					TOptional<FMVVMBlueprintPropertyPath> NewPath = PredPin(Pin);
					if (NewPath.IsSet())
					{
						NewPins.Emplace(Pin.GetId(), MoveTemp(NewPath.GetValue()));
					}
				}
				PredEventPath(Event->GetEventPath(), true);
				PredEventPath(Event->GetDestinationPath(), false);
				for (TTuple<FMVVMBlueprintPinId, FMVVMBlueprintPropertyPath>& Pin : NewPins)
				{
					if (bGenerateGraph)
					{
						Event->SetPinPath(Pin.Get<0>(), Pin.Get<1>());
					}
					else
					{
						Event->SetPinPathNoGraphGeneration(Pin.Get<0>(), Pin.Get<1>());
					}
				}
			}
		}
	}
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
		Binding.Conversion.DeprecateViewConversionFunction(GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint(), Binding);
	}

	if (GetLinkerCustomVersion(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::MVVMPropertyPathSelf)
	{
		const UWidgetBlueprint* ThisWidgetBlueprint = GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint();
		auto DeprecateSelfPath = [ThisWidgetBlueprint](FMVVMBlueprintPropertyPath& Path) -> bool
		{
			Path.GetSource(ThisWidgetBlueprint);
			return true;
		};
		UE::MVVM::Private::ForEachPropertyPath_Update(this, DeprecateSelfPath, false);
	}
}

void UMVVMBlueprintView::PreSave(FObjectPreSaveContext Context)
{
	UWidgetBlueprint* WidgetBlueprint = GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint();
	for (FMVVMBlueprintViewBinding& Binding : Bindings)
	{
		Binding.Conversion.SavePinValues(WidgetBlueprint);
	}
	for (UMVVMBlueprintViewEvent* Event : Events)
	{
		Event->SavePinValues();
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
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UMVVMBlueprintView, Events))
	{
		OnEventsUpdated.Broadcast();
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
	if (PropertyChainEvent.PropertyChain.Contains(UMVVMBlueprintView::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMVVMBlueprintView, Events))))
	{
		OnEventsUpdated.Broadcast();
	}
}

void UMVVMBlueprintView::AddAssetTags(TArray<FAssetRegistryTag>& OutTags) const
{
}

void UMVVMBlueprintView::AddAssetTags(FAssetRegistryTagsContext Context) const
{
	if (AvailableViewModels.Num() > 0)
	{
		TStringBuilder<512> Builder;
		for (const FMVVMBlueprintViewModelContext& ViewModelContext : AvailableViewModels)
		{
			if (ViewModelContext.IsValid())
			{
				if (Builder.Len() > 0)
				{
					Builder << TEXT(',');
				}
				Builder << ViewModelContext.GetViewModelName();
				Builder << TEXT(';');
				Builder << ViewModelContext.GetViewModelClass()->GetPathName();
			}
		}

		if (Builder.Len() > 0)
		{
			Context.AddTag(FAssetRegistryTag(FName("Viewmodels"), Builder.ToString(), FAssetRegistryTag::TT_Hidden));
		}
	}
}

void UMVVMBlueprintView::WidgetRenamed(FName OldObjectName, FName NewObjectName)
{
	bool bRenamed = false;

	UWidgetBlueprint* WidgetBlueprint = GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint();

	auto RenameWidget = [WidgetBlueprint , &bRenamed, OldObjectName, NewObjectName](FMVVMBlueprintPropertyPath& PropertyPath) -> bool
	{
		if (PropertyPath.GetSource(WidgetBlueprint) == EMVVMBlueprintFieldPathSource::Widget && PropertyPath.GetWidgetName() == OldObjectName)
		{
			PropertyPath.SetWidgetName(NewObjectName);
			bRenamed = true;
			return true;
		}
		return false;
	};

	UE::MVVM::Private::ForEachPropertyPath_Update(this, RenameWidget, true);

	if (bRenamed)
	{
		Modify();
		OnBindingsUpdated.Broadcast();
	}
}
#endif

