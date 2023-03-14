// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/Timecode.h"
#include "HAL/IConsoleManager.h"


static TAutoConsoleVariable<bool> CVarUseDropFormatTimecodeByDefaultWhenSupported(
	TEXT("timecode.UseDropFormatTimecodeByDefaultWhenSupported"),
	1.0f,
	TEXT("By default, should we generate a timecode in drop frame format when the frame rate does support it."),
	ECVF_Default);

/* FTimecode interface
 *****************************************************************************/
bool FTimecode::UseDropFormatTimecodeByDefaultWhenSupported()
{
	return CVarUseDropFormatTimecodeByDefaultWhenSupported.GetValueOnAnyThread();
}
