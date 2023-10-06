// Copyright Epic Games, Inc. All Rights Reserved.

#include "SourceControlCVars.h"

namespace SourceControlCVars
{
	TAutoConsoleVariable<bool> CVarSourceControlEnableRevertFromSceneOutliner(
		TEXT("SourceControl.Revert.EnableFromSceneOutliner"),
		false,
		TEXT("Allows a SourceControl 'Revert' operation to be triggered from the SceneOutliner."),
		ECVF_Default);

	TAutoConsoleVariable<bool> CVarSourceControlEnableRevertFromSubmitWidget(
		TEXT("SourceControl.Revert.EnableFromSubmitWidget"),
		false,
		TEXT("Allows a SourceControl 'Revert' operation to be triggered from the SubmitWidget."),
		ECVF_Default);

	TAutoConsoleVariable<bool> CVarSourceControlEnableRevertUnsaved(
		TEXT("SourceControl.RevertUnsaved.Enable"),
		false,
		TEXT("Allows a SourceControl 'Revert' operation to be triggered on an unsaved asset."),
		ECVF_Default);

	TAutoConsoleVariable<bool> CVarSourceControlEnableLoginDialogModal(
		TEXT("SourceControl.LoginDialog.ForceModal"),
		false,
		TEXT("Forces the SourceControl 'Login Dialog' to always be a modal dialog."),
		ECVF_Default);
}