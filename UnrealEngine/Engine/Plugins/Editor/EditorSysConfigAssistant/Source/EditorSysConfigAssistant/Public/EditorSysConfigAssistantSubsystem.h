// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSubsystem.h"
#include "EditorSysConfigIssue.h"
#include "Templates/SharedPointer.h"

#include "EditorSysConfigAssistantSubsystem.generated.h"

class IModularFeature;
class SNotificationItem;

UCLASS()
class EDITORSYSCONFIGASSISTANT_API UEditorSysConfigAssistantSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** Can be called on any thread */
	void AddIssue(const FEditorSysConfigIssue& Issue);
	/** Can be called on any thread */
	TArray<TSharedPtr<FEditorSysConfigIssue>> GetIssues();

	/** Must be called on the game thread */
	void ApplySysConfigChanges(TArrayView<const TSharedPtr<FEditorSysConfigIssue>> Issues);

	/** Must be called on the game thread */
	void DismissSystemConfigNotification();

private:
	void HandleModularFeatureRegistered(const FName& InFeatureName, IModularFeature* InFeature);
	void HandleModularFeatureUnregistered(const FName& InFeatureName, IModularFeature* InFeature);
	
	void HandleAssistantInitializationEvent();

	/** Must be called on the game thread */
	static void NotifySystemConfigIssues();

	/** Must be called on the game thread */
	void NotifyRestart(bool bApplicationOnly);

	void OnApplicationRestartClicked();
	void OnSystemRestartClicked();
	void OnRestartDismissClicked();

	FRWLock IssuesLock;
	TArray<TSharedPtr<FEditorSysConfigIssue>> Issues;

	TSharedPtr<SNotificationItem> IssueNotificationItem;
	TWeakPtr<SNotificationItem> RestartNotificationItem;
};
