//
// Copyright (C) Google Inc. 2017. All rights reserved.
//

#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class IResonanceAudioEditorModule : public IModuleInterface
{
public:
	/**
	* Singleton-like access to this module's interface.  This is just for convenience!
	* Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	*
	* @return Returns singleton instance, loading the module on demand if needed
	*/
	static inline IResonanceAudioEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IResonanceAudioEditorModule>("ResonanceAudioEditorModule");
	}

	/**
	* Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	*
	* @return True if the module is loaded and ready to use
	*/
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("ResonanceAudioEditorModule");
	}
};
