// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Toolkits/AssetEditorToolkit.h"

class ICameraAssetEditorToolkit;
class ICameraModeEditorToolkit;
class UCameraAsset;
class UCameraMode;

class IGameplayCamerasEditorModule : public IModuleInterface
{
public:

	static const FName GameplayCamerasEditorAppIdentifier;

	virtual ~IGameplayCamerasEditorModule() = default;

	virtual TSharedRef<ICameraAssetEditorToolkit> CreateCameraAssetEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UCameraAsset* CameraAsset) = 0;

	virtual TSharedRef<ICameraModeEditorToolkit> CreateCameraModeEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UCameraMode* CameraMode) = 0;
};

