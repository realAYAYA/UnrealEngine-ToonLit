// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "WebJSFunction.h"
#include "SWebBrowser.h"
#include "Widgets/SWindow.h"
#include "NodePort.h"
#include "NodeProcess.h"
#include "Misc/FileHelper.h"

#include "TCPServer.h"
#include "BridgeDragDropHelper.h"

#include "IWebBrowserWindow.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "IPlacementModeModule.h"
#include "ActorFactories/ActorFactoryBasicShape.h"

#include "BrowserBinding.generated.h"

DECLARE_DELEGATE_TwoParams(FOnDialogSuccess, FString, FString);
DECLARE_DELEGATE_TwoParams(FOnDialogFail, FString, FString);
DECLARE_DELEGATE_OneParam(FOnDropped, FString);
DECLARE_DELEGATE_OneParam(FOnDropDiscarded, FString);
DECLARE_DELEGATE_OneParam(FOnExit, FString);

UCLASS()
class UBrowserBinding : public UObject
{
	GENERATED_UCLASS_BODY()

private:
	void SwitchDragDropOp(TArray<FString> URLs, TSharedRef<FAssetDragDropOp> DragDropOperation);
	
public:
	FOnDialogSuccess DialogSuccessDelegate;
	FOnDialogFail DialogFailDelegate;
	FOnDropped OnDroppedDelegate;
	FOnDropDiscarded OnDropDiscardedDelegate;
	FOnExit OnExitDelegate;

	TSharedPtr<SWindow> DialogMainWindow;
	TSharedPtr<SWebBrowser> DialogMainBrowser;

	UFUNCTION()
	void DialogSuccessCallback(FWebJSFunction DialogJSCallback);

	UFUNCTION()
	void DialogFailCallback(FWebJSFunction DialogJSCallback);

	UFUNCTION()
	void OnDroppedCallback(FWebJSFunction OnDroppedJSCallback);

	UFUNCTION()
	void OnDropDiscardedCallback(FWebJSFunction OnDropDiscardedJSCallback);

	UFUNCTION()
	void OnExitCallback(FWebJSFunction OnExitJSCallback);

	UFUNCTION()
	void SaveAuthToken(FString Value);

	UFUNCTION()
	FString GetAuthToken();

	UFUNCTION()
	void SendSuccess(FString Value);

	UFUNCTION()
	void SendFailure(FString Message);

	UFUNCTION()
	void ShowDialog(FString Type, FString Url);

	UFUNCTION()
	void DragStarted(TArray<FString> ImageUrl, TArray<FString> IDs, TArray<FString> Types);

	UFUNCTION()
	void ShowLoginDialog(FString LoginUrl, FString ResponseCodeUrl);

	UFUNCTION()
	void OpenExternalUrl(FString Url);

	UFUNCTION()
	void ExportDataToMSPlugin(FString Data);

	UFUNCTION()
	FString GetProjectPath();

	UFUNCTION()
	void Logout();

	UFUNCTION()
	void StartNodeProcess();

	UFUNCTION()
	void RestartNodeProcess();

	UFUNCTION()
	void OpenMegascansPluginSettings();

	// Drag and drop

	bool bWasSwitchDragOperation = false;
	bool bIsDragging = false;
	bool bIsDropEventBound = false;

	TArray<FAssetData> InAssetData;
	TArray<FString> DragDropIDs;
	TArray<FString> DragDropTypes;
	TMap<FString, AActor*> AssetToSphereMap;
	TMap<FString, TArray<FString>> DragOperationToAssetsMap;
};
