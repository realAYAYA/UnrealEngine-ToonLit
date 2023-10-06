// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/Map.h" // IWYU pragma: keep

class FName;
class FText;

class USocialUser;
class ISocialInteractionWrapper;

/**
 * Represents a single discrete interaction between a local player and another user.
 * Useful for when you'd like to create some tangible list of interactions to compare/sort/classify/iterate.
 * Not explicitly required if you have a particular known interaction in mind - feel free to access the static API of a given interaction directly.
 */
class PARTY_API FSocialInteractionHandle
{
public:
	FSocialInteractionHandle() {}

	bool IsValid() const;
	bool operator==(const FSocialInteractionHandle& Other) const;
	bool operator!=(const FSocialInteractionHandle& Other) const { return !operator==(Other); }

	FName GetInteractionName() const;
	FText GetDisplayName(const USocialUser& User) const;
	FString GetSlashCommandToken() const;

	bool IsAvailable(const USocialUser& User) const;
	void ExecuteInteraction(USocialUser& User) const;
	void ExecuteInteractionWithContext(USocialUser& User, const TMap<FString, FString>& AnalyticsContext) const;

private:
	template <typename> friend class TSocialInteractionWrapper;
	FSocialInteractionHandle(const ISocialInteractionWrapper& Wrapper);

	const ISocialInteractionWrapper* InteractionWrapper = nullptr;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
