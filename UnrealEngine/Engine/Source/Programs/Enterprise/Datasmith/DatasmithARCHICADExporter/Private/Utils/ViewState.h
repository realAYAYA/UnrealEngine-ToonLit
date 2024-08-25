// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AddonTools.h"

BEGIN_NAMESPACE_UE_AC

// Schedule an idle task
class FViewState
{
  public:
	class FCutInfo
	{
	  public:
		// Contructor
		FCutInfo();

		// Contructor
		~FCutInfo();

		// Copy
		FCutInfo& operator=(const FCutInfo& InOther);

		// Equality test
		bool operator==(const FCutInfo& InOther) const;

	  private:
		void NormalizePlanes();

		API_3DCutPlanesInfo CutPlanesInfo;
	};

	FViewState();

	// Equality test
	bool operator==(const FViewState& InOther) const;

  private:
	void CollectVisibleLayers();

	void GetCamera();

	bool CompareCamera(const FViewState& InOther) const;

	FCutInfo					 CutInfo;
	TArray< GS::Int32 >			VisibleLayers;
	API_3DProjectionInfo		 ProjSets;
	GSErrCode					 ProjSetsError;
};

END_NAMESPACE_UE_AC
