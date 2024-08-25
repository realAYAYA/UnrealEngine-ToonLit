// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFramework/ActorPrimitiveColorHandler.h"
#include "GameFramework/Actor.h"
#include "Misc/LazySingleton.h"
#include "EngineUtils.h"

#define LOCTEXT_NAMESPACE "ActorColoration"

FActorPrimitiveColorHandler::FActorPrimitiveColorHandler()
{
#if ENABLE_ACTOR_PRIMITIVE_COLOR_HANDLER
	RegisterPrimitiveColorHandler(NAME_None, LOCTEXT("Disable", "Disable"), [](const UPrimitiveComponent*)
	{
		return FLinearColor::White;
	});

	ActivePrimitiveColorHandlerName = NAME_None;
	ActivePrimitiveColorHandler = Handlers.Find(NAME_None);
#endif
}

FActorPrimitiveColorHandler& FActorPrimitiveColorHandler::Get()
{
	return TLazySingleton<FActorPrimitiveColorHandler>::Get();
}

void FActorPrimitiveColorHandler::RegisterPrimitiveColorHandler(FName InHandlerName, FText InHandlerText, const FGetColorFunc& InGetColorFunc, const FActivateFunc& InActivateFunc)
{
#if ENABLE_ACTOR_PRIMITIVE_COLOR_HANDLER
	check(!Handlers.Contains(InHandlerName));
	Handlers.Add(InHandlerName, { InHandlerName, InHandlerText, InGetColorFunc, InActivateFunc });
	ActivePrimitiveColorHandler = &Handlers.FindChecked(ActivePrimitiveColorHandlerName);
#endif
}

void FActorPrimitiveColorHandler::UnregisterPrimitiveColorHandler(FName InHandlerName)
{
#if ENABLE_ACTOR_PRIMITIVE_COLOR_HANDLER
	check(!InHandlerName.IsNone());
	check(Handlers.Contains(InHandlerName));
	Handlers.Remove(InHandlerName);
	
	if (InHandlerName == ActivePrimitiveColorHandlerName)
	{
		ActivePrimitiveColorHandlerName = NAME_None;
	}

	ActivePrimitiveColorHandler = Handlers.Find(ActivePrimitiveColorHandlerName);
#endif
}

bool FActorPrimitiveColorHandler::SetActivePrimitiveColorHandler(FName InHandlerName, UWorld* InWorld)
{
#if ENABLE_ACTOR_PRIMITIVE_COLOR_HANDLER
	if (FPrimitiveColorHandler* NewActivePrimitiveColorHandler = Handlers.Find(InHandlerName); NewActivePrimitiveColorHandler && (NewActivePrimitiveColorHandler != ActivePrimitiveColorHandler))
	{
		ActivePrimitiveColorHandlerName = InHandlerName;
		ActivePrimitiveColorHandler = NewActivePrimitiveColorHandler;
		NewActivePrimitiveColorHandler->ActivateFunc();
		RefreshPrimitiveColorHandler(InHandlerName, InWorld);
		return true;
	}
#endif

	return false;
}

void FActorPrimitiveColorHandler::RefreshPrimitiveColorHandler(FName InHandlerName, UWorld* InWorld)
{
#if ENABLE_ACTOR_PRIMITIVE_COLOR_HANDLER
	if (ActivePrimitiveColorHandlerName == InHandlerName)
	{
		for (TActorIterator<AActor> It(InWorld); It; ++It)
		{
			It->ForEachComponent<UPrimitiveComponent>(false, [this](UPrimitiveComponent* PrimitiveComponent)
			{
				if (PrimitiveComponent->IsRegistered())
				{
					PrimitiveComponent->PushPrimitiveColorToProxy(GetPrimitiveColor(PrimitiveComponent));
				}
			});
		}
	}
#endif
}

void FActorPrimitiveColorHandler::RefreshPrimitiveColorHandler(FName InHandlerName, const TArray<AActor*>& InActors)
{
#if ENABLE_ACTOR_PRIMITIVE_COLOR_HANDLER
	if (ActivePrimitiveColorHandlerName == InHandlerName)
	{
		for (AActor* Actor : InActors)
		{
			Actor->ForEachComponent<UPrimitiveComponent>(false, [this](UPrimitiveComponent* PrimitiveComponent)
			{
				if (PrimitiveComponent->IsRegistered())
				{
					PrimitiveComponent->PushPrimitiveColorToProxy(GetPrimitiveColor(PrimitiveComponent));
				}
			});
		}
	}
#endif
}

void FActorPrimitiveColorHandler::RefreshPrimitiveColorHandler(FName InHandlerName, const TArray<UPrimitiveComponent*>& InPrimitiveComponents)
{
#if ENABLE_ACTOR_PRIMITIVE_COLOR_HANDLER
	if (ActivePrimitiveColorHandlerName == InHandlerName)
	{
		for (UPrimitiveComponent* PrimitiveComponent : InPrimitiveComponents)
		{
			if (PrimitiveComponent && PrimitiveComponent->IsRegistered())
			{
				PrimitiveComponent->PushPrimitiveColorToProxy(GetPrimitiveColor(PrimitiveComponent));
			}
		}
	}
#endif
}

FName FActorPrimitiveColorHandler::GetActivePrimitiveColorHandler() const
{
#if ENABLE_ACTOR_PRIMITIVE_COLOR_HANDLER
	return ActivePrimitiveColorHandlerName;
#else
	return NAME_None;
#endif
}

void FActorPrimitiveColorHandler::GetRegisteredPrimitiveColorHandlers(TArray<FPrimitiveColorHandler>& OutPrimitiveColorHandlers) const
{
#if ENABLE_ACTOR_PRIMITIVE_COLOR_HANDLER
	Handlers.GenerateValueArray(OutPrimitiveColorHandlers);
#endif
}

FLinearColor FActorPrimitiveColorHandler::GetPrimitiveColor(const UPrimitiveComponent* InPrimitiveComponent) const
{
#if ENABLE_ACTOR_PRIMITIVE_COLOR_HANDLER
	return ActivePrimitiveColorHandler->GetColorFunc(InPrimitiveComponent);
#else
	return FLinearColor::White;
#endif
}

#undef LOCTEXT_NAMESPACE