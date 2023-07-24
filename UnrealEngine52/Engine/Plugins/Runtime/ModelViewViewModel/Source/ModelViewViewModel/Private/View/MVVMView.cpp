// Copyright Epic Games, Inc. All Rights Reserved.

#include "View/MVVMView.h"
#include "FieldNotification/FieldMulticastDelegate.h"
#include "View/MVVMViewClass.h"
#include "Misc/MemStack.h"
#include "View/MVVMViewWorldSubsystem.h"

#include "Debugging/MVVMDebugging.h"
#include "Blueprint/UserWidget.h"
#include "MVVMMessageLog.h"
#include "Templates/ValueOrError.h"
#include "Types/MVVMFieldContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMView)


#define LOCTEXT_NAMESPACE "MVVMView"

///////////////////////////////////////////////////////////////////////
// 
///////////////////////////////////////////////////////////////////////

void UMVVMView::ConstructView(const UMVVMViewClass* InClassExtension)
{
	ensure(ClassExtension == nullptr);
	ClassExtension = InClassExtension;
}


void UMVVMView::Construct()
{
	check(ClassExtension);
	check(bConstructed == false);

	// Init ViewModel instances
	for (const FMVVMViewClass_SourceCreator& Item : ClassExtension->GetViewModelCreators())
	{
		Item.CreateInstance(ClassExtension, this, GetUserWidget());
	}

#if UE_WITH_MVVM_DEBUGGING
	UE::MVVM::FDebugging::BroadcastViewConstructed(this);
#endif

	bHasEveryTickBinding = false;

	const TArrayView<const FMVVMViewClass_CompiledBinding> CompiledBindings = ClassExtension->GetCompiledBindings();
	for (int32 Index = 0; Index < CompiledBindings.Num(); ++Index)
	{
		const FMVVMViewClass_CompiledBinding& Binding = CompiledBindings[Index];
		if (Binding.IsEnabledByDefault())
		{
			EnableLibraryBinding(Binding, Index);
		}
	}

	bConstructed = true;

	if (bHasEveryTickBinding)
	{
		GetUserWidget()->GetWorld()->GetSubsystem<UMVVMViewWorldSubsystem>()->AddViewWithEveryTickBinding(this);
	}
}


void UMVVMView::Destruct()
{
	check(bConstructed == true);
	bConstructed = false;

#if UE_WITH_MVVM_DEBUGGING
	UE::MVVM::FDebugging::BroadcastViewBeginDestruction(this);
#endif

	for (FRegisteredSource& Source : AllSources)
	{
		UObject* SourceObject = Source.Source.Get();
		if (Source.Count > 0 && SourceObject)
		{
			TScriptInterface<INotifyFieldValueChanged> SourceAsInterface = SourceObject;
			checkf(SourceAsInterface.GetInterface(), TEXT("It was added as a INotifyFieldValueChanged. It should still be."));
			SourceAsInterface->RemoveAllFieldValueChangedDelegates(this);
		}
	}
	AllSources.Reset();
	EnabledLibraryBindings.Reset();

	if (bHasEveryTickBinding)
	{
		GetUserWidget()->GetWorld()->GetSubsystem<UMVVMViewWorldSubsystem>()->RemoveViewWithEveryTickBinding(this);
	}
	bHasEveryTickBinding = false;
}


bool UMVVMView::SetViewModel(FName ViewModelName, TScriptInterface<INotifyFieldValueChanged> NewValue)
{
	if (!ViewModelName.IsNone() && ClassExtension != nullptr)
	{
		FObjectPropertyBase* FoundObjectProperty = FindFProperty<FObjectPropertyBase>(GetUserWidget()->GetClass(), ViewModelName);
		if (FoundObjectProperty == nullptr)
		{
			UE_LOG(LogMVVM, Error, TEXT("There is no viewmodel named '%s' in the view '%s'"), *ViewModelName.ToString(), *GetFullName());
			return false;
		}

		{
			bool bFound = false;
			for (const FMVVMViewClass_SourceCreator& Item : ClassExtension->GetViewModelCreators())
			{
				if (Item.GetSourcePropertyName() == ViewModelName)
				{
					bFound = true;
					if (NewValue.GetObject() && !NewValue.GetObject()->GetClass()->IsChildOf(Item.GetSourceClass()))
					{
						UE::MVVM::FMessageLog Log(GetUserWidget());
						Log.Error(LOCTEXT("SetViewModelInvalidValueType", "The new viewmodel is not of the expected type."));
						UE_LOG(LogMVVM, Error, TEXT("The viewmodel name '%s' is invalid for the view '%s'"), *ViewModelName.ToString(), *GetFullName());
						return false;
					}
					break;
				}
			}

			if (!bFound)
			{
				UE::MVVM::FMessageLog Log(GetUserWidget());
				Log.Error(LOCTEXT("SetViewModelViewModelNameNotFound", "The viewmodel name could not be found."));
				return false;
			}
		}


		UObject* PreviousValue = FoundObjectProperty->GetObjectPropertyValue_InContainer(GetUserWidget());
		if (PreviousValue != NewValue.GetObject())
		{
			FMemMark Mark(FMemStack::Get());
			TArray<int32, TMemStackAllocator<>> BindingToReenabled;
			const TArrayView<const FMVVMViewClass_CompiledBinding> CompiledBindings = ClassExtension->GetCompiledBindings();

			// Unregister any from that source
			{
				BindingToReenabled.Reserve(CompiledBindings.Num());

				for (int32 Index = 0; Index < CompiledBindings.Num(); ++Index)
				{
					const FMVVMViewClass_CompiledBinding& Binding = CompiledBindings[Index];
					if (Binding.GetSourceObjectPropertyName() == ViewModelName && IsLibraryBindingEnabled(Index))
					{
						DisableLibraryBinding(Binding, Index);
						BindingToReenabled.Add(Index);
					}
				}
			}

			FoundObjectProperty->SetObjectPropertyValue_InContainer(GetUserWidget(), NewValue.GetObject());

			bool bPreviousEveryTickBinding = bHasEveryTickBinding;
			bHasEveryTickBinding = false;
			if (NewValue.GetObject())
			{
				// Register back any binding that was previously enabled
				if (BindingToReenabled.Num() > 0)
				{
					for (int32 Index : BindingToReenabled)
					{
						const FMVVMViewClass_CompiledBinding& Binding = CompiledBindings[Index];
						EnableLibraryBinding(Binding, Index);
					}
				}
				else if (bConstructed)
				{
					// Enabled the default bindings
					for (int32 Index = 0; Index < CompiledBindings.Num(); ++Index)
					{
						const FMVVMViewClass_CompiledBinding& Binding = CompiledBindings[Index];
						if (Binding.IsEnabledByDefault() && Binding.GetSourceObjectPropertyName() == ViewModelName)
						{
							EnableLibraryBinding(Binding, Index);
						}
					}
				}
			}

			if (bPreviousEveryTickBinding != bHasEveryTickBinding)
			{
				if (bHasEveryTickBinding)
				{
					GetUserWidget()->GetWorld()->GetSubsystem<UMVVMViewWorldSubsystem>()->AddViewWithEveryTickBinding(this);
				}
				else
				{
					GetUserWidget()->GetWorld()->GetSubsystem<UMVVMViewWorldSubsystem>()->RemoveViewWithEveryTickBinding(this);
				}
			}
		}
		return true;
	}
	return false;
}


namespace UE::MVVM::Private
{
	using FRecursiveDetctionElement = TTuple<const UObject*, int32>;
	TArray<FRecursiveDetctionElement> RecursiveDetector;
}

void UMVVMView::HandledLibraryBindingValueChanged(UObject* InViewModelOrWidget, UE::FieldNotification::FFieldId InFieldId, int32 InCompiledBindingIndex) const
{
	check(InViewModelOrWidget);
	check(InFieldId.IsValid());

	if (ensure(ClassExtension))
	{
		checkf(ClassExtension->GetCompiledBindings().IsValidIndex(InCompiledBindingIndex), TEXT("The binding at index '%d' does not exist. The binding was probably not cleared on destroyed."), InCompiledBindingIndex);
		const FMVVMViewClass_CompiledBinding& Binding = ClassExtension->GetCompiledBinding(InCompiledBindingIndex);

		EMVVMExecutionMode ExecutionMode = Binding.GetExecuteMode();
		if (ExecutionMode == EMVVMExecutionMode::Delayed)
		{
			GetUserWidget()->GetWorld()->GetSubsystem<UMVVMViewWorldSubsystem>()->AddDelayedBinding(this, InCompiledBindingIndex);
		}
		else if (ExecutionMode != EMVVMExecutionMode::Tick)
		{
			// Test for recursivity
			const UMVVMView* Self = this;
			if (UE::MVVM::Private::RecursiveDetector.FindByPredicate([Self, InCompiledBindingIndex](const UE::MVVM::Private::FRecursiveDetctionElement& Element)
				{
					return Element.Get<0>() == Self && Element.Get<1>() == InCompiledBindingIndex;
				}) != nullptr)
			{
				ensureAlwaysMsgf(false, TEXT("Recursive binding detected"));
				//Todo add more infos. Callstack maybe? Log the chain?
				UE::MVVM::FMessageLog Log(GetUserWidget());
				Log.Error(LOCTEXT("RecursionDetected", "A recursive binding was detected (ie. A->B->C->A->B->C) at runtime."));
				return;
			}

			{
				UE::MVVM::Private::RecursiveDetector.Emplace(this, InCompiledBindingIndex);

				ExecuteLibraryBinding(Binding, InViewModelOrWidget);

				UE::MVVM::Private::RecursiveDetector.Pop();
			}
		}
		else
		{
			ensureMsgf(false, TEXT("We should not have registered the binding since it will always be executed."));
		}
	}
}


void UMVVMView::ExecuteDelayedBinding(const FMVVMViewDelayedBinding& DelayedBinding) const
{
	if (ensure(ClassExtension))
	{
		if (ensure(ClassExtension->GetCompiledBindings().IsValidIndex(DelayedBinding.GetCompiledBindingIndex())))
		{
			if (IsLibraryBindingEnabled(DelayedBinding.GetCompiledBindingIndex()))
			{
				const FMVVMViewClass_CompiledBinding& Binding = ClassExtension->GetCompiledBinding(DelayedBinding.GetCompiledBindingIndex());

				// Test for recursivity
				int32 CompiledBindingIndex = DelayedBinding.GetCompiledBindingIndex();
				const UMVVMView* Self = this;
				if (UE::MVVM::Private::RecursiveDetector.FindByPredicate([Self, CompiledBindingIndex](const UE::MVVM::Private::FRecursiveDetctionElement& Element)
					{
						return Element.Get<0>() == Self && Element.Get<1>() == CompiledBindingIndex;
					}) != nullptr)
				{
					ensureAlwaysMsgf(false, TEXT("Recursive binding detected"));
					//Todo add more infos. Callstack maybe? Log the chain?
					UE::MVVM::FMessageLog Log(GetUserWidget());
					Log.Error(LOCTEXT("RecursionDetected", "A recursive binding was detected (ie. A->B->C->A->B->C) at runtime."));
					return;
				}

				{
					UE::MVVM::Private::RecursiveDetector.Emplace(this, CompiledBindingIndex);

					ExecuteLibraryBinding(Binding);

					UE::MVVM::Private::RecursiveDetector.Pop();
				}
			}
		}
	}
}


void UMVVMView::ExecuteEveryTickBindings() const
{
	ensure(bHasEveryTickBinding);

	if (ClassExtension)
	{
		const TArrayView<const FMVVMViewClass_CompiledBinding> CompiledBindings = ClassExtension->GetCompiledBindings();

		for (int32 Index = 0; Index < CompiledBindings.Num(); ++Index)
		{
			const FMVVMViewClass_CompiledBinding& Binding = CompiledBindings[Index];
			if (Binding.GetExecuteMode() == EMVVMExecutionMode::Tick && IsLibraryBindingEnabled(Index))
			{
				ExecuteLibraryBinding(Binding);
			}
		}
	}
}


void UMVVMView::ExecuteLibraryBinding(const FMVVMViewClass_CompiledBinding& Binding) const
{
	check(ClassExtension);
	check(GetUserWidget());

	FMVVMCompiledBindingLibrary::EConversionFunctionType FunctionType = Binding.IsConversionFunctionComplex() ? FMVVMCompiledBindingLibrary::EConversionFunctionType::Complex : FMVVMCompiledBindingLibrary::EConversionFunctionType::Simple;
	TValueOrError<void, FMVVMCompiledBindingLibrary::EExecutionFailingReason> ExecutionResult = ClassExtension->GetBindingLibrary().Execute(GetUserWidget(), Binding.GetBinding(), FunctionType);

#if UE_WITH_MVVM_DEBUGGING
	if (ExecutionResult.HasError())
	{
		UE::MVVM::FDebugging::BroadcastLibraryBindingExecuted(this, Binding, ExecutionResult.GetError());
	}
	else
	{
		UE::MVVM::FDebugging::BroadcastLibraryBindingExecuted(this, Binding);
	}
#endif

	if (ExecutionResult.HasError())
	{
		UE::MVVM::FMessageLog Log(GetUserWidget());
		Log.Error(FText::Format(LOCTEXT("ExecuteBindingGenericExecute", "The binding '{0}' was not executed. {1}."), FText::FromString(Binding.ToString()), LOCTEXT("todo", "Reason is todo.")));
	}
	else if (bLogBinding)
	{
		UE::MVVM::FMessageLog Log(GetUserWidget());
		Log.Info(FText::Format(LOCTEXT("ExecuteLibraryBinding", "Execute binding '{0}'."), FText::FromString(Binding.ToString())));
	}
}


void UMVVMView::ExecuteLibraryBinding(const FMVVMViewClass_CompiledBinding& Binding, UObject* Source) const
{
	check(ClassExtension);
	check(GetUserWidget());
	check(Source);

	TValueOrError<void, FMVVMCompiledBindingLibrary::EExecutionFailingReason> ExecutionResult = Binding.IsConversionFunctionComplex()
		? ClassExtension->GetBindingLibrary().Execute(GetUserWidget(), Binding.GetBinding(), FMVVMCompiledBindingLibrary::EConversionFunctionType::Complex)
		: ClassExtension->GetBindingLibrary().ExecuteWithSource(GetUserWidget(), Binding.GetBinding(), Source);

#if UE_WITH_MVVM_DEBUGGING
	if (ExecutionResult.HasError())
	{
		UE::MVVM::FDebugging::BroadcastLibraryBindingExecuted(this, Binding, ExecutionResult.GetError());
	}
	else
	{
		UE::MVVM::FDebugging::BroadcastLibraryBindingExecuted(this, Binding);
	}
#endif
	if (ExecutionResult.HasError())
	{
		UE::MVVM::FMessageLog Log(GetUserWidget());
		Log.Error(FText::Format(LOCTEXT("ExecuteBindingGenericExecute", "The binding '{0}' was not executed. {1}."), FText::FromString(Binding.ToString()), LOCTEXT("todo", "Reason is todo.")));
	}
	else if (bLogBinding)
	{
		UE::MVVM::FMessageLog Log(GetUserWidget());
		Log.Info(FText::Format(LOCTEXT("ExecuteLibraryBinding", "Execute binding '{0}'."), FText::FromString(Binding.ToString())));
	}
}


//void UMVVMView::SetLibraryBindingEnabled(FGuid ViewModelId, FMVVMBindingName BindingName, bool bEnable)
//{
//	if (ensure(ClassExtension))
//	{
//		int32 LibraryBindingIndex = ClassExtension->IndexOfCompiledBinding(ViewModelId, BindingName);
//		if (LibraryBindingIndex != INDEX_NONE)
//		{
//			const bool bIsEnabled = IsLibraryBindingEnabled(LibraryBindingIndex);
//			if (bIsEnabled != bEnable)
//			{
//				const FMVVMViewClass_CompiledBinding& Item = ClassExtension->GetCompiledBindings(LibraryBindingIndex);
//				if (bEnable)
//				{
//					EnableLibraryBinding(Item, LibraryBindingIndex);
//				}
//				else
//				{
//					DisableLibraryBinding(Item, LibraryBindingIndex);
//				}
//			}
//		}
//	}
//}


//bool UMVVMView::IsLibraryBindingEnabled(FGuid ViewModelId, FMVVMBindingName BindingName) const
//{
//	if (ensure(ClassExtension))
//	{
//		int32 LibraryBindingIndex = ClassExtension->IndexOfCompiledBinding(ViewModelId, BindingName);
//		return IsLibraryBindingEnabled(LibraryBindingIndex);
//	}
//	return false;
//}


bool UMVVMView::IsLibraryBindingEnabled(int32 InBindindIndex) const
{
	return EnabledLibraryBindings.IsValidIndex(InBindindIndex) && EnabledLibraryBindings[InBindindIndex];
}


void UMVVMView::EnableLibraryBinding(const FMVVMViewClass_CompiledBinding& Binding, int32 BindingIndex)
{
	EnabledLibraryBindings.PadToNum(BindingIndex + 1, false);
	check(EnabledLibraryBindings[BindingIndex] == false);

	EMVVMExecutionMode ExecutionMode = Binding.GetExecuteMode();
	bool bRegistered = true;
	if (!Binding.IsOneTime() && ExecutionMode != EMVVMExecutionMode::Tick)
	{
		bRegistered = RegisterLibraryBinding(Binding, BindingIndex);
	}

	if (bRegistered && bLogBinding)
	{
		UE::MVVM::FMessageLog Log(GetUserWidget());
		Log.Info(FText::Format(LOCTEXT("EnableLibraryBinding", "Enable binding '{0}'."), FText::FromString(Binding.ToString())));
	}

	EnabledLibraryBindings[BindingIndex] = bRegistered;

	if (bRegistered && Binding.NeedsExecutionAtInitialization())
	{
		ExecuteLibraryBinding(Binding);
	}

	bHasEveryTickBinding = bHasEveryTickBinding || ExecutionMode == EMVVMExecutionMode::Tick;
}


void UMVVMView::DisableLibraryBinding(const FMVVMViewClass_CompiledBinding& Binding, int32 BindingIndex)
{
	check(IsLibraryBindingEnabled(BindingIndex));


	EMVVMExecutionMode ExecutionMode = Binding.GetExecuteMode();
	EnabledLibraryBindings[BindingIndex] = false;
	if (!Binding.IsOneTime() && ExecutionMode != EMVVMExecutionMode::Tick)
	{
		UnregisterLibraryBinding(Binding);
	}

	if (bLogBinding)
	{
		UE::MVVM::FMessageLog Log(GetUserWidget());
		Log.Info(FText::Format(LOCTEXT("DisableLibraryBinding", "Disable binding '{0}'."), FText::FromString(Binding.ToString())));
	}
}


bool UMVVMView::RegisterLibraryBinding(const FMVVMViewClass_CompiledBinding& Binding, int32 BindingIndex)
{
	check(ClassExtension);

	TValueOrError<UE::FieldNotification::FFieldId, void> FieldIdResult = ClassExtension->GetBindingLibrary().GetFieldId(Binding.GetSourceFieldId());
	if (FieldIdResult.HasError())
	{
#if UE_WITH_MVVM_DEBUGGING
		UE::MVVM::FDebugging::BroadcastLibraryBindingRegistered(this, Binding, UE::MVVM::FDebugging::ERegisterLibraryBindingResult::Failed_InvalidFieldId);
#endif
		UE_LOG(LogMVVM, Error, TEXT("'%s' can't register binding '%s'. The FieldId is invalid."), *GetFullName(), *Binding.ToString());
		return false;
	}

	UE::FieldNotification::FFieldId FieldId = FieldIdResult.StealValue();
	if (!FieldId.IsValid())
	{
#if UE_WITH_MVVM_DEBUGGING
		UE::MVVM::FDebugging::BroadcastLibraryBindingRegistered(this, Binding, UE::MVVM::FDebugging::ERegisterLibraryBindingResult::Failed_FieldIdNotFound);
#endif
		UE_LOG(LogMVVM, Error, TEXT("'%s' can't register binding '%s'. The FieldId was not found on the source."), *GetFullName(), *Binding.ToString());
		return false;
	}

	// The source may not have been created because the property path was wrong.
	TScriptInterface<INotifyFieldValueChanged> Source = FindSource(Binding, true);
	if (Source.GetInterface() == nullptr)
	{
#if UE_WITH_MVVM_DEBUGGING
		UE::MVVM::FDebugging::BroadcastLibraryBindingRegistered(this, Binding, UE::MVVM::FDebugging::ERegisterLibraryBindingResult::Failed_InvalidSource);
#endif
		if (!Binding.IsRegistrationOptional())
		{
			UE_LOG(LogMVVM, Error, TEXT("'%s' can't register binding '%s'. The source is invalid."), *GetFullName(), *Binding.ToString());
		}
		return false;
	}

	// Only bind if the source and the destination are valid.
	if (ClassExtension->GetBindingLibrary().EvaluateFieldPath(GetUserWidget(), Binding.GetBinding().GetSourceFieldPath()).HasError())
	{
#if UE_WITH_MVVM_DEBUGGING
		UE::MVVM::FDebugging::BroadcastLibraryBindingRegistered(this, Binding, UE::MVVM::FDebugging::ERegisterLibraryBindingResult::Failed_InvalidSourceField);
#endif
		UE_LOG(LogMVVM, Warning, TEXT("'%s' can't register binding '%s'. The destination was not evaluated."), *GetFullName(), *Binding.ToString());
		return false;
	}
	if (ClassExtension->GetBindingLibrary().EvaluateFieldPath(GetUserWidget(), Binding.GetBinding().GetDestinationFieldPath()).HasError())
	{
#if UE_WITH_MVVM_DEBUGGING
		UE::MVVM::FDebugging::BroadcastLibraryBindingRegistered(this, Binding, UE::MVVM::FDebugging::ERegisterLibraryBindingResult::Failed_InvalidDestinationField);
#endif
		UE_LOG(LogMVVM, Warning, TEXT("'%s' can't register binding '%s'. The destination was not evaluated."), *GetFullName(), *Binding.ToString());
		return false;
	}


	UE::FieldNotification::FFieldMulticastDelegate::FDelegate Delegate = UE::FieldNotification::FFieldMulticastDelegate::FDelegate::CreateUObject(this, &UMVVMView::HandledLibraryBindingValueChanged, BindingIndex);
	bool bResult = Source->AddFieldValueChangedDelegate(FieldId, MoveTemp(Delegate)).IsValid();
	if (bResult)
	{
		FRegisteredSource* FoundSource = AllSources.FindByPredicate([&Source](const FRegisteredSource& Other) { return Other.Source == Source.GetObject(); });
		if (FoundSource)
		{
			++FoundSource->Count;
		}
		else
		{
			FRegisteredSource Item;
			Item.Source = Source.GetObject();
			Item.Count = 1;
			AllSources.Add(MoveTemp(Item));
		}
	}
#if UE_WITH_MVVM_DEBUGGING
	UE::MVVM::FDebugging::BroadcastLibraryBindingRegistered(this, Binding, UE::MVVM::FDebugging::ERegisterLibraryBindingResult::Success);
#endif
	return bResult;
}


void UMVVMView::UnregisterLibraryBinding(const FMVVMViewClass_CompiledBinding& Binding)
{
	TScriptInterface<INotifyFieldValueChanged> Source = FindSource(Binding, true);
	if (Source.GetInterface() && ClassExtension)
	{
		TValueOrError<UE::FieldNotification::FFieldId, void> FieldIdResult = ClassExtension->GetBindingLibrary().GetFieldId(Binding.GetSourceFieldId());
		if (ensureMsgf(FieldIdResult.HasValue() && FieldIdResult.GetValue().IsValid(), TEXT("If the binding was enabled then the FieldId should exist.")))
		{
			Source->RemoveAllFieldValueChangedDelegates(FieldIdResult.GetValue(), this);
		}

		int32 FoundIndex = AllSources.IndexOfByPredicate([&Source](const FRegisteredSource& Other) { return Other.Source == Source.GetObject(); });
		if (ensureMsgf(FoundIndex != INDEX_NONE, TEXT("If the binding was enabled, then the source should also be there.")))
		{
			FRegisteredSource& FoundSource = AllSources[FoundIndex];
			ensureMsgf(FoundSource.Count > 0, TEXT("The count should match the number of RegisterLibraryBinding and UnregisterLibraryBinding"));
			--FoundSource.Count;
			if (FoundSource.Count == 0)
			{
				AllSources.RemoveAtSwap(FoundIndex);
			}
		}
	}
}


TScriptInterface<INotifyFieldValueChanged> UMVVMView::FindSource(const FMVVMViewClass_CompiledBinding& Binding, bool bAllowNull) const
{
	UUserWidget* UserWidget = GetUserWidget();
	check(UserWidget);
	check(ClassExtension);

	if (Binding.IsSourceObjectItself())
	{
		return TScriptInterface<INotifyFieldValueChanged>(UserWidget);
	}
	else
	{
		const FObjectPropertyBase* SourceObjectProperty = CastField<FObjectPropertyBase>(UserWidget->GetClass()->FindPropertyByName(Binding.GetSourceObjectPropertyName()));
		if (SourceObjectProperty == nullptr)
		{
			UE_LOG(LogMVVM, Error, TEXT("'%s' could not evaluate the source for binding '%s'. The property name is invalid."), *GetFullName(), *Binding.ToString());
			return TScriptInterface<INotifyFieldValueChanged>();
		}

		UObject* Source = SourceObjectProperty->GetObjectPropertyValue_InContainer(UserWidget);
		if (Source == nullptr)
		{
			if (!bAllowNull)
			{
				UE_LOG(LogMVVM, Error, TEXT("'%s' could not evaluate the source for binding '%s'. The path point to an invalid object."), *GetFullName(), *Binding.ToString());
			}
			return TScriptInterface<INotifyFieldValueChanged>();
		}

		if (!Source->Implements<UNotifyFieldValueChanged>())
		{
			UE_LOG(LogMVVM, Error, TEXT("'%s' could not evaluate the source for binding '%s'. The object '%s' doesn't implements INotifyFieldValueChanged."), *GetFullName(), *Binding.ToString(), *Source->GetFName().ToString());
			return TScriptInterface<INotifyFieldValueChanged>();
		}

		return TScriptInterface<INotifyFieldValueChanged>(Source);
	}
}


#undef LOCTEXT_NAMESPACE

