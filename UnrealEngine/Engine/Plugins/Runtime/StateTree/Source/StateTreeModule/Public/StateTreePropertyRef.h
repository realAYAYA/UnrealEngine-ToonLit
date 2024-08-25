// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeIndexTypes.h"
#include "StateTreeExecutionContext.h"
#include "StateTreePropertyRef.generated.h"

struct FStateTreePropertyRef;

namespace UE::StateTree::PropertyRefHelpers
{
    /**
	 * @param PropertyRef Property's reference to get pointer to.
	 * @param InstanceDataStorage Instance Data Storage
	 * @param ExecutionFrame Execution frame owning referenced property
	 * @param ParentExecutionFrame Parent of execution frame owning referenced property
	 * @return Pointer to referenced property if succeeded.
	 */
	template<class T>
	static T* GetMutablePtrToProperty(const FStateTreePropertyRef& PropertyRef, FStateTreeInstanceStorage& InstanceDataStorage, const FStateTreeExecutionFrame& ExecutionFrame, const FStateTreeExecutionFrame* ParentExecutionFrame)
	{
		const FStateTreePropertyBindings& PropertyBindings = ExecutionFrame.StateTree->GetPropertyBindings();
		if (const FStateTreePropertyAccess* PropertyAccess = PropertyBindings.GetPropertyAccess(PropertyRef))
		{
			// Passing empty ContextAndExternalDataViews, as PropertyRef is not allowed to point to context or external data.
			FStateTreeDataView SourceView = FStateTreeExecutionContext::GetDataView(InstanceDataStorage, nullptr, ParentExecutionFrame, ExecutionFrame, {}, PropertyAccess->SourceDataHandle);
			return PropertyBindings.GetMutablePropertyPtr<T>(SourceView, *PropertyAccess);
		}

		return nullptr;
	}
}

/**
 * Property ref allows to get a pointer to selected property in StateTree.
 * The expected type of the reference should be set in "RefType" meta specifier.
 *
 * Meta specifiers for the type:
 *  - RefType = "<type>"
 *		- Specifies the type of property to reference.
 *		- Supported types are: bool, byte, int32, int64, float, double, Name, String, Text, UObject pointers, and structs.
 *  - IsRefToArray
 *		- If specified, the reference is to an TArray<RefType>
 *  - Optional
 *		- If specified, the reference can be left unbound, otherwise the compiler report error if the reference is not bound.
 *
 * Example:
 *
 *  // Reference to float
 *	UPROPERTY(EditAnywhere, meta = (RefType = "float"))
 *	FStateTreePropertyRef RefToFloat;
 * 
 *  // Reference to FTestStructBase
 *	UPROPERTY(EditAnywhere, meta = (RefType = "/Script/ModuleName.TestStructBase"))
 *	FStateTreePropertyRef RefToTest;
 *
 *  // Reference to TArray<FTestStructBase>
 *	UPROPERTY(EditAnywhere, meta = (RefType = "/Script/ModuleName.TestStructBase", IsRefToArray))
 *	FStateTreePropertyRef RefToArrayOfTests;
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreePropertyRef
{
	GENERATED_BODY()

	FStateTreePropertyRef() = default;

	/** @return pointer to the property if possible, nullptr otherwise. */
	template<class T>
	T* GetMutablePtr(FStateTreeExecutionContext& Context) const
	{
		const FStateTreeExecutionFrame* CurrentlyProcessedFrame = Context.GetCurrentlyProcessedFrame();
		check(CurrentlyProcessedFrame);

		return UE::StateTree::PropertyRefHelpers::GetMutablePtrToProperty<T>(*this, Context.GetMutableInstanceData()->GetMutableStorage(), *CurrentlyProcessedFrame, Context.GetCurrentlyProcessedParentFrame());
	}

	/**
	 * Used internally.
	 * @return index to referenced property access
	 */
	FStateTreeIndex16 GetRefAccessIndex() const
	{
		return RefAccessIndex;
	}

private:
	UPROPERTY()
	FStateTreeIndex16 RefAccessIndex;

	friend FStateTreePropertyBindingCompiler;
};

/**
 * TStateTreePropertyRef is a type-safe FStateTreePropertyRef wrapper against the given type.
 * @note When used as a property, this automatically defines PropertyRef property meta-data.
 * 
 * Example:
 *
 *  // Reference to float
 *	UPROPERTY(EditAnywhere)
 *	TStateTreePropertyRef<float> RefToFloat;
 * 
 *  // Reference to FTestStructBase
 *	UPROPERTY(EditAnywhere)
 *	TStateTreePropertyRef<FTestStructBase> RefToTest;
 *
 *  // Reference to TArray<FTestStructBase>
 *	UPROPERTY(EditAnywhere)
 *	TStateTreePropertyRef<TArray<FTestStructBase>> RefToArrayOfTests;
 */
template<class TRef>
struct TStateTreePropertyRef
{
	/** @return pointer to the property if possible, nullptr otherwise. */
	TRef* GetMutablePtr(FStateTreeExecutionContext& Context) const
	{
		return PropertyRef.GetMutablePtr<TRef>(Context);
	}

	/**
	 * Used internally.
	 * @return internal property ref
	 */
	FStateTreePropertyRef GetInternalPropertyRef() const
	{
		return PropertyRef;
	}

private:
	FStateTreePropertyRef PropertyRef;
};

/**
 * External Handle allows to wrap-up property reference to make it accessible without having an access to StateTreeExecutionContext. Useful for capturing property reference in callbacks.
 */
template<class TRef>
struct TStateTreePropertyRefExternalHandle
{
	TStateTreePropertyRefExternalHandle(FStateTreePropertyRef InPropertyRef, FStateTreeExecutionContext& InContext)
		: WeakInstanceStorage(InContext.GetMutableInstanceData()->GetWeakMutableStorage())
		, WeakStateTree(InContext.GetCurrentlyProcessedFrame()->StateTree)
		, RootState(InContext.GetCurrentlyProcessedFrame()->RootState)
		, PropertyRef(InPropertyRef)
	{}

	TStateTreePropertyRefExternalHandle(TStateTreePropertyRef<TRef> InPropertyRef, FStateTreeExecutionContext& InContext)
		: TStateTreePropertyRefExternalHandle(InPropertyRef.GetInternalPropertyRef(), InContext)
	{}

	/** @return pointer to the property if possible, nullptr otherwise. */
	TRef* GetMutablePtr() const
	{
		if (!WeakInstanceStorage.IsValid())
		{
			return nullptr;
		}

		FStateTreeInstanceStorage& InstanceStorage = *WeakInstanceStorage.Pin();

		const TArray<FStateTreeExecutionFrame>& ActiveFrames = InstanceStorage.GetExecutionState().ActiveFrames;
		const int32 FrameIndex = ActiveFrames.IndexOfByPredicate([this](const FStateTreeExecutionFrame& Frame)
		{
			return Frame.RootState == RootState && Frame.StateTree == WeakStateTree;
		});

		if (FrameIndex == INDEX_NONE)
		{
			return nullptr;
		}

		const FStateTreeExecutionFrame& Frame = ActiveFrames[FrameIndex];
		const FStateTreeExecutionFrame* ParentFrame = FrameIndex > 0 ? &ActiveFrames[FrameIndex - 1] : nullptr;

		return UE::StateTree::PropertyRefHelpers::GetMutablePtrToProperty<TRef>(PropertyRef, InstanceStorage, Frame, ParentFrame);
	}

private:
	TWeakPtr<FStateTreeInstanceStorage> WeakInstanceStorage;
	TWeakObjectPtr<const UStateTree> WeakStateTree = nullptr;
	FStateTreeStateHandle RootState = FStateTreeStateHandle::Invalid;
	FStateTreePropertyRef PropertyRef;
};
