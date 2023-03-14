// Copyright Epic Games, Inc. All Rights Reserved.


#include "EditorModeTools.h"


FModeTool::FModeTool():
	ID( MT_None ),
	bUseWidget( 1 )
{}

FModeTool::~FModeTool()
{
}

void FModeTool::DrawHUD(FEditorViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas)
{
}

void FModeTool::Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI)
{
}


