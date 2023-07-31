// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "IKRetargetEditorStyle.h"

class FIKRetargetCommands : public TCommands<FIKRetargetCommands>
{
public:
	FIKRetargetCommands() : TCommands<FIKRetargetCommands>
	(
		"IKRetarget",
		NSLOCTEXT("Contexts", "IKRetarget", "IK Retarget"),
		NAME_None,
		FIKRetargetEditorStyle::Get().GetStyleSetName() // Icon Style Set
	)
	{}

	/** reset retarget pose */
	TSharedPtr< FUICommandInfo > ShowRetargetPose;
	
	/** edit retarget pose */
	TSharedPtr< FUICommandInfo > EditRetargetPose;

	/** reset retarget pose to ref pose */
	TSharedPtr< FUICommandInfo > ResetAllBones;
	/** reset retarget pose to ref pose */
	TSharedPtr< FUICommandInfo > ResetSelectedBones;
	/** reset retarget pose to ref pose */
	TSharedPtr< FUICommandInfo > ResetSelectedAndChildrenBones;

	/** delete retarget pose */
	TSharedPtr< FUICommandInfo > DeleteRetargetPose;

	/** rename retarget pose */
	TSharedPtr< FUICommandInfo > RenameRetargetPose;

	/** new retarget pose */
	TSharedPtr< FUICommandInfo > NewRetargetPose;

	/** duplicate retarget pose */
	TSharedPtr< FUICommandInfo > DuplicateRetargetPose;

	/** import retarget pose */
	TSharedPtr< FUICommandInfo > ImportRetargetPose;

	/** import retarget pose from anim */
	TSharedPtr< FUICommandInfo > ImportRetargetPoseFromAnim;

	/** export retarget pose */
	TSharedPtr< FUICommandInfo > ExportRetargetPose;

	/** export animation */
	TSharedPtr< FUICommandInfo > ExportAnimation;

	/** initialize commands */
	virtual void RegisterCommands() override;
};
