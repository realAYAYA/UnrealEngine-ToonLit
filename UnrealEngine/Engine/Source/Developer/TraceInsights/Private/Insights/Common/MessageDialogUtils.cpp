// Copyright Epic Games, Inc. All Rights Reserved.

#include "MessageDialogUtils.h"

#include "Dialog/SCustomDialog.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MessageDialogUtils"

namespace Insights
{

//////////////////////////////////////////////////////////////////////////

EDialogResponse FMessageDialogUtils::ShowChoiceDialog(const FText& Title, const FText& Content)
{
	TSharedRef<SCustomDialog> Dialog = SNew(SCustomDialog)
		.Title(Title)
		.Content()
		[
			SNew(STextBlock).Text(Content)
		]
		.Buttons({
		SCustomDialog::FButton(LOCTEXT("OK", "OK")),
		SCustomDialog::FButton(LOCTEXT("Cancel", "Cancel"))
		});

	// returns 0 when OK is pressed, 1 when Cancel is pressed, -1 if the window is closed
	const int ButtonPressed = Dialog->ShowModal();
	if (ButtonPressed == 0)
	{
		return EDialogResponse::OK;
	}

	return EDialogResponse::Cancel;
}

//////////////////////////////////////////////////////////////////////////

} // namespace Insights

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
