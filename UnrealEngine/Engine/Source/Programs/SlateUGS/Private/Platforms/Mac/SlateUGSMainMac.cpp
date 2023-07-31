// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlateUGSApp.h"
#include "Mac/MacProgramDelegate.h"
#include "LaunchEngineLoop.h"

int main(int argc, char *argv[])
{
	[MacProgramDelegate mainWithArgc:argc argv:argv programMain:RunSlateUGS programExit:FEngineLoop::AppExit];
}


