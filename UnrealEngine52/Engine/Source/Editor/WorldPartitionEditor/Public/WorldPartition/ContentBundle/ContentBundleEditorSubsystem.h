// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "EditorSubsystem.h"
#include "WorldPartition/ContentBundle/ContentBundleEditorSubsystemInterface.h"
#include "IActorEditorContextClient.h"

#include "ContentBundleEditorSubsystem.generated.h"

class FContentBundleEditor;
class UContentBundleDescriptor;
class UContentBundleEditorSubsystem;

UCLASS(Within = ContentBundleEditorSubsystem)
class UContentBundleEditorSubsystemModule : public UObject
{
	GENERATED_BODY()

public:
	//~ Begin UObject interface
	virtual void BeginDestroy() override { check(!IsInitialize()); Super::BeginDestroy(); }
	//~ End UObject interface

	void Initialize() { DoInitialize(); bIsInitialized = true; }
	void Deinitialize() { bIsInitialized = false; DoDenitialize(); }

	bool IsInitialize() const { return bIsInitialized; }

	UContentBundleEditorSubsystem* GetSubsystem() const { return GetOuterUContentBundleEditorSubsystem(); }

protected:
	virtual void DoInitialize() {};
	virtual void DoDenitialize() {};

private:
	bool bIsInitialized = false;
};

UCLASS()
class UContentBundleEditingSubmodule : public UContentBundleEditorSubsystemModule, public IActorEditorContextClient
{
	GENERATED_BODY()

public:
	//~ Begin IActorEditorContextClient interface
	virtual void OnExecuteActorEditorContextAction(UWorld* InWorld, const EActorEditorContextAction& InType, AActor* InActor = nullptr) override;
	virtual bool GetActorEditorContextDisplayInfo(UWorld* InWorld, FActorEditorContextClientDisplayInfo& OutDiplayInfo) const override;
	virtual bool CanResetContext(UWorld* InWorld) const override { return true; }
	virtual TSharedRef<SWidget> GetActorEditorContextWidget(UWorld* InWorld) const override;
	virtual FOnActorEditorContextClientChanged& GetOnActorEditorContextClientChanged() override { return ActorEditorContextClientChanged; }
	//~ End IActorEditorContextClient interface

	bool ActivateContentBundleEditing(TSharedPtr<FContentBundleEditor>& ContentBundleEditor);
	bool DeactivateContentBundleEditing(TSharedPtr<FContentBundleEditor>& ContentBundleEditor);
	bool DeactivateCurrentContentBundleEditing();
	bool IsEditingContentBundle() const;
	bool IsEditingContentBundle(const TSharedPtr<FContentBundleEditor>& ContentBundleEditor) const;
	bool IsEditingContentBundle(const FGuid& ContentBundleGuid) const;
	TSharedPtr<FContentBundleEditor> GetEditorContentBundle(const FGuid& ContentBundleGuid) const;

protected:
	//~ Begin UContentBundleEditorSubsystemModule interface
	virtual void DoInitialize() override;
	virtual void DoDenitialize() override;
	//~ End UContentBundleEditorSubsystemModule interface

private:
	void PushContentBundleEditing();
	void PopContentBundleEditing();
	void StartEditing(TSharedPtr<FContentBundleEditor>& ContentBundleEditor);
	void StopEditing(TSharedPtr<FContentBundleEditor>& ContentBundleEditor);
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;

	UPROPERTY()
	TArray<FGuid> EditingContentBundlesStack;

	UPROPERTY()
	FGuid EditingContentBundleGuid;

	// Used for undo/redo
	FGuid PreUndoRedoEditingContentBundleGuid;

	FOnActorEditorContextClientChanged ActorEditorContextClientChanged;
};

UCLASS()
class WORLDPARTITIONEDITOR_API UContentBundleEditorSubsystem : public UEditorSubsystem, public IContentBundleEditorSubsystemInterface
{
	GENERATED_BODY()

public:
	static UContentBundleEditorSubsystem* Get() { return StaticCast<UContentBundleEditorSubsystem*>(IContentBundleEditorSubsystemInterface::Get()); }

	UContentBundleEditorSubsystem() {}

	//~ Begin UEditorSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End UEditorSubsystem interface

	//~ Begin IContentBundleEditorSubsystemInterface interface
	virtual void NotifyContentBundleAdded(const FContentBundleEditor* ContentBundle) override { OnContentBundleAdded().Broadcast(ContentBundle); }
	virtual void NotifyContentBundleRemoved(const FContentBundleEditor* ContentBundle) override { OnContentBundleRemoved().Broadcast(ContentBundle); }
	virtual void NotifyContentBundleInjectedContent(const FContentBundleEditor* ContentBundle) override;
	virtual void NotifyContentBundleRemovedContent(const FContentBundleEditor* ContentBundle) override;
	virtual void NotifyContentBundleChanged(const FContentBundleEditor* ContentBundle) override { OnContentBundleChanged().Broadcast(ContentBundle); }
	//~ End IContentBundleEditorSubsystemInterface interface

	UWorld* GetWorld() const;

	UContentBundleEditingSubmodule* GetEditingSubmodule() { return ContentBundleEditingSubModule; }

	TSharedPtr<FContentBundleEditor> GetEditorContentBundleForActor(const AActor* Actor);

	TArray<TSharedPtr<FContentBundleEditor>> GetEditorContentBundles();
	TSharedPtr<FContentBundleEditor> GetEditorContentBundle(const UContentBundleDescriptor* ContentBundleDescriptor) const;
	TSharedPtr<FContentBundleEditor> GetEditorContentBundle(const FGuid& ContentBundleGuid) const;

	void SelectActors(FContentBundleEditor& EditorContentBundle);
	void DeselectActors(FContentBundleEditor& EditorContentBundle);

	void ReferenceAllActors(FContentBundleEditor& EditorContentBundle);
	void UnreferenceAllActors(FContentBundleEditor& EditorContentBundle);

	bool IsEditingContentBundle() const;
	bool IsEditingContentBundle(const FGuid& ContentBundleGuid) const;
	bool ActivateContentBundleEditing(TSharedPtr<FContentBundleEditor>& ContentBundleEditor) const;
	bool DeactivateContentBundleEditing(TSharedPtr<FContentBundleEditor>& ContentBundleEditor) const;
	bool IsContentBundleEditingActivated(TSharedPtr<FContentBundleEditor>& ContentBundleEditor) const;

	DECLARE_EVENT_OneParam(UContentBundleEditorSubsystem, FOnContentBundleChanged, const FContentBundleEditor*);
	FOnContentBundleChanged& OnContentBundleChanged() { return ContentBundleChanged; }

	DECLARE_EVENT_OneParam(UContentBundleEditorSubsystem, FOnContentBundleAdded, const FContentBundleEditor*);
	FOnContentBundleAdded& OnContentBundleAdded() { return ContentBundleAdded; }

	DECLARE_EVENT_OneParam(UContentBundleEditorSubsystem, FOnContentBundleRemoved, const FContentBundleEditor*);
	FOnContentBundleRemoved& OnContentBundleRemoved() { return ContentBundleRemoved; }

private:
	void SelectActorsInternal(FContentBundleEditor& EditorContentBundle, bool bSelect);

	UPROPERTY()
	TObjectPtr<UContentBundleEditingSubmodule> ContentBundleEditingSubModule;

	FOnContentBundleChanged ContentBundleChanged;
	FOnContentBundleAdded ContentBundleAdded;
	FOnContentBundleRemoved ContentBundleRemoved;
};