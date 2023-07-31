// Copyright Epic Games, Inc. All Rights Reserved.

#include "Firebase.h"

DEFINE_LOG_CATEGORY(LogFirebase);

void IFirebaseModuleInterface::StartupModule()
{
}

void IFirebaseModuleInterface::ShutdownModule()
{
}

class FFirebaseModule : public IFirebaseModuleInterface
{
};

IMPLEMENT_MODULE(FFirebaseModule, Firebase);
