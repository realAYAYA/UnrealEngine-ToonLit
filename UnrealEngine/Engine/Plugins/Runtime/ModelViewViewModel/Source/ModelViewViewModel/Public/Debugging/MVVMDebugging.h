// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h" // IWYU pragma: keep

#ifndef UE_WITH_MVVM_DEBUGGING
#define UE_WITH_MVVM_DEBUGGING !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#endif


#if UE_WITH_MVVM_DEBUGGING

#include "Bindings/MVVMCompiledBindingLibrary.h"

class UMVVMView;
struct FMVVMViewClass_CompiledBinding;
class UUserWidget;

namespace UE::MVVM
{
/** */
class MODELVIEWVIEWMODEL_API FDebugging
{
public:
	struct FView
	{
	public:
		FView() = default;
		FView(const UMVVMView* View);
		FView(const UUserWidget* UserWidget, const UMVVMView* View);

	public:
		const UUserWidget* GetUserWidget() const
		{
			return UserWidget;
		}

		const UMVVMView* GetView() const
		{
			return View;
		}
	private:
		const UUserWidget* UserWidget = nullptr;
		const UMVVMView* View = nullptr;
	};
public:
	struct FViewConstructedArgs
	{
	};

	DECLARE_EVENT_TwoParams(FDebugging, FViewConstructed, const FView&, const FViewConstructedArgs&);
	/** Broadcast when a view is created and the viewmodels are instantiated and the binding are not registered. */
	static FViewConstructed OnViewConstructed;
	static void BroadcastViewConstructed(const UMVVMView* View);


	struct FViewBeginDestructionArgs
	{
	};

	DECLARE_EVENT_TwoParams(FDebugging, FViewDestructing, const FView&, const FViewBeginDestructionArgs&);
	/** Broadcast before a view is destroyed. */
	static FViewDestructing OnViewBeginDestruction;
	static void BroadcastViewBeginDestruction(const UMVVMView* View);

public:
	enum class ERegisterLibraryBindingResult : uint8
	{
		Success,
		Failed_InvalidFieldId,
		Failed_FieldIdNotFound,
		Failed_InvalidSource,
		Failed_InvalidSourceField,
		Failed_Unknown,
	};

	struct FLibraryBindingRegisteredArgs
	{
		FLibraryBindingRegisteredArgs() = delete;
		FLibraryBindingRegisteredArgs(const FMVVMViewClass_CompiledBinding& Binding, ERegisterLibraryBindingResult Result);
		const FMVVMViewClass_CompiledBinding& Binding;
		ERegisterLibraryBindingResult Result;
	};

	DECLARE_EVENT_TwoParams(FDebugging, FLibraryBindingRegistered, const FView&, const FLibraryBindingRegisteredArgs&);
	/** Broadcast when a binding is registered to the view/viewmodel or failed to registered to the view/viewmodel. */
	static FLibraryBindingRegistered OnLibraryBindingRegistered;
	static void BroadcastLibraryBindingRegistered(const UMVVMView* View, const FLibraryBindingRegisteredArgs& Args);
	static void BroadcastLibraryBindingRegistered(const UMVVMView* View, const FMVVMViewClass_CompiledBinding& Binding, ERegisterLibraryBindingResult Result);

public:
	struct FLibraryBindingExecutedArgs
	{
		FLibraryBindingExecutedArgs() = delete;
		FLibraryBindingExecutedArgs(const FMVVMViewClass_CompiledBinding& Binding);
		FLibraryBindingExecutedArgs(const FMVVMViewClass_CompiledBinding& Binding, FMVVMCompiledBindingLibrary::EExecutionFailingReason Result);
		const FMVVMViewClass_CompiledBinding& Binding;
		TOptional<FMVVMCompiledBindingLibrary::EExecutionFailingReason> FailingReason;
	};

	/**  */
	DECLARE_EVENT_TwoParams(FDebugging, FLibraryBindingExecuted, const FView&, const FLibraryBindingExecutedArgs&);
	/** Broadcast when a registered field is modified and a binding need to execute. */
	static FLibraryBindingExecuted OnLibraryBindingExecuted;
	static void BroadcastLibraryBindingExecuted(const UMVVMView* View, const FLibraryBindingExecutedArgs& Args);
	static void BroadcastLibraryBindingExecuted(const UMVVMView* View, const FMVVMViewClass_CompiledBinding& Binding);
	static void BroadcastLibraryBindingExecuted(const UMVVMView* View, const FMVVMViewClass_CompiledBinding& Binding, FMVVMCompiledBindingLibrary::EExecutionFailingReason Result);
};
} // UE::MVVM

#else


namespace UE::MVVM
{
class FDebugging
{
};
} // UE::MVVM

#endif

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
