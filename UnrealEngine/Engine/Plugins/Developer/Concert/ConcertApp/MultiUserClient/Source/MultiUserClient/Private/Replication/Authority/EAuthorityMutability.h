// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace UE::MultiUserClient
{
	/** Various reasons in which an object can be when considering changing its authority */
	enum class EAuthorityMutability : uint8
	{
		/** The object has no registered properties. Cannot take authority. */
		NotApplicable,
		
		/** There is a conflict with another client */
		Conflict,
		
		/** Authority can be taken */
		Allowed
	};
}