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
class UContentBundleEditionSubmodule : public UContentBundleEditorSubsystemModule, public IActorEditorContextClient
{
	GENERATED_BODY()

public:
	//~ Begin IActorEditorContextClient interface
	virtual void OnExecuteActorEditorContextAction(UWorld* InWorld, const EActorEditorContextAction& InType, AActor* InActor = nullptr) override;
	virtual bool GetActorEditorContextDisplayInfo(UWorld* InWorld, FActorEditorContextClientDisplayInfo& OutDiplayInfo) const override;
	virtual bool CanResetContext(UWorld* InWorld) const override;
	virtual TSharedRef<SWidget> GetActorEditorContextWidget(UWorld* InWorld) const override;
	virtual FOnActorEditorContextClientChanged& GetOnActorEditorContextClientChanged() override { return ActorEditorContextClientChanged; }
	//~ End IActorEditorContextClient interface

	bool ActivateContentBundleEditing(TSharedPtr<FContentBundleEditor>& ContentBundleEditor);
	bool DeactivateContentBundleEditing(TSharedPtr<FContentBundleEditor>& ContentBundleEditor);
	bool DeactivateCurrentContentBundleEditing();
	bool IsEditingContentBundle() const { return EditingContentBundle.IsValid(); }
	bool IsEditingContentBundle(const FContentBundleEditor* ContentBundleEditor) const { return EditingContentBundle.Pin().Get() == ContentBundleEditor; }

protected:
	//~ Begin UContentBundleEditorSubsystemModule interface
	virtual void DoInitialize() override;
	virtual void DoDenitialize() override;
	//~ End UContentBundleEditorSubsystemModule interface

private:
	void PushContentBundleEditing();
	void PopContentBundleEditing();

	TArray<TWeakPtr<FContentBundleEditor>> ActiveContentBundlesStack;

	TWeakPtr<FContentBundleEditor> EditingContentBundle;

	FOnActorEditorContextClientChanged ActorEditorContextClientChanged;
};

UCLASS()
class WORLDPARTITIONEDITOR_API UContentBundleEditorSubsystem : public UEditorSubsystem, public IContentBundleEditorSubsystemInterface
{
	GENERATED_BODY()

public:
	static UContentBundleEditorSubsystem* Get() { return StaticCast<UContentBundleEditorSubsystem*>(IContentBundleEditorSubsystemInterface::Get()); }

	UContentBundleEditorSubsystem();

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

	UContentBundleEditionSubmodule* GetEditionSubmodule() { return ContentBundleEditionSubModule; }

	TSharedPtr<FContentBundleEditor> GetEditorContentBundleForActor(const AActor* Actor);

	TArray<TSharedPtr<FContentBundleEditor>> GetEditorContentBundles();
	TSharedPtr<FContentBundleEditor> GetEditorContentBundle(const UContentBundleDescriptor* ContentBundleDescriptor) const;

	bool HasContentBundle(const UContentBundleDescriptor* ContentBundleDescriptor) const;

	void SelectActors(FContentBundleEditor& EditorContentBundle);
	void DeselectActors(FContentBundleEditor& EditorContentBundle);

	void ReferenceAllActors(FContentBundleEditor& EditorContentBundle);
	void UnreferenceAllActors(FContentBundleEditor& EditorContentBundle);

	bool ActivateContentBundleEditing(TSharedPtr<FContentBundleEditor>& ContentBundleEditor) const;
	bool DectivateContentBundleEditing(TSharedPtr<FContentBundleEditor>& ContentBundleEditor) const;
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
	TObjectPtr<UContentBundleEditionSubmodule> ContentBundleEditionSubModule;

	FOnContentBundleChanged ContentBundleChanged;
	FOnContentBundleAdded ContentBundleAdded;
	FOnContentBundleRemoved ContentBundleRemoved;
};