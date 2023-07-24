// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utils/AddonTools.h"

#include "SyncContext.h"
#include "Model.hpp"

BEGIN_NAMESPACE_UE_AC

// Class to export as a fbx document
class FExporter
{
  public:
	// Constructor
	FExporter();

	// Destructor
	~FExporter();

	// Export the AC model in the specified file
	void DoExport(const ModelerAPI::Model& InModel, const API_IOParams& IOParams);

	// Export the AC model in the specified file
	void DoExport(const ModelerAPI::Model& InModel, const IO::Location& InDestFile);

	static GSErrCode DoChooseDestination(IO::Location* OutDestFile);
};

END_NAMESPACE_UE_AC
