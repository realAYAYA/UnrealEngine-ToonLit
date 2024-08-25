// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaRundownPageTransitionBuilder.h"

#include "Rundown/AvaRundown.h"
#include "Rundown/AvaRundownPagePlayer.h"
#include "Rundown/Transition/AvaRundownPageTransition.h"

FAvaRundownPageTransitionBuilder::~FAvaRundownPageTransitionBuilder()
{
	if (Rundown)
	{
		for (UAvaRundownPageTransition* PageTransition : PageTransitions)
		{
			// Add any remaining playing pages in the channel that are not either exit or enter page already.
			for (UAvaRundownPagePlayer* PagePlayer : Rundown->GetPagePlayers())
			{
				if (PagePlayer->ChannelFName == PageTransition->GetChannelName())
				{
					if (!PageTransition->HasPagePlayer(PagePlayer))
					{
						PageTransition->AddPlayingPage(PagePlayer);
					}
				}
			}
			
			Rundown->AddPageTransition(PageTransition);

			bool bShouldDiscard = false;
			if (PageTransition->CanStart(bShouldDiscard))
			{
				PageTransition->Start();
			}
			else
			{
				// Some playables are still loading, push the command for later execution.
				Rundown->GetPlaybackManager().PushPlaybackTransitionStartCommand(PageTransition);
			}
		}
	}
}

UAvaRundownPageTransition* FAvaRundownPageTransitionBuilder::FindTransition(const UAvaRundownPagePlayer* InPlayer) const
{
	// Currently, the only batching criteria is the channel.
	for (UAvaRundownPageTransition* PageTransition : PageTransitions)
	{
		if (PageTransition->GetChannelName() == InPlayer->ChannelFName)
		{
			return PageTransition;
		}
	}
	return nullptr;	
}

UAvaRundownPageTransition* FAvaRundownPageTransitionBuilder::FindOrAddTransition(const UAvaRundownPagePlayer* InPlayer)
{
	if (UAvaRundownPageTransition* PageTransition = FindTransition(InPlayer))
	{
		return PageTransition;
	}
		
	UAvaRundownPageTransition* PageTransition = NewObject<UAvaRundownPageTransition>(Rundown);
	if (PageTransition)
	{
		PageTransitions.Add(PageTransition);
	}
	return PageTransition;	
}