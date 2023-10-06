// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

/**
 * A simple module that act as bridge between the Interchange plugin and the UnrealEd module
 * The goal of this module is to simply avoid some code duplication between the Interchange system and the old editor factories for the textures
 */

IMPLEMENT_MODULE(FDefaultModuleImpl, TextureUtilitiesCommon)
