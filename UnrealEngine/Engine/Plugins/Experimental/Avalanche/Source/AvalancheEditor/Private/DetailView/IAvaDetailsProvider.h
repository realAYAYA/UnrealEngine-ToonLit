// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointerFwd.h"

class FEditorModeTools;
class IDetailKeyframeHandler;

class IAvaDetailsProvider
{
public:
	virtual FEditorModeTools* GetDetailsModeTools() const = 0;

	virtual TSharedPtr<IDetailKeyframeHandler> GetDetailsKeyframeHandler() const = 0;
};
