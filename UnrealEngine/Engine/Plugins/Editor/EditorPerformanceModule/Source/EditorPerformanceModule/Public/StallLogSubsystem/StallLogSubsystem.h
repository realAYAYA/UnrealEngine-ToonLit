// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSubsystem.h"

#include "Containers/ContainersFwd.h"
#include "Delegates/IDelegateInstance.h"
#include "Templates/SharedPointer.h"

#include "StallLogSubsystem.generated.h"

class FDelegateHandle;
class FSpawnTabArgs;
class FStallLogHistory;
struct FSlateBrush;
class SDockTab;
class SWidget;

/**
 * Subsystem that provides feedback on stall detection
 */
UCLASS()
class EDITORPERFORMANCEMODULE_API UStallLogSubsystem  : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:

	TSharedRef<SDockTab> CreateStallLogTab(const FSpawnTabArgs& InArgs);
	const FSlateBrush* GetStatusBarBadgeIcon() const;

	void RegisterStallDetectedDelegates();
	void UnregisterStallDetectedDelegates();

private:

	FDelegateHandle OnStallDetectedDelegate;
	FDelegateHandle OnStallCompletedDelegate;
	TSharedPtr<FStallLogHistory, ESPMode::ThreadSafe> StallLogHistory;
	
	TSharedPtr<SWidget> StallLog;
	TWeakPtr<SDockTab> StallLogTab;
};
