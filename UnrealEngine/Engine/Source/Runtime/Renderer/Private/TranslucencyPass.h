// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// enum instead of bool to get better visibility when we pass around multiple bools, also allows for easier extensions
namespace ETranslucencyPass
{
	enum Type
	{
		TPT_StandardTranslucency,
		TPT_TranslucencyAfterDOF,
		TPT_TranslucencyAfterDOFModulate,
		TPT_TranslucencyAfterMotionBlur,

		/** Drawing all translucency, regardless of separate or standard.  Used when drawing translucency outside of the main renderer, eg FRendererModule::DrawTile. */
		TPT_AllTranslucency,
		TPT_MAX
	};
};

enum class ETranslucencyView
{
	None       = 0,
	UnderWater = 1 << 0,
	AboveWater = 1 << 1,
	RayTracing = 1 << 2
};
ENUM_CLASS_FLAGS(ETranslucencyView);