// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class IDDCCleanup
{
public:
	virtual ~IDDCCleanup() = default;
	virtual bool IsFinished() const = 0;
	virtual void WaitBetweenDeletes(bool bWait) = 0;
};
