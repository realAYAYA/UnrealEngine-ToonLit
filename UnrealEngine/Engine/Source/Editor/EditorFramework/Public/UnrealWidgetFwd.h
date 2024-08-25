// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class FWidget;

/** Coordinate system identifiers. */
enum ECoordSystem
{
	COORD_None = -1,
	COORD_World,
	COORD_Local,
	COORD_Parent,
	COORD_Max,
};

namespace UE {
namespace Widget {
	enum EWidgetMode
	{
		WM_None = -1,
		WM_Translate,
		WM_TranslateRotateZ,
		WM_2D,
		WM_Rotate,
		WM_Scale,
		WM_Max,
	};
}
}
