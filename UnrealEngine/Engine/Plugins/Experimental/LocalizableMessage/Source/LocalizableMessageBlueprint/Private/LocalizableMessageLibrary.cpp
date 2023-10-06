// Copyright Epic Games, Inc. All Rights Reserved.

#include "LocalizableMessageLibrary.h"

#include "ILocalizableMessageModule.h"
#include "Internationalization/Internationalization.h"
#include "LocalizableMessage.h"
#include "LocalizableMessageProcessor.h"
#include "LocalizationContext.h"

FText ULocalizableMessageLibrary::Conv_LocalizableMessageToText(UObject* WorldContextObject, const FLocalizableMessage& Message)
{
	FLocalizationContext LocContext(WorldContextObject);
	FLocalizableMessageProcessor& Processor = ILocalizableMessageModule::Get().GetLocalizableMessageProcessor();
	return Processor.Localize(Message, LocContext);
}

bool ULocalizableMessageLibrary::IsEmpty_LocalizableMessage(const FLocalizableMessage& Message)
{
	return Message.IsEmpty();
}

void ULocalizableMessageLibrary::Reset_LocalizableMessage(FLocalizableMessage& Message)
{
	Message.Reset();
}