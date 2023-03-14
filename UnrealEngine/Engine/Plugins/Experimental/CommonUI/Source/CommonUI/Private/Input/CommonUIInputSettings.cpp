// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/CommonUIInputSettings.h"
#include "GameFramework/PlayerInput.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonUIInputSettings)

const UCommonUIInputSettings& UCommonUIInputSettings::Get()
{
	return *GetDefault<UCommonUIInputSettings>();
}

void UCommonUIInputSettings::PostInitProperties()
{
	Super::PostInitProperties();
	
	if (ensure(HasAnyFlags(RF_ClassDefaultObject)))
	{
		//@todo DanH: Resolve the actions and the config overrides
	}
}

const FUIInputAction* UCommonUIInputSettings::FindAction(FUIActionTag ActionTag) const
{
	//@todo DanH: We'll likely want a TMap for these
	return InputActions.FindByPredicate([ActionTag](const FUIInputAction& Action) { return Action.ActionTag == ActionTag; });
}

