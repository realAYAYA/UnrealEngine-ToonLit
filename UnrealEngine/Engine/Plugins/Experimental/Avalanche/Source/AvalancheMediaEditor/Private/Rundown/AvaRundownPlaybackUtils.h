// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Rundown/AvaRundownPage.h"

class UAvaRundown;

struct FAvaRundownPlaybackUtils
{
	static bool IsPageIdValid(int32 InPageId)
	{
		return InPageId != FAvaRundownPage::InvalidPageId;
	}
	
	static int32 GetPageIdToPlayNext(const UAvaRundown* InRundown, const FAvaRundownPageListReference& InPageListReference, bool bInPreview, FName InPreviewChannelName);
	
	static TArray<int32> GetPagesToTakeToProgram(const UAvaRundown* InRundown, const TArray<int32>& InSelectedPageIds, FName InPreviewChannel = NAME_None);
};