// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interfaces/OnlinePresenceInterface.h"

// Implementation for OnlineSubsystems that have not implemented this version yet
void IOnlinePresence::SetPresence(const FUniqueNetId& User, FOnlinePresenceSetPresenceParameters&& Parameters, const FOnPresenceTaskCompleteDelegate& Delegate)
{
	TSharedPtr<FOnlineUserPresence> ExistingPresence;
	const bool bNeedsExistingPresence = !Parameters.StatusStr.IsSet() || !Parameters.State.IsSet() || !Parameters.Properties.IsSet();
	if (bNeedsExistingPresence)
	{
		GetCachedPresence(User, ExistingPresence);
	}

	FOnlineUserPresenceStatus LegacyParameters;

	if (Parameters.StatusStr.IsSet())
	{
		LegacyParameters.StatusStr = MoveTemp(Parameters.StatusStr.GetValue());
	}
	else if (ExistingPresence)
	{
		LegacyParameters.StatusStr = ExistingPresence->Status.StatusStr;
	}

	if (Parameters.State.IsSet())
	{
		LegacyParameters.State = Parameters.State.GetValue();
	}
	else if (ExistingPresence)
	{
		LegacyParameters.State = ExistingPresence->Status.State;
	}

	if (Parameters.Properties.IsSet())
	{
		LegacyParameters.Properties = MoveTemp(Parameters.Properties.GetValue());
	}
	else if (ExistingPresence)
	{
		LegacyParameters.Properties = ExistingPresence->Status.Properties;
	}

	SetPresence(User, LegacyParameters, Delegate);
}
