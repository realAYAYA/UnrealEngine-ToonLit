// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR && WITH_OCIO
#include "OpenColorIO/OpenColorIO.h"
#endif

class FOpenColorIONativeConfiguration {
public:

#if WITH_EDITOR && WITH_OCIO
	OCIO_NAMESPACE::ConstConfigRcPtr Get() const
	{
		return Config;
	}

	void Set(OCIO_NAMESPACE::ConstConfigRcPtr InConfig)
	{
		Config = InConfig;
	}
#endif

private:
#if WITH_EDITORONLY_DATA && WITH_OCIO
	OCIO_NAMESPACE::ConstConfigRcPtr Config;
#endif //WITH_EDITORONLY_DATA
};
