// Copyright Epic Games, Inc. All Rights Reserved.

#include "Framework/Commands/Commands.h"
#include "Styling/ISlateStyle.h"

#define LOC_DEFINE_REGION

void MakeUICommand_InternalUseOnly( FBindingContext* This, TSharedPtr< FUICommandInfo >& OutCommand, const TCHAR* InSubNamespace, const TCHAR* InCommandName, const TCHAR* InCommandNameUnderscoreTooltip, const ANSICHAR* DotCommandName, const TCHAR* FriendlyName, const TCHAR* InDescription, const EUserInterfaceActionType CommandType, const FInputChord& InDefaultChord, const FInputChord& InAlternateDefaultChord)
{
	static const FString UICommandsStr(TEXT("UICommands"));
	const FString Namespace = InSubNamespace && FCString::Strlen(InSubNamespace) > 0 ? UICommandsStr + TEXT(".") + InSubNamespace : UICommandsStr;

	FUICommandInfo::MakeCommandInfo(
		This->AsShared(),
		OutCommand,
		InCommandName,
		FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText( FriendlyName, *Namespace, InCommandName ),
		FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText( InDescription, *Namespace, InCommandNameUnderscoreTooltip ),
		FSlateIcon( This->GetStyleSetName(), ISlateStyle::Join( This->GetContextName(), DotCommandName ) ),
		CommandType,
		InDefaultChord,
		InAlternateDefaultChord
	);
}

#undef LOC_DEFINE_REGION
