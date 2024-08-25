// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeSettings.h"
#include "LandscapeModule.h"
#include "Modules/ModuleManager.h"
#include "LandscapeEditorServices.h"

#if WITH_EDITOR

void ULandscapeSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;
	if ((PropertyName == GET_MEMBER_NAME_CHECKED(ULandscapeSettings, BrushSizeUIMax))
		|| (PropertyName == GET_MEMBER_NAME_CHECKED(ULandscapeSettings, BrushSizeClampMax)))
	{
		// If landscape mode is active, refresh the detail panel to apply the changes immediately : 
		ILandscapeModule& LandscapeModule = FModuleManager::GetModuleChecked<ILandscapeModule>("Landscape");
		LandscapeModule.GetLandscapeEditorServices()->RefreshDetailPanel();
	}
}

#endif // WITH_EDITOR
