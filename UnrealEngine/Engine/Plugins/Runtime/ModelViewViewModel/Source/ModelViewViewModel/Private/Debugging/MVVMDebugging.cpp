// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debugging/MVVMDebugging.h"

#if UE_WITH_MVVM_DEBUGGING

#include "View/MVVMView.h"

#define LOCTEXT_NAMESPACE "MVVMDebugging"

namespace UE::MVVM
{
/** */
FDebugging::FView::FView(const UMVVMView* InView)
	: UserWidget(InView->GetUserWidget())
	, View(InView)
{
}

FDebugging::FView::FView(const UUserWidget* InUserWidget, const UMVVMView* InView)
	: UserWidget(InUserWidget)
	, View(InView)
{
}

/** */
FDebugging::FViewConstructed FDebugging::OnViewConstructed;

void FDebugging::BroadcastViewConstructed(const UMVVMView* InView)
{
	OnViewConstructed.Broadcast(InView, FViewConstructedArgs());
}

/** */
FDebugging::FViewDestructing FDebugging::OnViewBeginDestruction;

void FDebugging::BroadcastViewBeginDestruction(const UMVVMView* InView)
{
	OnViewBeginDestruction.Broadcast(InView, FViewBeginDestructionArgs());
}

/** */
FDebugging::FLibraryBindingRegisteredArgs::FLibraryBindingRegisteredArgs(const FMVVMViewClass_CompiledBinding& InBinding, ERegisterLibraryBindingResult InResult)
	: Binding(InBinding)
	, Result(InResult)
{
}

FDebugging::FLibraryBindingRegistered FDebugging::OnLibraryBindingRegistered;

void FDebugging::BroadcastLibraryBindingRegistered(const UMVVMView* InView, const FLibraryBindingRegisteredArgs& Args)
{
	check(InView);
	OnLibraryBindingRegistered.Broadcast(FView(InView), Args);
}

void FDebugging::BroadcastLibraryBindingRegistered(const UMVVMView* InView, const FMVVMViewClass_CompiledBinding& InBinding, ERegisterLibraryBindingResult InResult)
{
	if (OnLibraryBindingRegistered.IsBound())
	{
		check(InView);
		OnLibraryBindingRegistered.Broadcast(FView(InView), FLibraryBindingRegisteredArgs(InBinding, InResult));
	}
}

/** */
FDebugging::FLibraryBindingExecutedArgs::FLibraryBindingExecutedArgs(const FMVVMViewClass_CompiledBinding& InBinding)
	: Binding(InBinding)
{
}

FDebugging::FLibraryBindingExecutedArgs::FLibraryBindingExecutedArgs(const FMVVMViewClass_CompiledBinding& InBinding, FMVVMCompiledBindingLibrary::EExecutionFailingReason InResult)
	: Binding(InBinding)
	, FailingReason(InResult)
{
}

FDebugging::FLibraryBindingExecuted FDebugging::OnLibraryBindingExecuted;

void FDebugging::BroadcastLibraryBindingExecuted(const UMVVMView* InView, const FLibraryBindingExecutedArgs& Args)
{
	check(InView);
	OnLibraryBindingExecuted.Broadcast(FView(InView), Args);
}

void FDebugging::BroadcastLibraryBindingExecuted(const UMVVMView* InView, const FMVVMViewClass_CompiledBinding& InBinding)
{
	if (OnLibraryBindingExecuted.IsBound())
	{
		check(InView);
		OnLibraryBindingExecuted.Broadcast(FView(InView), FLibraryBindingExecutedArgs(InBinding));
	}
}

void FDebugging::BroadcastLibraryBindingExecuted(const UMVVMView* InView, const FMVVMViewClass_CompiledBinding& InBinding, FMVVMCompiledBindingLibrary::EExecutionFailingReason InResult)
{
	if (OnLibraryBindingExecuted.IsBound())
	{
		check(InView);
		OnLibraryBindingExecuted.Broadcast(FView(InView), FLibraryBindingExecutedArgs(InBinding, InResult));
	}
}
} // namespace UE::MVVM

#undef LOCTEXT_NAMESPACE

#endif //UE_WITH_MVVM_DEBUGGING
