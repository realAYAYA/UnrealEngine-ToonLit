// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/AppStyle.h"
#include "Framework/Commands/Commands.h"


class FSkeletalMeshModelingToolsCommands : public TCommands<FSkeletalMeshModelingToolsCommands>
{
public:
	FSkeletalMeshModelingToolsCommands()
	    : TCommands<FSkeletalMeshModelingToolsCommands>(
	    	TEXT("SkeletalMeshModelingTools"),
	    	NSLOCTEXT("Contexts", "SkeletalMeshModelingToolsCommands", "Skeletal Mesh Modeling Tools"),
	    	NAME_None,
	    	FAppStyle::GetAppStyleSetName())
	{
	}

	void RegisterCommands() override;
	static const FSkeletalMeshModelingToolsCommands& Get();

	// Modeling tools commands
	TSharedPtr<FUICommandInfo> ToggleModelingToolsMode;
};
