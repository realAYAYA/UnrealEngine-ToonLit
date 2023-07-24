// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointer.h"

class FImgMediaPlayer;
class ISlateStyle;

/**
* Interface for the MediaPlayerEditor module.
*/
class MEDIAPLAYEREDITOR_API IMediaPlayerEditorModule
	: public IModuleInterface
{
public:
	
	/** Get the style used by this module. */
	virtual TSharedPtr<ISlateStyle> GetStyle() = 0;

};
