// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPPMChainGraphEditorModule.h"
#include "CoreMinimal.h"


class FPPMChainGraphEditorModule : public IPPMChainGraphEditorModule, public TSharedFromThis<FPPMChainGraphEditorModule>
{
public:

	virtual ~FPPMChainGraphEditorModule() = default;

	//~ Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface interface
};

