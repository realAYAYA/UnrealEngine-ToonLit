// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXPixelMappingLayoutSettings.generated.h"


/** Layout options for the Pixel Mapping editor */
UCLASS(Config = DMXPixelMappingEditor, DefaultConfig, meta = (DisplayName = "DMXEditor"))
class UDMXPixelMappingLayoutSettings 
	: public UObject
{
	GENERATED_BODY()

public:
	/** If true, scales children when the parent component is resized */
	UPROPERTY(Config, EditAnywhere, Category = "Layout Settings")
	bool bScaleChildrenWithParent = true;

	/** If true, selects parent when a child is clicked */
	UPROPERTY(Config, EditAnywhere, Category = "Layout Settings")
	bool bAlwaysSelectGroup = false;

	/** If true, applies layout scripts instantly when they are loaded */
	UPROPERTY(Config, EditAnywhere, Category = "Layout Settings")
	bool bApplyLayoutScriptWhenLoaded = true;

	/** If true, shows the name of the Fixture Patch where applicable. May affect editor performance. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout Settings")
	bool bShowComponentNames = true;

	/** If true, shows Fixture info bout the Fixture Patch where applicable. May affect editor performance. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout Settings")
	bool bShowPatchInfo = false;

	/** If true, shows Cell IDs where applicable. May affect editor performance. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout Settings")
	bool bShowCellIDs = false;

	/** Gets a delegate broadcast when Layout Settings changed */
	static FSimpleMulticastDelegate& GetOnLayoutSettingsChanged() { return OnLayoutSettingsChanged; }

protected:
	//~ Being UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject interface

private:
	/** Broadcast when Layout Settings changed */
	static FSimpleMulticastDelegate OnLayoutSettingsChanged;

};
