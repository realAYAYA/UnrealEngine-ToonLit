// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/Layout/SBox.h"

struct FIKRigLogger;
class IMessageLogListing;
class FIKRetargetEditorController;

class SIKRigOutputLog : public SBox
{
	
public:
	
	SLATE_BEGIN_ARGS(SIKRigOutputLog) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FName& InLogName);

	void ClearLog() const;
	
private:
	
	/** the output log */
	TSharedPtr<SWidget> OutputLogWidget;
	TSharedPtr<IMessageLogListing> MessageLogListing;
};