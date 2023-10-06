// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HttpManager.h"

class FHttpThread;

class FAppleHttpManager : public FHttpManager
{
	//~ Begin HttpManager Interface
protected:
	virtual FHttpThreadBase* CreateHttpThread() override;
	//~ End HttpManager Interface
};
