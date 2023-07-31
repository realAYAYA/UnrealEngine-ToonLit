// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BuildPatchSettings.h"
#include "BuildPatchState.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "Interfaces/IBuildInstaller.h"
#include "Interfaces/IBuildManifest.h"
#include "Interfaces/IBuildPatchServicesModule.h"
#include "Internationalization/Text.h"
#include "Templates/SharedPointer.h"

class IBuildPatchServicesModule;

//This class is used to help manage a PreLoadScreen based on a BuildPatchServices install.
class PRELOADSCREEN_API FBuildPatchServicesPreLoadManagerBase : public TSharedFromThis<FBuildPatchServicesPreLoadManagerBase>
{
public:
    FBuildPatchServicesPreLoadManagerBase();
    virtual ~FBuildPatchServicesPreLoadManagerBase() {}

    virtual void Init();
    
    //Setup BPT with everything now loaded
    virtual void StartBuildPatchServices(BuildPatchServices::FBuildInstallerConfiguration Settings);

    //BPT finished
    virtual void OnContentBuildInstallerComplete(const IBuildInstallerRef& Installer);

    virtual bool IsDone() const;

    virtual int64 GetDownloadSize() { return ContentBuildInstaller.IsValid() ? ContentBuildInstaller->GetTotalDownloadRequired() : 0; }
    virtual int64 GetDownloadProgress() { return ContentBuildInstaller.IsValid() ? ContentBuildInstaller->GetTotalDownloaded() : 0; }
    
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnBuildPatchCompleted, bool );
    FOnBuildPatchCompleted OnBuildPatchCompletedDelegate;

    virtual void PauseBuildPatchInstall();
    virtual void ResumeBuildPatchInstall();
	virtual void CancelBuildPatchInstall();

    float GetProgressPercent() const { return ContentBuildInstaller.IsValid() ? ContentBuildInstaller->GetUpdateProgress() : 0.0f; }
    EBuildPatchDownloadHealth GetDownloadHealth() const { return ContentBuildInstaller.IsValid() ? ContentBuildInstaller->GetDownloadHealth() : EBuildPatchDownloadHealth::NUM_Values; }
    const FText& GetStatusText() const;
    BuildPatchServices::EBuildPatchState GetState() const { return ContentBuildInstaller.IsValid() ? ContentBuildInstaller->GetState() : BuildPatchServices::EBuildPatchState::Initializing; }

    FText GetErrorMessageBody() const { return ContentBuildInstaller.IsValid() ? ContentBuildInstaller->GetErrorText() : FText(); }
    EBuildPatchInstallError GetErrorType() const { return ContentBuildInstaller.IsValid() ? ContentBuildInstaller->GetErrorType() : EBuildPatchInstallError::NoError; }
    FString GetErrorCode() const { return ContentBuildInstaller.IsValid() ? ContentBuildInstaller->GetErrorCode() : TEXT("U"); }

    virtual bool IsActive()
    {
        return (ContentBuildInstaller.IsValid()
            && !ContentBuildInstaller->IsComplete()
            && !ContentBuildInstaller->HasError()
            && ContentBuildInstaller->GetState() > BuildPatchServices::EBuildPatchState::Resuming);
    }

    IBuildInstallerPtr GetInstaller() { return ContentBuildInstaller; }

protected:
    bool bPatchingStarted;
    bool bPatchingFinished;

    IBuildPatchServicesModule* BuildPatchServicesModule;
    IBuildInstallerPtr ContentBuildInstaller;
};
