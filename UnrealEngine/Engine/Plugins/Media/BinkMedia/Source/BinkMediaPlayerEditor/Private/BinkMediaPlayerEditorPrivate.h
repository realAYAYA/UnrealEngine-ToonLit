// Copyright Epic Games Tools LLC
//   Licenced under the Unreal Engine EULA 

#pragma once


/* Private dependencies
 *****************************************************************************/

#include "Runtime/Launch/Resources/Version.h"
#include "CoreMinimal.h"
#include "AssetToolsModule.h"
#include "Styling/SlateStyle.h"
#include "BinkMediaPlayer.h"
#include "BinkMediaTexture.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Slate/SceneViewport.h"
#include "Slate/SlateTextures.h"
#include "TickableObjectRenderThread.h"
#include "WorkspaceMenuStructureModule.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkit.h"
#include "PropertyCustomizationHelpers.h"

/* Private includes
 *****************************************************************************/

#include "Factories/BinkMediaPlayerFactory.h"
#include "Factories/BinkMediaPlayerFactoryNew.h"
#include "Factories/BinkMediaTextureFactoryNew.h"

#include "Models/BinkMediaPlayerEditorCommands.h"
#include "Models/BinkMediaPlayerEditorTexture.h"
#include "BinkMediaPlayerEditorToolkit.h"

#include "SBinkWidgets.h"

#include "binkplugin_ue4.h"
