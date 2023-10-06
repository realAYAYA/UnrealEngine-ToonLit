// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

class IDirectLinkUI;

/**
 * Static interface for the datasmith exporter UI module
 */
class IDatasmithExporterUIModule
{
public:
	/**
	 * This will return nullptr if the datasmith exporter manager initialization wasn't done with the datasmith ui argument at true
	 */
	static DATASMITHEXPORTERUI_API IDatasmithExporterUIModule* Get();

	/**
	 * Return the Direct Link UI interface
	 * @return A valid pointer if the exporter was initialized with the messaging argument being true
	 */
	virtual IDirectLinkUI* GetDirectLinkExporterUI() const = 0;

	virtual ~IDatasmithExporterUIModule() = default;
};
