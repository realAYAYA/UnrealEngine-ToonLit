// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "DrawPrimitiveDebugger.h"
#include "SDrawPrimitiveDebugger.h"
#include "ViewDebug.h"
#include "Widgets/Docking/SDockTab.h"

DEFINE_LOG_CATEGORY(LogDrawPrimitiveDebugger)

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommand SummonDebuggerCmd(
		TEXT("DrawPrimitiveDebugger.Open"),
		TEXT("Summons the graphics debugger window."),
		FConsoleCommandDelegate::CreateLambda([]() { IDrawPrimitiveDebugger::Get().OpenDebugWindow(); })
		);
/*static FAutoConsoleCommand EnableLiveCaptureCmd(
		TEXT("DrawPrimitiveDebugger.EnableLiveCapture"),
		TEXT("Enables live graphics data capture each frame."),
		FConsoleCommandDelegate::CreateLambda([]() { IDrawPrimitiveDebugger::Get().EnableLiveCapture(); })
		);
static FAutoConsoleCommand DisableLiveCaptureCmd(
		TEXT("DrawPrimitiveDebugger.DisableLiveCapture"),
		TEXT("Disables live graphics data capture."),
		FConsoleCommandDelegate::CreateLambda([]() { IDrawPrimitiveDebugger::Get().DisableLiveCapture(); })
		);*/ // TODO: Re-enable these commands once live capture performance has been fixed
static FAutoConsoleCommand TakeSnapshotCmd(
		TEXT("DrawPrimitiveDebugger.Snapshot"),
		TEXT("Updates the current view information for a single frame."),
		FConsoleCommandDelegate::CreateLambda([]() { IDrawPrimitiveDebugger::Get().CaptureSingleFrame(); })
		);
#endif

class FDrawPrimitiveDebuggerModule : public IDrawPrimitiveDebugger
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	virtual void CaptureSingleFrame() override;
	virtual bool IsLiveCaptureEnabled() const override;
	virtual void EnableLiveCapture() override;
	virtual void DisableLiveCapture() override;
	virtual void OpenDebugWindow() override;
	virtual void CloseDebugWindow() override;
#if !UE_BUILD_SHIPPING
	static const FViewDebugInfo& GetViewDebugInfo();
#endif

private:
	bool bLiveCaptureEnabled = false;
	FDelegateHandle UpdateDelegateHandle;
	TSharedPtr<SDrawPrimitiveDebugger> DebuggerWidget;
	TSharedPtr<SDockTab> DebuggerTab;

	TSharedRef<SDockTab> MakeDrawPrimitiveDebuggerTab(const FSpawnTabArgs&);

	void OnTabClosed(TSharedRef<SDockTab> Tab);
	
	void OnUpdateViewInformation();
};

IMPLEMENT_MODULE(FDrawPrimitiveDebuggerModule, DrawPrimitiveDebugger)

void FDrawPrimitiveDebuggerModule::StartupModule()
{
	bLiveCaptureEnabled = false;
#if !UE_BUILD_SHIPPING
	UpdateDelegateHandle = FViewDebugInfo::Instance.AddUpdateHandler(this, &FDrawPrimitiveDebuggerModule::OnUpdateViewInformation);
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner("DrawPrimitiveDebugger", FOnSpawnTab::CreateRaw(this, &FDrawPrimitiveDebuggerModule::MakeDrawPrimitiveDebuggerTab) );
#endif
}

void FDrawPrimitiveDebuggerModule::ShutdownModule()
{
#if !UE_BUILD_SHIPPING
	FViewDebugInfo::Instance.RemoveUpdateHandler(UpdateDelegateHandle);
#endif
}

void FDrawPrimitiveDebuggerModule::CaptureSingleFrame()
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogDrawPrimitiveDebugger, Log, TEXT("Collecting a single frame graphics data capture"));
	FViewDebugInfo::Instance.CaptureNextFrame();
#endif
}

bool FDrawPrimitiveDebuggerModule::IsLiveCaptureEnabled() const
{
	return bLiveCaptureEnabled;
}

void FDrawPrimitiveDebuggerModule::EnableLiveCapture()
{
#if !UE_BUILD_SHIPPING
	if (!bLiveCaptureEnabled)
	{
		UE_LOG(LogDrawPrimitiveDebugger, Log, TEXT("Enabling live graphics data capture"));
		bLiveCaptureEnabled = true;
		FViewDebugInfo::Instance.EnableLiveCapture();
	}
#endif
}

void FDrawPrimitiveDebuggerModule::DisableLiveCapture()
{
#if !UE_BUILD_SHIPPING
	if (bLiveCaptureEnabled)
	{
		UE_LOG(LogDrawPrimitiveDebugger, Log, TEXT("Disabling live graphics data capture"));
		bLiveCaptureEnabled = false;
		FViewDebugInfo::Instance.DisableLiveCapture();
	}
#endif
}

#if !UE_BUILD_SHIPPING
const FViewDebugInfo& FDrawPrimitiveDebuggerModule::GetViewDebugInfo()
{
	return FViewDebugInfo::Get();
}
#endif

void FDrawPrimitiveDebuggerModule::OpenDebugWindow()
{
#if !UE_BUILD_SHIPPING
	if (!DebuggerTab.IsValid())
	{
		UE_LOG(LogDrawPrimitiveDebugger, Log, TEXT("Opening the Draw Primitive Debugger"));
		if (!FViewDebugInfo::Instance.HasEverUpdated()) CaptureSingleFrame();
		FGlobalTabmanager::Get()->TryInvokeTab(FTabId("DrawPrimitiveDebugger"));
	}
#endif
}

void FDrawPrimitiveDebuggerModule::CloseDebugWindow()
{
#if !UE_BUILD_SHIPPING
	if (DebuggerTab.IsValid())
	{
		UE_LOG(LogDrawPrimitiveDebugger, Log, TEXT("Closing the Draw Primitive Debugger"));
		DebuggerTab->RequestCloseTab();
	}
#endif
}

TSharedRef<SDockTab> FDrawPrimitiveDebuggerModule::MakeDrawPrimitiveDebuggerTab(const FSpawnTabArgs&)
{
	DebuggerTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.OnTabClosed(SDockTab::FOnTabClosedCallback::CreateRaw(this, &FDrawPrimitiveDebuggerModule::OnTabClosed));
	
	if (!DebuggerWidget.IsValid())
	{
		DebuggerWidget = SNew(SDrawPrimitiveDebugger);
		DebuggerTab->SetContent(DebuggerWidget.ToSharedRef());
	}
	return DebuggerTab.ToSharedRef();
}

void FDrawPrimitiveDebuggerModule::OnTabClosed(TSharedRef<SDockTab> Tab)
{
	DebuggerTab.Reset();
	DebuggerTab = nullptr;
	DebuggerWidget.Reset();
	DebuggerWidget = nullptr;
}

void FDrawPrimitiveDebuggerModule::OnUpdateViewInformation()
{
	if (DebuggerWidget && DebuggerWidget.IsValid())
	{
		DebuggerWidget->Refresh();
	}
}