// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/EditorNotifyObject.h"
#include "Animation/AnimSequenceBase.h"

UEditorNotifyObject::UEditorNotifyObject(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

bool UEditorNotifyObject::ApplyChangesToMontage()
{
	if(AnimObject)
	{
		for(FAnimNotifyEvent& Notify : AnimObject->Notifies)
		{
			if(Notify.Guid == Event.Guid)
			{
				Event.OnChanged(Event.GetTime());

				// If we have a duration this is a state notify
				if(Event.GetDuration() > 0.0f)
				{
					Event.EndLink.OnChanged(Event.EndLink.GetTime());

					// Always keep link methods in sync between notifies and duration links
					if(Event.GetLinkMethod() != Event.EndLink.GetLinkMethod())
					{
						Event.EndLink.ChangeLinkMethod(Event.GetLinkMethod());
					}
				}
				Notify = Event;
				break;
			}
		}
	}

	return true;
}

void UEditorNotifyObject::InitialiseNotify(const FAnimNotifyEvent& InNotify)
{
	if(AnimObject)
	{
		Event = InNotify;
	}
}
