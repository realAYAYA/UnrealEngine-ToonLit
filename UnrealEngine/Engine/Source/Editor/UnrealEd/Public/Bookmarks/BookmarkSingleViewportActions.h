// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Bookmarks/BookMarkTypeActions.h"

/**
 * A bookmark action which jumps a single viewport to a single bookmark
 */
class FBookmarkSingleViewportActions : public FBookMarkTypeActions
{
public:

	virtual void JumpToBookmark(UBookmarkBase* InBookmark, const TSharedPtr<struct FBookmarkBaseJumpToSettings> InSettings, FEditorViewportClient& InViewportClient) override
	{
		if (UBookMark* Bookmark = Cast<UBookMark>(InBookmark))
		{
			ApplyBookmarkToViewportClient(Bookmark, &InViewportClient);
		}
	}
};
