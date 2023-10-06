// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class IPlugin;

/** Interface to manage project references to external content */
class IProjectExternalContentInterface
{
public:
	/** Returns whether the project can reference external content */
	virtual bool IsEnabled() const = 0;

	/**
     * Returns whether the specified external content is referenced by the project
	 * @param ExternalContentId External content identifier (verse path, link code, etc)
	 */
	virtual bool HasExternalContent(const FString& ExternalContentId) const = 0;

	/** 
	 * Returns whether the specified external content is loaded (and referenced by the project)
	 * @param ExternalContentId External content identifier (verse path, link code, etc)
	 */
	virtual bool IsExternalContentLoaded(const FString& ExternalContentId) const = 0;

	/** Returns the list of external content referenced by the project (verse paths, link codes, etc) */
	virtual TArray<FString> GetExternalContentIds() const = 0;

	/**
	 * Called upon AddExternalContent completion
	 * @param bSuccess Whether the external content was successfully added to the project
	 * @param Plugins List of loaded plugins hosting the external content
	 */
	DECLARE_DELEGATE_TwoParams(FAddExternalContentComplete, bool /*bSuccess*/, const TArray<TSharedRef<IPlugin>>& /*Plugins*/);

	/**
	 * Adds a reference to external content to the project and asynchronously downloads/loads the external content
	 * @param ExternalContentId External content identifier (verse path, link code, etc)
	 * @param CompleteCallback See FAddExternalContentComplete
	 */
	virtual void AddExternalContent(const FString& ExternalContentId, FAddExternalContentComplete CompleteCallback = FAddExternalContentComplete()) = 0;

	/**
	 * Called upon RemoveExternalContent completion
	 * @param bSuccess Whether the external content was successfully removed from the project (could be canceled by the user)
	 */
	DECLARE_DELEGATE_OneParam(FRemoveExternalContentComplete, bool /*bSuccess*/);

	/**
	 * Removes references to external content from the project and unloads the external content
	 * @param ExternalContentIds External content identifiers (verse path, link code, etc)
	 * @param CompleteCallback See FRemoveExternalContentComplete
	 */
	virtual void RemoveExternalContent(TConstArrayView<FString> ExternalContentIds, FRemoveExternalContentComplete CompleteCallback = FRemoveExternalContentComplete()) = 0;

	void RemoveExternalContent(const FString& ExternalContentId, FRemoveExternalContentComplete CompleteCallback = FRemoveExternalContentComplete())
	{
		TArray<FString, TInlineAllocator<1>> ExternalContentIds = { ExternalContentId };
		RemoveExternalContent(ExternalContentIds, MoveTemp(CompleteCallback));
	}
};
