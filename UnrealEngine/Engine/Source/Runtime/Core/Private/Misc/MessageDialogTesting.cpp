// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"
#include "HAL/IConsoleManager.h"

#if (WITH_EDITOR || IS_PROGRAM) && !UE_BUILD_SHIPPING

static void TestMessageDialog()
{

	static int currentType = 0;
	static TArray<EAppMsgType::Type> MsgTypes;

	MsgTypes.Add(EAppMsgType::Ok);
	MsgTypes.Add(EAppMsgType::YesNo);
	MsgTypes.Add(EAppMsgType::OkCancel);
	MsgTypes.Add(EAppMsgType::YesNoCancel);
	MsgTypes.Add(EAppMsgType::CancelRetryContinue);
	MsgTypes.Add(EAppMsgType::YesNoYesAllNoAll);
	MsgTypes.Add(EAppMsgType::YesNoYesAllNoAllCancel);
	MsgTypes.Add(EAppMsgType::YesNoYesAll);

	FText Message = FText::FromString("This is an important message.");
	FText LongMessage = FText::FromString("This is a really long important message. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Pellentesque nisi lectus, accumsan in ex a, varius ullamcorper turpis. Sed elementum eleifend nisl vitae pellentesque. Ut ac laoreet augue. Interdum et malesuada fames ac ante ipsum primis in faucibus. Nam ut lacus tortor. Fusce ante purus, ultricies sed rhoncus accumsan, egestas sed leo. Nam sed odio diam. Nullam ac ex consequat, viverra ante id, sodales dui. Suspendisse cursus urna vel arcu pulvinar fermentum. Vivamus et maximus nunc, sed dictum nibh. Ut ex nisl, iaculis et convallis ut, viverra a ligula. Donec nisl sapien, consectetur ut porttitor a, laoreet non ante. Quisque.  Lorem ipsum dolor sit amet, consectetur adipiscing elit. Pellentesque nisi lectus, accumsan in ex a, varius ullamcorper turpis. Sed elementum eleifend nisl vitae pellentesque. Ut ac laoreet augue. Interdum et malesuada fames ac ante ipsum primis in faucibus. Nam ut lacus tortor. Fusce ante purus, ultricies sed rhoncus accumsan, egestas sed leo. Nam sed odio diam. Nullam ac ex consequat, viverra ante id, sodales dui. Suspendisse cursus urna vel arcu pulvinar fermentum. Vivamus et maximus nunc, sed dictum nibh. Ut ex nisl, iaculis et convallis ut, viverra a ligula. Donec nisl sapien, consectetur ut porttitor a, laoreet non ante. Quisque.");
	FText DialogTitle = FText::FromString("Message Title");

	EAppReturnType::Type RetVal = EAppReturnType::Ok;

	while (RetVal == EAppReturnType::Yes ||
		   RetVal == EAppReturnType::YesAll || 
		   RetVal == EAppReturnType::Ok || 
		   RetVal == EAppReturnType::Retry || 
		   RetVal == EAppReturnType::Continue)
	{
		RetVal = FMessageDialog::Open(MsgTypes[currentType], Message, &DialogTitle);
		currentType = (currentType + 1) % MsgTypes.Num();
	}
}

FAutoConsoleCommand TestMessageDialogCommand(TEXT("Slate.TestMessageDialog"), TEXT(""), FConsoleCommandDelegate::CreateStatic(&TestMessageDialog));

#endif