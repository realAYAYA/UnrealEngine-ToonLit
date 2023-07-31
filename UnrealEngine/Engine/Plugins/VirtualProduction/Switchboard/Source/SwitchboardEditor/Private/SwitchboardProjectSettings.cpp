// Copyright Epic Games, Inc. All Rights Reserved.

#include "SwitchboardProjectSettings.h"


USwitchboardProjectSettings::USwitchboardProjectSettings()
{
}


USwitchboardProjectSettings* USwitchboardProjectSettings::GetSwitchboardProjectSettings()
{
	return GetMutableDefault<USwitchboardProjectSettings>();
}
