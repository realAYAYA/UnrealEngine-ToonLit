// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Misc/Optional.h"

/**
 * Describes the origin and visibility of Verse code
 **/
namespace EVerseScope
{
	enum Type : uint8
	{
		PublicAPI,    // Created by Epic and only public definitions will be visible to public users
		InternalAPI,  // Created by Epic and is entirely hidden from public users
		PublicUser,   // Created by a public user
		InternalUser, // Created by an Epic internal user
	};

	/**
	 * Converts a string to a EVerseScope::Type value
	 */
	PROJECTS_API TOptional<EVerseScope::Type> FromString(const TCHAR* Text);

	/**
	 * Returns the name of a Verse scope.
	 */
	PROJECTS_API const TCHAR* ToString(const EVerseScope::Type Value);
};
