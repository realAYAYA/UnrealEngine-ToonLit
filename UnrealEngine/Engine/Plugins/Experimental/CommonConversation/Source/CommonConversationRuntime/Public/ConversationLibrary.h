// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "Kismet/BlueprintFunctionLibrary.h"

#include "Templates/SubclassOf.h"
#include "ConversationLibrary.generated.h"

struct FGameplayTag;

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

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "ConversationInstance.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#endif
