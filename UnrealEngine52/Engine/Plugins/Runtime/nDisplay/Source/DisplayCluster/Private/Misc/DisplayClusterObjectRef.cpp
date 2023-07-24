// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/DisplayClusterObjectRef.h"

USceneComponent* FDisplayClusterSceneComponentRef::GetOrFindSceneComponent() const
{
	FScopeLock lock(&DataGuard);

	if (!IsDefinedSceneComponent())
	{
		return nullptr;
	}

	if (!IsSceneComponentValid())
	{
		ComponentPtr.Reset();

		AActor* Actor = GetOrFindSceneActor();
		if (Actor)
		{
			for (UActorComponent* ItActorComponent : Actor->GetComponents())
			{
				// Component can be null when a viewport is deleted and GC occurs.
				if (ItActorComponent && ItActorComponent->GetFName() == ComponentName)
				{
					USceneComponent* SceneComponent = Cast<USceneComponent>(ItActorComponent);
					if (SceneComponent)
					{
						ComponentPtr = TWeakObjectPtr<USceneComponent>(SceneComponent);
						return SceneComponent;
					}
				}
			}
			// Component not found. Actor structure changed??
		}
	}

	return ComponentPtr.Get();
}
