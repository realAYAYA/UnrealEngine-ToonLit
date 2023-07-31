// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/DeveloperSettings.h"
#include "ConversationInstance.h"

#include "ConversationSettings.generated.h"

/**
 * Conversation settings.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Conversation"))
class COMMONCONVERSATIONRUNTIME_API UConversationSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	UConversationSettings();

	UClass* GetConversationInstanceClass() const { return ConversationInstanceClass.LoadSynchronous(); }

protected:

	UPROPERTY(config, EditAnywhere, Category=Conversation)
	TSoftClassPtr<UConversationInstance> ConversationInstanceClass;
};
