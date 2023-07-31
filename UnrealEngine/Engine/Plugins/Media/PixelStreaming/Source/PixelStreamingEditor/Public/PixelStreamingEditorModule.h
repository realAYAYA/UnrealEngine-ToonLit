// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPixelStreamingModule.h"
#include "PixelStreamingServers.h"
#include "PixelStreamingEditorUtils.h"

namespace UE::EditorPixelStreaming
{
    class FPixelStreamingToolbar;
}

class PIXELSTREAMINGEDITOR_API FPixelStreamingEditorModule : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    void StartStreaming(UE::EditorPixelStreaming::EStreamTypes InStreamType);
    void StopStreaming();
    void SetStreamType(UE::EditorPixelStreaming::EStreamTypes InStreamType) { StreamType = InStreamType; };
    UE::EditorPixelStreaming::EStreamTypes GetStreamType();

    void StartSignalling();
    void StopSignalling();
    TSharedPtr<UE::PixelStreamingServers::IServer> GetSignallingServer();
    
    void SetSignallingDomain(const FString& InSignallingDomain);
    FString GetSignallingDomain() { return SignallingDomain; };
    void SetStreamerPort(int32 InStreamerPort);
    int32 GetStreamerPort() { return StreamerPort; };
    void SetViewerPort(int32 InViewerPort);
    int32 GetViewerPort() { return ViewerPort; };
       
    static FPixelStreamingEditorModule* GetModule();
private:
    void InitEditorStreaming(IPixelStreamingModule& Module);
	bool ParseResolution(const TCHAR* InResolution, uint32& OutX, uint32& OutY);
    void MaybeResizeEditor(TSharedPtr<SWindow> RootWindow);
    FString GetSignallingServerURL();

    TSharedPtr<UE::EditorPixelStreaming::FPixelStreamingToolbar> Toolbar;
    static FPixelStreamingEditorModule* PixelStreamingEditorModule;	
    // Signalling/webserver
	TSharedPtr<UE::PixelStreamingServers::IServer> SignallingServer;
    // Download process for PS web frontend files (if we want to view output in the browser)
	TSharedPtr<FMonitoredProcess> DownloadProcess;
    //
    UE::EditorPixelStreaming::EStreamTypes StreamType;
    // The signalling server host: eg ws://127.0.0.1
    FString SignallingDomain;
    // The port the streamer will connect to. eg 8888
    int32 StreamerPort;
    // The port the streams can be viewed at on the browser. eg 80
    int32 ViewerPort = 80;

public:
    bool bUseExternalSignallingServer = false;
};
