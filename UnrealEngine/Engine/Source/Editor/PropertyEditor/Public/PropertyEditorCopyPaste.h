// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "PropertyHandle.h"

/** Utilities related to copy/paste. */
namespace UE::PropertyEditor
{
	/** Gets the property path from the provided PropertyHandle (if valid). */
	[[nodiscard]] PROPERTYEDITOR_API FString GetPropertyPath(const TSharedPtr<IPropertyHandle>& InPropertyHandle);

	/** Returns true if the Tag is empty, or the specified property matches it. */
	[[nodiscard]] PROPERTYEDITOR_API bool TagMatchesProperty(const FString& InTag, const TSharedPtr<IPropertyHandle>& InPropertyHandle);
}

/** Subscribe to and parse the string contents. Note that the caller is responsible for providing valid UE text,
 * and the subscriber is responsible for applying it if applicable.  */
typedef TMulticastDelegate<void(const FString& InTag, const FString& InText, const TOptional<FGuid>& InOperationId)> FOnPasteFromText;
