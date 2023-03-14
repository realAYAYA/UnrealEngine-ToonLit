// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utils/AddonTools.h"

BEGIN_NAMESPACE_UE_AC

class FReportDialog;

class FReportWindow
{
  public:
	static void Create();
	static void Delete();
};

class FTraceListener : public ITraceListener
{
  public:
	static FTraceListener& Get();

	static void Delete();

    bool HasUpdate() const { return bHasUpdate; }
    
    GS::UniString GetTraces();
    
    void Clear();

private:
	FTraceListener();
    ~FTraceListener();

	virtual void NewTrace(EP2DB InTraceLevel, const utf8_string& InMsg) override;

	volatile bool bHasUpdate = false;
	utf8_string Traces;

	// Control access on this object (for queue operations)
	GS::Lock AccessControl;

	// Condition variable
	GS::Condition CV;
};

END_NAMESPACE_UE_AC
