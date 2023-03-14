// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "ConversationInstance.h"

#include "ConversationLibrary.generated.h"

class AActor;
class UConversationInstance;

UCLASS()
class COMMONCONVERSATIONRUNTIME_API UConversationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	UConversationLibrary();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Conversation", meta = (DeterminesOutputType = "ConversationType"))
	static UConversationInstance* StartConversation(FGameplayTag ConversationEntryTag, AActor* Instigator, FGameplayTag InstigatorTag,
		AActor* Target, FGameplayTag TargetTag, const TSubclassOf<UConversationInstance> ConversationInstanceClass = nullptr);
};
