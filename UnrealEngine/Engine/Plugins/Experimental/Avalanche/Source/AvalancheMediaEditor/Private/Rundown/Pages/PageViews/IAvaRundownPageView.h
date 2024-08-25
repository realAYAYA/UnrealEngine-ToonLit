// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaType.h"
#include "Delegates/Delegate.h"
#include "Templates/SharedPointer.h"

class FReply;
class FText;
class UAvaRundown;
struct FAssetData;
struct FSoftObjectPath;

enum class EAvaRundownPageActionState : uint8
{
	None,
	Requested,
	Completed,
	Cancelled,
};

enum class EAvaRundownPageViewSelectionChangeType : uint8
{
	Deselect,
	ReplaceSelection,
	AddToSelection
};

class IAvaRundownPageView : public IAvaTypeCastable, public TSharedFromThis<IAvaRundownPageView>
{
public:
	UE_AVA_INHERITS(IAvaRundownPageView, IAvaTypeCastable);

	virtual UAvaRundown* GetRundown() const = 0;
	
	virtual int32 GetPageId() const = 0;
	virtual FText GetPageIdText() const = 0;
	virtual FText GetPageNameText() const = 0;
	virtual FText GetPageTransitionLayerNameText() const = 0;

	virtual FText GetPageSummary() const = 0;
	virtual FText GetPageDescription() const = 0;

	virtual bool IsTemplate() const = 0;

	virtual bool HasObjectPath(const UAvaRundown* InRundown) const = 0;
	virtual FSoftObjectPath GetObjectPath(const UAvaRundown* InRundown) const = 0;
	virtual FText GetObjectName(const UAvaRundown* InRundown) const = 0;
	virtual void OnObjectChanged(const FAssetData& InAssetData) = 0;

	virtual bool Rename(const FText& InNewName) = 0;
	virtual bool RenameFriendlyName(const FText& InNewName) = 0;
	
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPageAction, EAvaRundownPageActionState);
	virtual FOnPageAction& GetOnRename() = 0;
	virtual FOnPageAction& GetOnRenumber() = 0;

	virtual FReply OnAssetStatusButtonClicked() = 0;
	virtual bool CanChangeAssetStatus() const = 0;

	virtual FReply OnPreviewButtonClicked() = 0;
	virtual bool CanPreview() const = 0;

	virtual FReply OnPlayButtonClicked() = 0;
	virtual bool CanPlay() const = 0;

	virtual bool IsPageSelected() const = 0;
	virtual bool SetPageSelection(EAvaRundownPageViewSelectionChangeType InSelectionChangeType) = 0;
};
