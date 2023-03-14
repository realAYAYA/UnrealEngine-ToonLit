// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorViewportLayoutOnePane.h"

UE_DEPRECATED(5.0, "Level Pane layouts are now owned by their level layouts. Use FEditorViewportLayoutOnePane instead, and add any custom types to FAssetEditorViewportLayout::FactoryPaneConfigurationFromTypeName.")
typedef FEditorViewportLayoutOnePane FLevelViewportLayoutOnePane;
