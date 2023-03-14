// Copyright Epic Games, Inc. All Rights Reserved.

#include "RenderGridDeveloperModule.h"

#define LOCTEXT_NAMESPACE "RenderGridDeveloperModule"


void UE::RenderGrid::Private::FRenderGridDeveloperModule::StartupModule() {}
void UE::RenderGrid::Private::FRenderGridDeveloperModule::ShutdownModule() {}


#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(UE::RenderGrid::Private::FRenderGridDeveloperModule, RenderGridDeveloper)
