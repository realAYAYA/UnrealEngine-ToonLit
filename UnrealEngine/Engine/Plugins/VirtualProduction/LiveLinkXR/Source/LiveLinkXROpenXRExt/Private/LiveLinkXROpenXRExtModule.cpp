// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkXROpenXRExtModule.h"
#include "LiveLinkXROpenXRExt.h"


DEFINE_LOG_CATEGORY(LogLiveLinkXROpenXRExt);


IMPLEMENT_MODULE(FLiveLinkXROpenXRExtModule, LiveLinkXROpenXRExt);


void FLiveLinkXROpenXRExtModule::StartupModule()
{
	OpenXrExt = MakeShared<FLiveLinkXROpenXRExtension>();
}


void FLiveLinkXROpenXRExtModule::ShutdownModule()
{
	OpenXrExt.Reset();
}


TSharedPtr<FLiveLinkXROpenXRExtension> FLiveLinkXROpenXRExtModule::GetExtension()
{
	return OpenXrExt;
}
