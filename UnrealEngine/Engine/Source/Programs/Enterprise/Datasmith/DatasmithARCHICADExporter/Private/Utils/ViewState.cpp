// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewState.h"
#include "AutoChangeDatabase.h"

BEGIN_NAMESPACE_UE_AC

// Contructor
FViewState::FCutInfo::FCutInfo()
{
	Zap(&CutPlanesInfo);
	GSErrCode err = ACAPI_Environment(APIEnv_Get3DCuttingPlanesID, &CutPlanesInfo, NULL);
	if (err != NoError && err != APIERR_BADPARS)
		UE_AC_TestGSError(err);
}

// Contructor
FViewState::FCutInfo::~FCutInfo()
{
	BMKillHandle((GSHandle*)&CutPlanesInfo.shapes);
}

// Copy
FViewState::FCutInfo& FViewState::FCutInfo::operator=(const FCutInfo& InOther)
{
	if (&InOther != this)
	{
		// Delete old shapes if present
		BMKillHandle((GSHandle*)&CutPlanesInfo.shapes);

		CutPlanesInfo = InOther.CutPlanesInfo;
		CutPlanesInfo.shapes = nullptr;
		BMHandleToHandle((GSHandle)InOther.CutPlanesInfo.shapes, (GSHandle*)&CutPlanesInfo.shapes);
	}

	return *this;
}

// Equality test
bool FViewState::FCutInfo::operator==(const FCutInfo& InOther) const
{
	// clang-format off
	if (CutPlanesInfo.isCutPlanes != InOther.CutPlanesInfo.isCutPlanes ||
		CutPlanesInfo.useCustom != InOther.CutPlanesInfo.useCustom ||
		CutPlanesInfo.customPen != InOther.CutPlanesInfo.customPen ||
		CutPlanesInfo.customMater != InOther.CutPlanesInfo.customMater ||
		CutPlanesInfo.nShapes != InOther.CutPlanesInfo.nShapes)
	{
		return false;
	}

	short iShape;
	UE_AC_Assert(CutPlanesInfo.nShapes == 0 ||
				 (CutPlanesInfo.shapes != nullptr && InOther.CutPlanesInfo.shapes != nullptr));
	for (iShape = 0; iShape < CutPlanesInfo.nShapes; iShape++)
	{
		const API_3DCutShapeType& myShape = (*CutPlanesInfo.shapes)[iShape];
		const API_3DCutShapeType& otherShape = (*InOther.CutPlanesInfo.shapes)[iShape];
		if (myShape.cutStatus != otherShape.cutStatus ||
			myShape.cutPen != otherShape.cutPen ||
			myShape.cutMater != otherShape.cutMater ||
			myShape.pa != otherShape.pa ||
			myShape.pb != otherShape.pb ||
			myShape.pc != otherShape.pc ||
			myShape.pd != otherShape.pd)
		{
			return false;
		}
	}
	// clang-format on

	return true;
}

void FViewState::FCutInfo::NormalizePlanes()
{
	if (CutPlanesInfo.isCutPlanes && CutPlanesInfo.nShapes)
	{
		for (short iShape = 0; iShape < CutPlanesInfo.nShapes; iShape++)
		{
			API_3DCutShapeType& shape = (*CutPlanesInfo.shapes)[iShape];
			// Normalize planes equations
			if ((shape.cutStatus & 0x0007) == 0)
			{
				double l = shape.pa * shape.pa + shape.pb * shape.pb + shape.pc * shape.pc;
				if (l > 0)
				{
					l = 1 / sqrt(l);
					shape.pa *= l;
					shape.pb *= l;
					shape.pc *= l;
				}
				else
				{
					UE_AC_DebugF("CCutPlanes::NormalizePlanes - Vector length is 0\n");
				}
			}
		}
	}
}

FViewState::FViewState()
{
	CollectVisibleLayers();
	GetCamera();
}

// Equality test
bool FViewState::operator==(const FViewState& InOther) const
{
	return CutInfo == InOther.CutInfo && VisibleLayers == InOther.VisibleLayers && CompareCamera(InOther);
}

void FViewState::CollectVisibleLayers()
{
	API_Attribute LayerAttribute;
	Zap(&LayerAttribute);
	LayerAttribute.header.typeID = API_LayerID;

	API_AttributeIndex LayerCount = 0;
	GSErrCode		   GSErr = ACAPI_Attribute_GetNum(API_LayerID, &LayerCount);
	if (GSErr == NoError)
	{
		VisibleLayers.Reserve(LayerCount);

		for (API_AttributeIndex Index = 1; Index <= LayerCount && GSErr == NoError; Index++)
		{
			LayerAttribute.header.index = Index;
			GSErr = ACAPI_Attribute_Get(&LayerAttribute);
			if (GSErr == NoError)
			{
				if ((LayerAttribute.layer.head.flags & APILay_Hidden) == 0)
				{
					VisibleLayers.Add(Index);
				}
			}
			else if (GSErr == APIERR_DELETED)
			{
				GSErr = NoError;
			}
		}
	}

	UE_AC_TestGSError(GSErr);
}

void FViewState::GetCamera()
{
	bool				bHas3DWindow = false;
	FAutoChangeDatabase changeDB(APIWind_3DModelID, &bHas3DWindow);
	if (bHas3DWindow)
	{
		ProjSetsError = ACAPI_Environment(APIEnv_Get3DProjectionSetsID, &ProjSets, NULL);
		if (ProjSetsError != APIERR_BADDATABASE)
		{
			UE_AC_TestGSError(ProjSetsError);
		}
	}
}

bool FViewState::CompareCamera(const FViewState& InOther) const
{
	if (ProjSetsError == NoError && InOther.ProjSetsError == NoError)
	{
		if (ProjSets.isPersp != InOther.ProjSets.isPersp)
		{
			return false;
		}
		if (ProjSets.isPersp)
		{
			if (ProjSets.u.persp.pos.x != InOther.ProjSets.u.persp.pos.x ||
				ProjSets.u.persp.pos.y != InOther.ProjSets.u.persp.pos.y ||
				ProjSets.u.persp.cameraZ != InOther.ProjSets.u.persp.cameraZ ||
				ProjSets.u.persp.targetZ != InOther.ProjSets.u.persp.targetZ ||
				ProjSets.u.persp.distance != InOther.ProjSets.u.persp.distance ||
				ProjSets.u.persp.azimuth != InOther.ProjSets.u.persp.azimuth ||
				ProjSets.u.persp.rollAngle != InOther.ProjSets.u.persp.rollAngle ||
				ProjSets.u.persp.viewCone != InOther.ProjSets.u.persp.viewCone)
			{
				return false;
			}
		}
		else
		{
			if (memcmp(&ProjSets.u.axono.invtranmat, &InOther.ProjSets.u.axono.invtranmat,
					   sizeof(ProjSets.u.axono.invtranmat)) != 0)
			{
				return false;
			}
		}
	}

	return true;
}

END_NAMESPACE_UE_AC
