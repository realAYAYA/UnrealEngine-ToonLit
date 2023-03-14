// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Launch/SProjectLauncherLaunchCustomRoles.h"


#define LOCTEXT_NAMESPACE "SProjectLauncherLaunchCustomRoles"


/* SProjectLauncherLaunchCustomRoles structors
 *****************************************************************************/

SProjectLauncherLaunchCustomRoles::~SProjectLauncherLaunchCustomRoles()
{
	if (Model.IsValid())
	{
		Model->OnProfileSelected().RemoveAll(this);
	}
}


/* SProjectLauncherLaunchCustomRoles interface
 *****************************************************************************/

void SProjectLauncherLaunchCustomRoles::Construct(const FArguments& InArgs, const TSharedRef<FProjectLauncherModel>& InModel)
{
	Model = InModel;

	ChildSlot
	[
		SNullWidget::NullWidget
	];
}


#undef LOCTEXT_NAMESPACE
