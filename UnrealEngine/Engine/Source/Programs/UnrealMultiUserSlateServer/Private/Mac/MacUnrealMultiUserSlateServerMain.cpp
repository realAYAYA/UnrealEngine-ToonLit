// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealMultiUserSlateServerRun.h"
#include "Mac/MacProgramDelegate.h"
#include "LaunchEngineLoop.h"

int main( int argc, char *argv[] )
{
	return [MacProgramDelegate mainWithArgc:argc argv:argv programMain:RunUnrealMultiUserServer programExit:FEngineLoop::AppExit];
}
