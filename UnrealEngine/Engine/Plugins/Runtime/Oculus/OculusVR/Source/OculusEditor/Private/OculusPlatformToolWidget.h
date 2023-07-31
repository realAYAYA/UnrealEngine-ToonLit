// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "OculusPlatformToolSettings.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Engine/PostProcessVolume.h"
#include "Framework/Text/SlateHyperlinkRun.h"
#include "HttpModule.h"
#include "HttpManager.h"
#include "Interfaces/IHttpResponse.h"
#include "Async/AsyncWork.h"
#include "HAL/Event.h"
#include "HAL/ThreadSafeBool.h"
#include "OculusPluginWrapper.h"
#include "Brushes/SlateDynamicImageBrush.h"

class SOculusPlatformToolWidget;

// Function Delegates
DECLARE_DELEGATE_OneParam(FEnableUploadButtonDel, bool);
DECLARE_DELEGATE_OneParam(FUpdateLogTextDel, FString);
DECLARE_DELEGATE_OneParam(FSetProcessDel, FProcHandle);
DECLARE_DELEGATE_RetVal_TwoParams(bool, FFieldValidatorDel, FString, FString&);

PRAGMA_DISABLE_DEPRECATION_WARNINGS

class SOculusPlatformToolWidget : public SCompoundWidget
{
public:
	typedef void(SOculusPlatformToolWidget::*PTextComboBoxDel)(TSharedPtr<FString>, ESelectInfo::Type);
	typedef void(SOculusPlatformToolWidget::*PTextComittedDel)(const FText&, ETextCommit::Type);
	typedef FReply(SOculusPlatformToolWidget::*PButtonClickedDel)();
	typedef bool(SOculusPlatformToolWidget::*PFieldValidatorDel)(FString, FString&);
	typedef void(SOculusPlatformToolWidget::*PCheckBoxChangedDel)(ECheckBoxState);

	SLATE_BEGIN_ARGS(SOculusPlatformToolWidget)
	{}
	SLATE_END_ARGS();

	SOculusPlatformToolWidget();
	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	static FString LogText;

private:
	TSharedPtr<SMultiLineEditableTextBox> ToolConsoleLog;
	TSharedPtr<SVerticalBox> GeneralSettingsBox;
	TSharedPtr<SHorizontalBox> ButtonToolbar;
	TSharedPtr<SVerticalBox> OptionalSettings;
	TSharedPtr<SVerticalBox> ExpansionFilesSettings;
	TSharedPtr<FSlateDynamicImageBrush> ODHIconDynamicImageBrush;

	UEnum* PlatformEnum;
	UEnum* GamepadEmulationEnum;
	UEnum* AssetTypeEnum;
	UDEPRECATED_UOculusPlatformToolSettings* PlatformSettings;
	TArray<TSharedPtr<FString>> OculusPlatforms;
	TArray<TSharedPtr<FString>> RiftGamepadEmulation;
	TArray<TSharedPtr<FString>> AssetType;

	bool Options2DCollapsed;
	bool OptionsRedistPackagesCollapsed;
	bool ActiveUploadButton;
	bool RequestUploadButtonActive;
	FProcHandle PlatformProcess;
	FThreadSafeBool LogTextUpdated;

	FEnableUploadButtonDel EnableUploadButtonDel;
	FUpdateLogTextDel UpdateLogTextDel;
	FSetProcessDel SetProcessDel;

	// Callbacks
	FReply OnStartPlatformUpload();
	FReply OnSelectRiftBuildDirectory();
	FReply OnClearRiftBuildDirectory();
	FReply OnSelectLaunchFilePath();
	FReply OnClearLaunchFilePath();
	FReply OnSelectSymbolDirPath();
	FReply OnClearSymbolDirPath();
	FReply OnSelect2DLaunchPath();
	FReply OnClear2DLaunchPath();
	FReply OnCancelUpload();
	FReply OnSelectLanguagePacksPath();
	FReply OnClearLanguagePacksPath();
	FReply OnSelectExpansionFilesPath();
	FReply OnClearExpansionFilesPath();

	FString GenerateSymbolPath();

	void OnPlatformSettingChanged(TSharedPtr<FString> ItemSelected, ESelectInfo::Type SelectInfo);
	void OnApplicationIDChanged(const FText& InText, ETextCommit::Type InCommitType);
	void OnApplicationTokenChanged(const FText& InText, ETextCommit::Type InCommitType);
	void OnReleaseChannelChanged(const FText& InText, ETextCommit::Type InCommitType);
	void OnReleaseNoteChanged(const FText& InText, ETextCommit::Type InCommitType);
	void OnRiftBuildVersionChanged(const FText& InText, ETextCommit::Type InCommitType);
	void OnRiftLaunchParamsChanged(const FText& InText, ETextCommit::Type InCommitType);
	void OnRiftFirewallChanged(ECheckBoxState CheckState);
	void OnRedistPackageStateChanged(ECheckBoxState CheckState, FRedistPackage* Package);
	void OnRiftGamepadEmulationChanged(TSharedPtr<FString> ItemSelected, ESelectInfo::Type SelectInfo);
	void On2DLaunchParamsChanged(const FText& InText, ETextCommit::Type InCommitType);
	void On2DOptionsExpanded(bool bExpanded);
	void OnRedistPackagesExpanded(bool bExpanded);
	void OnAssetConfigRequiredChanged(ECheckBoxState CheckState, int i);
	void OnAssetConfigTypeChanged(TSharedPtr<FString> ItemSelected, ESelectInfo::Type SelectInfo, int i);
	void OnAssetConfigSKUChanged(const FText& InText, ETextCommit::Type InCommitType, int i);
	void OnUploadDebugSymbolsChanged(ECheckBoxState CheckState);
	void OnDebugSymbolsOnlyChanged(ECheckBoxState CheckState);
	void OnBuildIDChanged(const FText& InText, ETextCommit::Type InCommitType);

	// UI Constructors
	void BuildGeneralSettingsBox(TSharedPtr<SVerticalBox> box);
	void BuildTextComboBoxField(TSharedPtr<SVerticalBox> box, FText name, TArray<TSharedPtr<FString>>* options, TSharedPtr<FString> current, PTextComboBoxDel deleg, int32 indentAmount = 0);
	void BuildTextField(TSharedPtr<SVerticalBox> box, FText name, FText text, FText tooltip, PTextComittedDel deleg, bool isPassword = false, int32 indentAmount = 0);
	void BuildFileDirectoryField(TSharedPtr<SVerticalBox> box, FText name, FText path, FText tooltip, PButtonClickedDel deleg, PButtonClickedDel clearDeleg, int32 indentAmount = 0);
	void BuildCheckBoxField(TSharedPtr<SVerticalBox> box, FText name, bool check, FText tooltip, PCheckBoxChangedDel deleg, int32 indentAmount = 0);
	void BuildButtonToolbar(TSharedPtr<SHorizontalBox> box);
	void BuildRiftOptionalFields(TSharedPtr<SVerticalBox> area);
	void BuildRedistPackagesBox(TSharedPtr<SVerticalBox> box);
	void BuildExpansionFileBox(TSharedPtr<SVerticalBox> box);
	void BuildAssetConfigBox(TSharedPtr<SVerticalBox> box, FAssetConfig config, int index);

	// Text Field Validators
	void ValidateTextField(PFieldValidatorDel del, FString text, FString name, bool& success);
	bool GenericFieldValidator(FString text, FString& error);
	bool IDFieldValidator(FString text, FString& error);
	bool DirectoryFieldValidator(FString text, FString& error);
	bool FileFieldValidator(FString text, FString& error);
	bool LaunchParamValidator(FString text, FString& error);

	bool ConstructArguments(FString& args);
	bool ConstructDebugSymbolArguments(FString& args);
	void EnableUploadButton(bool enabled);
	void LoadConfigSettings();
	void UpdateLogText(FString text);
	void SetPlatformProcess(FProcHandle proc);
	void LoadRedistPackages();
};

class FPlatformDownloadTask : public FNonAbandonableTask
{
	friend class FAsyncTask<FPlatformDownloadTask>;

private:
	FUpdateLogTextDel UpdateLogText;
	FString ToolConsoleLog;
	FEvent* downloadCompleteEvent;
	FEvent* SaveCompleteEvent;
	TArray<uint8> httpData;

public:
	FPlatformDownloadTask(FUpdateLogTextDel textDel, FEvent* saveEvent);

	void OnDownloadRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
	void OnRequestDownloadProgress(FHttpRequestPtr HttpRequest, int32 BytesSend, int32 InBytesReceived);

protected:
	void DoWork();
	void UpdateProgressLog(int progress);

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FPlatformDownloadTask, STATGROUP_ThreadPoolAsyncTasks);
	}
};

class FPlatformUploadTask : public FNonAbandonableTask
{
	friend class FAsyncTask<FPlatformUploadTask>;

public:
	FPlatformUploadTask(FString args, FEnableUploadButtonDel del, FUpdateLogTextDel textDel, FSetProcessDel procDel);

private:
	void* ReadPipe;
	void* WritePipe;

	FSetProcessDel SetProcess;
	FUpdateLogTextDel UpdateLogText;
	FEnableUploadButtonDel EnableUploadButton;
	FString LaunchArgs;

protected:
	void DoWork();

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FPlatformUploadTask, STATGROUP_ThreadPoolAsyncTasks);
	}
};

class FPlatformLoadRedistPackagesTask : public FNonAbandonableTask
{
	friend class FAsyncTask<FPlatformLoadRedistPackagesTask>;

public:
	FPlatformLoadRedistPackagesTask(FUpdateLogTextDel textDel);

private:
	void* ReadPipe;
	void* WritePipe;

	FUpdateLogTextDel UpdateLogText;

protected:
	void DoWork();

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FPlatformLoadRedistPackagesTask, STATGROUP_ThreadPoolAsyncTasks);
	}
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS

