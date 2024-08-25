// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class IStormSyncImportWizard
{
public:
	/** Returns user choice, either import or cancel */
	virtual bool ShouldImport() const = 0;
};
