// Copyright Epic Games, Inc. All Rights Reserved.

#include "PatchCheckModule.h"
#include "PatchCheck.h"

class FPatchCheckModule : public TPatchCheckModule<FPatchCheck>
{
};

IMPLEMENT_MODULE(FPatchCheckModule, PatchCheck);
