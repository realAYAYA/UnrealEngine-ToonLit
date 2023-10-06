// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"

#include "DocumentationRedirect.generated.h"

USTRUCT()
struct DOCUMENTATION_API FDocumentationUrl
{
	GENERATED_BODY()

	FDocumentationUrl() = default;
	FDocumentationUrl(const FString& Link, const FString& BaseUrlId) : Link(Link), BaseUrlId(BaseUrlId) {}

	/** Link for documentation page (page id, full url) */
	UPROPERTY()
	FString Link;

	UPROPERTY()
	FString BaseUrlId;

	bool IsValid() const { return !Link.IsEmpty() || !BaseUrlId.IsEmpty(); }

	FString ToString() const { return !BaseUrlId.IsEmpty() ? (BaseUrlId + ":" + Link) : Link; }
};

/** Engine level documentation link redirect */
USTRUCT()
struct DOCUMENTATION_API FDocumentationRedirect
{
	GENERATED_BODY()

	/**
	 * From Key
	 * 
	 * @note Documentation redirects supports redirection using a UnrealEd URL config key or raw documentation link.
	 *		 This key will be checked to see if it's an UnrealEd URL config key or treated as a raw documentation link as a fallback.
	 *		 See examples of UnrealEd URL config keys in the UnrealEd.URLs section of the BaseEditor.ini config file.
	 */
	UPROPERTY()
	FString From;

	/** To Url */
	UPROPERTY()
	FDocumentationUrl ToUrl;
};
