// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Rundown/AvaRundownEditorDefines.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateEnums.h"
#include "Widgets/SCompoundWidget.h"

class FAvaRundownEditor;
class FReply;
class FText;
class SAvaRundownPageRemoteControlProps;
class SAvaRundownRCControllerPanel;
struct FAvaRundownPage;
struct FSoftObjectPath;
struct FSlateBrush;

class SAvaRundownPageDetails : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAvaRundownPageDetails) {}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const TSharedPtr<FAvaRundownEditor>& InRundownEditor);

	virtual ~SAvaRundownPageDetails() override;

	void OnPageEvent(const TArray<int32>& InSelectedPageIds, UE::AvaRundown::EPageEvent InPageEvent);
	void OnPageSelectionChanged(const TArray<int32>& InSelectedPageIds);
	void OnManagedInstanceCacheEntryInvalidated(const FSoftObjectPath& InAssetPath);

protected:
	FReply ToggleExposedPropertiesVisibility();

	const FSlateBrush* GetExposedPropertiesVisibilityBrush() const;

	const FAvaRundownPage& GetSelectedPage() const;
	FAvaRundownPage& GetMutableSelectedPage() const;

	void RefreshSelectedPage();

	bool HasSelectedPage() const;

	FText GetPageId() const;

	/** Only update page id on commit. */
	void OnPageIdCommitted(const FText& InNewText, ETextCommit::Type InCommitType);

	FText GetPageDescription() const;

	/** Update page name live. */
	void OnPageNameChanged(const FText& InNewText);

	FReply DuplicateSelectedPage();

private:
	TWeakPtr<FAvaRundownEditor> RundownEditorWeak;

	TSharedPtr<SAvaRundownPageRemoteControlProps> RemoteControlProps;

	TSharedPtr<SAvaRundownRCControllerPanel> RCControllerPanel;

	bool bRefreshSelectedPageQueued = false;

	int32 ActivePageId;
};
