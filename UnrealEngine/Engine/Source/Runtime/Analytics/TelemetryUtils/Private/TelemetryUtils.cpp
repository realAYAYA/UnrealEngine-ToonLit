// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "TelemetryUtils.h"
#include "TelemetryRouter.h"

IMPLEMENT_MODULE( FTelemetryUtils, TelemetryUtils );

FTelemetryUtils::FTelemetryUtils()
{
}

FTelemetryUtils::~FTelemetryUtils()
{
}

void FTelemetryUtils::StartupModule()
{
    Router = MakeUnique<FTelemetryRouter>();
}

void FTelemetryUtils::ShutdownModule()
{
    Router.Reset();
}
