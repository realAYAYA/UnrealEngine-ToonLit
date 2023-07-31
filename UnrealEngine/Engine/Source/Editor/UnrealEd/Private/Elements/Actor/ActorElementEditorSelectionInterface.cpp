// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Actor/ActorElementEditorSelectionInterface.h"

#include "Elements/Actor/ActorElementData.h"
#include "Elements/Framework/EngineElementsLibrary.h"
#include "Elements/Framework/TypedElementHandle.h"
#include "Elements/Framework/TypedElementList.h"
#include "Elements/Interfaces/TypedElementSelectionInterface.h"
#include "Elements/Object/ObjectElementEditorSelectionInterface.h"
#include "GameFramework/Actor.h"
#include "Serialization/Archive.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FActorElementTransactedElement : public ITypedElementTransactedElement
{
private:
	virtual TUniquePtr<ITypedElementTransactedElement> CloneImpl() const override
	{
		return MakeUnique<FActorElementTransactedElement>(*this);
	}

	virtual FTypedElementHandle GetElementImpl() const override
	{
		const AActor* Actor = ActorPtr.Get(/*bEvenIfPendingKill*/true);
		return Actor
			? UEngineElementsLibrary::AcquireEditorActorElementHandle(Actor)
			: FTypedElementHandle();
	}

	virtual void SetElementImpl(const FTypedElementHandle& InElementHandle) override
	{
		AActor* Actor = ActorElementDataUtil::GetActorFromHandleChecked(InElementHandle);
		ActorPtr = Actor;
	}

	virtual void SerializeImpl(FArchive& InArchive) override
	{
		InArchive << ActorPtr;
	}

	TWeakObjectPtr<AActor> ActorPtr;
};

bool UActorElementEditorSelectionInterface::IsElementSelected(const FTypedElementHandle& InElementHandle, const FTypedElementListConstPtr& SelectionSetPtr, const FTypedElementIsSelectedOptions& InSelectionOptions)
{
	const AActor* Actor = ActorElementDataUtil::GetActorFromHandle(InElementHandle);
	return SelectionSetPtr && Actor && IsActorSelected(Actor, SelectionSetPtr.ToSharedRef(), InSelectionOptions);
}

bool UActorElementEditorSelectionInterface::ShouldPreventTransactions(const FTypedElementHandle& InElementHandle)
{
	const AActor* Actor = ActorElementDataUtil::GetActorFromHandle(InElementHandle);
	return Actor && UObjectElementEditorSelectionInterface::ShouldObjectPreventTransactions(Actor);
}

TUniquePtr<ITypedElementTransactedElement> UActorElementEditorSelectionInterface::CreateTransactedElementImpl()
{
	return MakeUnique<FActorElementTransactedElement>();
}

bool UActorElementEditorSelectionInterface::IsActorSelected(const AActor* InActor, FTypedElementListConstRef InSelectionSet, const FTypedElementIsSelectedOptions& InSelectionOptions)
{
	if (InSelectionSet->Num() == 0)
	{
		return false;
	}

	if (FTypedElementHandle ActorElement = UEngineElementsLibrary::AcquireEditorActorElementHandle(InActor, /*bAllowCreate*/false))
	{
		if (InSelectionSet->Contains(ActorElement))
		{
			return true;
		}
	}

	if (InSelectionOptions.AllowIndirect())
	{
		if (const AActor* RootSelectionActor = InActor->GetRootSelectionParent())
		{
			if (FTypedElementHandle RootSelectionElement = UEngineElementsLibrary::AcquireEditorActorElementHandle(RootSelectionActor, /*bAllowCreate*/false))
			{
				return InSelectionSet->Contains(RootSelectionElement);
			}
		}
	}

	return false;
}
