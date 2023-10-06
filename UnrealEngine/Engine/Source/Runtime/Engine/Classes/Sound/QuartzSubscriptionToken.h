// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Templates/SharedPointer.h"

class UQuartzSubsystem;
class FQuartzTickableObject;
struct FQuartzTickableObjectsManager;

class FQuartzSubscriptionToken
{
public:
	ENGINE_API FQuartzSubscriptionToken();
	ENGINE_API ~FQuartzSubscriptionToken();
	
	ENGINE_API void Subscribe(FQuartzTickableObject* Subscriber, UQuartzSubsystem* QuartzSubsystem);
	ENGINE_API void Unsubscribe();

	ENGINE_API TSharedPtr<FQuartzTickableObjectsManager> GetTickableObjectManager();
	ENGINE_API bool IsSubscribed() const;
	
private:
	FQuartzTickableObject* SubscribingObject;
	TWeakPtr<FQuartzTickableObjectsManager> TickableObjectManagerPtr;
};
