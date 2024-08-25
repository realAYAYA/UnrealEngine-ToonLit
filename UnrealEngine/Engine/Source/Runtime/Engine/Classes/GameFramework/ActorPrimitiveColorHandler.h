// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "Components/PrimitiveComponent.h"

class UWorld;
class UPrimitiveComponent;

#define ENABLE_ACTOR_PRIMITIVE_COLOR_HANDLER !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

/**
 * FActorPrimitiveColorHandler is a simple mechanism for custom actor coloration registration. Once an actor color
 * handler is registered, it can automatically be activated with the SHOW ACTORCOLORATION <HANDLERNAME> command.
 */
class ENGINE_API FActorPrimitiveColorHandler
{
	using FGetColorFunc = TFunction<FLinearColor(const UPrimitiveComponent*)>;
	using FActivateFunc = TFunction<void(void)>;

public:
	struct FPrimitiveColorHandler
	{
		FPrimitiveColorHandler(FName InHandlerName, FText InHandlerText, const FGetColorFunc& InGetColorFunc, const FActivateFunc& InActivateFunc)
			: HandlerName(InHandlerName)
			, HandlerText(InHandlerText)
			, GetColorFunc(InGetColorFunc)
			, ActivateFunc(InActivateFunc)
		{}

		FName HandlerName;
		FText HandlerText;
		FGetColorFunc GetColorFunc;
		FActivateFunc ActivateFunc;
	};	

	FActorPrimitiveColorHandler();
	static FActorPrimitiveColorHandler& Get();

	void RegisterPrimitiveColorHandler(FName InHandlerName, FText InHandlerText, const FGetColorFunc& InHandlerFunc, const FActivateFunc& InActivateFunc = []() {});
	void UnregisterPrimitiveColorHandler(FName InHandlerName);
	void GetRegisteredPrimitiveColorHandlers(TArray<FPrimitiveColorHandler>& OutPrimitiveColorHandlers) const;

	FName GetActivePrimitiveColorHandler() const;
	bool SetActivePrimitiveColorHandler(FName InHandlerName, UWorld* InWorld);

	void RefreshPrimitiveColorHandler(FName InHandlerName, UWorld* InWorld);
	void RefreshPrimitiveColorHandler(FName InHandlerName, const TArray<AActor*>& InActors);
	void RefreshPrimitiveColorHandler(FName InHandlerName, const TArray<UPrimitiveComponent*>& InPrimitiveComponents);

	FLinearColor GetPrimitiveColor(const UPrimitiveComponent* InPrimitiveComponent) const;

private:
#if ENABLE_ACTOR_PRIMITIVE_COLOR_HANDLER
	FName ActivePrimitiveColorHandlerName;
	FText ActivePrimitiveColorHandlerText;
	FPrimitiveColorHandler* ActivePrimitiveColorHandler;
	TMap<FName, FPrimitiveColorHandler> Handlers;
#endif
};