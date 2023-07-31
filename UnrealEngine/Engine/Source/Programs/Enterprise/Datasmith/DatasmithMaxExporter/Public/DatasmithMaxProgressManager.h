// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "DatasmithProgressManager.h"

class FDatasmithMaxProgressManager : public IDatasmithProgressManager
{
public:
	FDatasmithMaxProgressManager();

	virtual ~FDatasmithMaxProgressManager() override;

	virtual void ProgressEvent(float InProgressRatio, const TCHAR* InProgressString) override;

	void SetMainMessage(const TCHAR* InProgressMessage);

	void SetProgressStart(float InProgressStart)
	{
		ProgressStart = InProgressStart;
	}

	void SetProgressEnd(float InProgressEnd)
	{
		ProgressEnd = InProgressEnd;
	}

private:
	float ProgressStart;
	float ProgressEnd;
	int32 ProgressBarCounter;
};