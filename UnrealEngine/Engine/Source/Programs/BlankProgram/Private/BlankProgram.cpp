// Copyright Epic Games, Inc. All Rights Reserved.


#include "BlankProgram.h"

#include "RequiredProgramMainCPPInclude.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlankProgram, Log, All);

IMPLEMENT_APPLICATION(BlankProgram, "BlankProgram");

INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	GEngineLoop.PreInit(ArgC, ArgV);
	UE_LOG(LogBlankProgram, Display, TEXT("Hello World"));
	FEngineLoop::AppExit();
	return 0;
}
