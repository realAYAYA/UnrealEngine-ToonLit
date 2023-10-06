// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Actor/ActorElementDetailsInterface.h"

#include "Elements/Actor/ActorElementData.h"
#include "GameFramework/Actor.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FActorTypedElementDetailsObject : public ITypedElementDetailsObject
{
public:
	explicit FActorTypedElementDetailsObject(AActor* InActor)
		: ActorPtr(InActor)
	{
	}

	virtual UObject* GetObject() override
	{
		return ActorPtr.Get();
	}

private:
	TWeakObjectPtr<AActor> ActorPtr;
};

TUniquePtr<ITypedElementDetailsObject> UActorElementDetailsInterface::GetDetailsObject(const FTypedElementHandle& InElementHandle)
{
	if (AActor* Actor = ActorElementDataUtil::GetActorFromHandle(InElementHandle))
	{
		return MakeUnique<FActorTypedElementDetailsObject>(Actor);
	}
	return nullptr;
}
