// Copyright Epic Games, Inc. All Rights Reserved.

#include "AliasModelToTechSoftConverter.h"

#include "Hal/PlatformMemory.h"
#include "Math/Color.h"
#include "MeshDescriptionHelper.h"
#include "TechSoftInterface.h"
#include "TechSoftUtils.h"
#include "TUniqueTechSoftObj.h"


#ifdef USE_OPENMODEL

// Alias API wrappes object in AlObjects. This is an abstract base class which holds a reference to an anonymous data structure.
// The only way to compare two AlObjects is to compare their data structure. That is the reason why private fields are made public. 
#define private public
#include "AlCurve.h"
#include "AlDagNode.h"
#include "AlShell.h"
#include "AlShellNode.h"
#include "AlSurface.h"
#include "AlSurfaceNode.h"
#include "AlTrimBoundary.h"
#include "AlTrimCurve.h"
#include "AlTrimRegion.h"
#include "AlTM.h"

namespace UE_DATASMITHWIRETRANSLATOR_NAMESPACE
{

#ifdef USE_TECHSOFT_SDK

namespace AliasToTechSoftUtils
{

enum EAxis { U, V };

template<typename Surface_T>
A3DSurfBase* AddNURBSSurface(const Surface_T& AliasSurface, EAliasObjectReference InObjectReference, const AlMatrix4x4& InAlMatrix)
{
	CADLibrary::TUniqueTSObj<A3DSurfNurbsData> NurbsSurfaceData;

	NurbsSurfaceData->m_eKnotType = A3DEKnotType::kA3DKnotTypeUnspecified;
	NurbsSurfaceData->m_eSurfaceForm = A3DEBSplineSurfaceForm::kA3DBSplineSurfaceFormUnspecified;

	NurbsSurfaceData->m_uiUDegree = AliasSurface.uDegree();
	NurbsSurfaceData->m_uiVDegree = AliasSurface.vDegree();

	NurbsSurfaceData->m_uiUCtrlSize = AliasSurface.uNumberOfCVsInclMultiples();
	NurbsSurfaceData->m_uiVCtrlSize = AliasSurface.vNumberOfCVsInclMultiples();
	NurbsSurfaceData->m_uiUKnotSize = AliasSurface.realuNumberOfKnots() + 2;
	NurbsSurfaceData->m_uiVKnotSize = AliasSurface.realvNumberOfKnots() + 2;

	TArray<A3DDouble> UNodalVector;
	TArray<A3DDouble> VNodalVector;

	TFunction<void(EAxis, TArray<A3DDouble>&)> FillNodalVector = [&](EAxis Axis, TArray<A3DDouble>& OutNodalVector)
	{
		TArray<double> NodalVector;
		if(Axis == EAxis::U)
		{
			OutNodalVector.Reserve(NurbsSurfaceData->m_uiUKnotSize);
			NodalVector.SetNumUninitialized(NurbsSurfaceData->m_uiUKnotSize - 2);
			AliasSurface.realuKnotVector(NodalVector.GetData());
		}
		else
		{
			OutNodalVector.Reserve(NurbsSurfaceData->m_uiVKnotSize);
			NodalVector.SetNumUninitialized(NurbsSurfaceData->m_uiVKnotSize - 2);
			AliasSurface.realvKnotVector(NodalVector.GetData());
		}

		OutNodalVector.Add(NodalVector[0]);
		for(double Value : NodalVector)
		{
			OutNodalVector.Add(Value);
		}
		OutNodalVector.Add(NodalVector.Last());

		if (Axis == EAxis::U)
		{
			NurbsSurfaceData->m_pdUKnots = OutNodalVector.GetData();
		}
		else
		{
			NurbsSurfaceData->m_pdVKnots = OutNodalVector.GetData();
		}
	};

	FillNodalVector(EAxis::U, UNodalVector);
	FillNodalVector(EAxis::V, VNodalVector);

	const int32 CoordinateCount = NurbsSurfaceData->m_uiUCtrlSize * NurbsSurfaceData->m_uiVCtrlSize * 4;
	TArray<double> HomogeneousPoles;
	HomogeneousPoles.SetNumUninitialized(CoordinateCount);

	if (InObjectReference == EAliasObjectReference::WorldReference)
	{
		AliasSurface.CVsWorldPositionInclMultiples(HomogeneousPoles.GetData());
	}
	else if (InObjectReference == EAliasObjectReference::ParentReference)
	{
		AlTM TranformMatrix(InAlMatrix);
		AliasSurface.CVsAffectedPositionInclMultiples(TranformMatrix, HomogeneousPoles.GetData());
	}
	else  // EAliasObjectReference::LocalReference
	{
		AliasSurface.CVsUnaffectedPositionInclMultiples(HomogeneousPoles.GetData());
	}

	int32 PoleCount = NurbsSurfaceData->m_uiUCtrlSize * NurbsSurfaceData->m_uiVCtrlSize;
	TArray<A3DDouble> Weights;
	TArray<A3DVector3dData> ControlPoints;
	ControlPoints.SetNum(PoleCount);
	Weights.SetNum(PoleCount);
	A3DVector3dData* ControlPointPtr = ControlPoints.GetData();
	A3DDouble* WeightPtr = Weights.GetData();
	
	NurbsSurfaceData->m_pdWeights = WeightPtr;
	NurbsSurfaceData->m_pCtrlPts = ControlPointPtr;
	
	TFunction<void(const double*, A3DVector3dData&, A3DDouble&)> SetA3DPole = [](const double* HomogeneousPole, A3DVector3dData & OutTSPoint, A3DDouble & OutWeight)
	{
		OutTSPoint.m_dX = HomogeneousPole[0] * 10.;  // cm (Alias MetricUnit) to mm
		OutTSPoint.m_dY = HomogeneousPole[1] * 10.;
		OutTSPoint.m_dZ = HomogeneousPole[2] * 10.;
		OutWeight = HomogeneousPole[3];
	};

	double* AliasPoles = HomogeneousPoles.GetData();
	for (int32 Index = 0; Index < PoleCount; ++Index, AliasPoles += 4, ++ControlPointPtr, ++WeightPtr)
	{
		SetA3DPole(AliasPoles, *ControlPointPtr, *WeightPtr);
	}

	return CADLibrary::TechSoftInterface::CreateSurfaceNurbs(*NurbsSurfaceData);
}

} // ns AliasToTechsoftUtils

A3DCrvBase* FAliasModelToTechSoftConverter::CreateCurve(const AlTrimCurve& AliasTrimCurve)
{
	CADLibrary::TUniqueTSObj<A3DCrvNurbsData> NurbsCurveData;

	NurbsCurveData->m_bIs2D = true;
	NurbsCurveData->m_bRational = true;
	NurbsCurveData->m_uiDegree = AliasTrimCurve.degree();

	int ControlPointCount = AliasTrimCurve.numberOfCVs();
	int32 KnotCount = AliasTrimCurve.realNumberOfKnots();

	using AlPoint = double[3];

	TArray<AlPoint> AliasPoles;
	TArray<double> AliasNodalVector;
	AliasNodalVector.SetNum(ControlPointCount);
	AliasPoles.SetNumUninitialized(ControlPointCount*4);

	// Notice that each CV has three coordinates - the three coordinates describe 2D parameter space, with a homogeneous coordinate.
	// Each control point is u, v and w, where u and v are parameter space and w is the homogeneous coordinate.
	AliasTrimCurve.CVsUVPosition(AliasNodalVector.GetData(), AliasPoles.GetData());

	AliasNodalVector.SetNum(KnotCount);
	AliasTrimCurve.realKnotVector(AliasNodalVector.GetData());

	TArray<A3DDouble> NodalVector;
	NodalVector.Reserve(KnotCount+2);
	NodalVector.Add(AliasNodalVector[0]);
	for (double Value : AliasNodalVector)
	{
		NodalVector.Add(Value);
	}
	NodalVector.Add(AliasNodalVector.Last());

	TArray<double> Weights;
	Weights.SetNumUninitialized(ControlPointCount);

	AliasNodalVector.SetNumUninitialized(KnotCount);

	TArray<A3DVector3dData> ControlPointArray;
	TArray<A3DDouble> WeightArray;

	ControlPointArray.SetNumUninitialized(ControlPointCount);
	WeightArray.SetNumUninitialized(ControlPointCount);

	A3DVector3dData* ControlPointPtr = ControlPointArray.GetData();
	A3DDouble* WeightPtr = WeightArray.GetData();
	AlPoint* AliasPolePtr = AliasPoles.GetData();

	TFunction<void(const AlPoint&, A3DVector3dData&, A3DDouble&)> SetA3DPole = [](const AlPoint& AliasPole, A3DVector3dData& OutTSPoint, A3DDouble& OutWeight)
	{
		OutTSPoint.m_dX = AliasPole[0];
		OutTSPoint.m_dY = AliasPole[1];
		OutTSPoint.m_dZ = 0;
		OutWeight = AliasPole[2];
	};

	for (int32 Index = 0; Index < ControlPointCount; ++Index, ++AliasPolePtr, ++ControlPointPtr, ++WeightPtr)
	{
		SetA3DPole(*AliasPolePtr, *ControlPointPtr, *WeightPtr);
	}

	NurbsCurveData->m_eKnotType = kA3DKnotTypeUnspecified;
	NurbsCurveData->m_eCurveForm = kA3DBSplineCurveFormUnspecified;

	NurbsCurveData->m_pCtrlPts = ControlPointArray.GetData();
	NurbsCurveData->m_uiCtrlSize = ControlPointArray.Num();

	NurbsCurveData->m_pdWeights = WeightArray.GetData();
	NurbsCurveData->m_uiWeightSize = WeightArray.Num();

	NurbsCurveData->m_pdKnots = NodalVector.GetData();
	NurbsCurveData->m_uiKnotSize = NodalVector.Num();

	return CADLibrary::TechSoftInterface::CreateCurveNurbs(*NurbsCurveData);
}

A3DTopoCoEdge* FAliasModelToTechSoftConverter::CreateEdge(const AlTrimCurve& TrimCurve)
{
	A3DCrvBase* NurbsCurvePtr = CreateCurve(TrimCurve);
	if (NurbsCurvePtr == nullptr)
	{
		return nullptr;
	}

	A3DTopoEdge* EdgePtr = CADLibrary::TechSoftUtils::CreateTopoEdge();
	if (EdgePtr == nullptr)
	{
		return nullptr;
	}

	CADLibrary::TUniqueTSObj<A3DTopoCoEdgeData> CoEdgeData;

	CoEdgeData->m_pUVCurve = NurbsCurvePtr;
	CoEdgeData->m_pEdge = EdgePtr;
	CoEdgeData->m_ucOrientationWithLoop = TrimCurve.isReversed();
	CoEdgeData->m_ucOrientationUVWithLoop = 1;

	A3DTopoCoEdge* CoEdgePtr = CADLibrary::TechSoftInterface::CreateTopoCoEdge(*CoEdgeData);

	// Only TrimCurve with twin need to be in the map used in LinkEdgesLoop 
	TUniquePtr<AlTrimCurve> TwinCurve(TrimCurve.getTwinCurve());
	if (TwinCurve.IsValid())
	{
		AlEdgeToTSCoEdge.Add(TrimCurve.fSpline, CoEdgePtr);
	}
	return CoEdgePtr;
}

A3DTopoLoop* FAliasModelToTechSoftConverter::CreateTopoLoop(const AlTrimBoundary& TrimBoundary)
{
	TArray<A3DTopoCoEdge*> Edges;
	Edges.Reserve(20);

	for (TUniquePtr<AlTrimCurve> TrimCurve(TrimBoundary.firstCurve()); TrimCurve.IsValid(); TrimCurve = TUniquePtr<AlTrimCurve>(TrimCurve->nextCurve()))
	{
		A3DTopoCoEdge* Edge = CreateEdge(*TrimCurve);
		if (Edge != nullptr)
		{
			Edges.Add(Edge);
		}
	}

	if (Edges.Num() == 0)
	{
		return nullptr;
	}

	CADLibrary::TUniqueTSObj<A3DTopoLoopData> LoopData;

	LoopData->m_ppCoEdges = Edges.GetData();
	LoopData->m_uiCoEdgeSize = Edges.Num();
	LoopData->m_ucOrientationWithSurface = 1;

	return CADLibrary::TechSoftInterface::CreateTopoLoop(*LoopData);
}

void FAliasModelToTechSoftConverter::LinkEdgesLoop(const AlTrimBoundary& TrimBoundary)
{
	for (TUniquePtr<AlTrimCurve> TrimCurve(TrimBoundary.firstCurve()); TrimCurve.IsValid(); TrimCurve = TUniquePtr<AlTrimCurve>(TrimCurve->nextCurve()))
	{
		A3DTopoCoEdge** Edge = AlEdgeToTSCoEdge.Find(TrimCurve->fSpline);
		if (Edge == nullptr)
		{
			continue;
		}

		// Link edges
		TUniquePtr<AlTrimCurve> TwinCurve(TrimCurve->getTwinCurve());
		if (TwinCurve.IsValid())
		{
			if (A3DTopoCoEdge** TwinEdge = AlEdgeToTSCoEdge.Find(TwinCurve->fSpline))
			{
				if (TwinEdge != nullptr)
				{
					CADLibrary::TechSoftInterface::LinkCoEdges(*Edge, *TwinEdge);
					break;
				}
			}
		}
	}
}

A3DTopoFace* FAliasModelToTechSoftConverter::AddTrimRegion(const AlTrimRegion& InTrimRegion, const FColor& Color, EAliasObjectReference InObjectReference, const AlMatrix4x4& InAlMatrix)
{
	A3DSurfBase* CarrierSurface = AliasToTechSoftUtils::AddNURBSSurface(InTrimRegion, InObjectReference, InAlMatrix);
	if (CarrierSurface == nullptr)
	{
		return nullptr;
	}

	TArray<A3DTopoLoop*> Loops;
	Loops.Reserve(5);

	for (TUniquePtr<AlTrimBoundary> TrimBoundary(InTrimRegion.firstBoundary()); TrimBoundary.IsValid(); TrimBoundary = TUniquePtr<AlTrimBoundary>(TrimBoundary->nextBoundary()))
	{
		A3DTopoLoop* Loop = CreateTopoLoop(*TrimBoundary);
		if (Loop != nullptr)
		{
			Loops.Add(Loop);
			LinkEdgesLoop(*TrimBoundary);
		}
	}

	if (Loops.Num() == 0)
	{
		return nullptr;
	}

	CADLibrary::TUniqueTSObj<A3DTopoFaceData> Face;
	Face->m_pSurface = CarrierSurface;
	Face->m_bHasTrimDomain = false;
	Face->m_ppLoops = Loops.GetData();
	Face->m_uiLoopSize = Loops.Num();
	Face->m_uiOuterLoopIndex = 0;
	Face->m_dTolerance = 0.01; //mm

	A3DTopoFace* FacePtr = CADLibrary::TechSoftInterface::CreateTopoFace(*Face);

	CADLibrary::TechSoftUtils::SetEntityGraphicsColor(FacePtr, Color);

	return FacePtr;
}
#endif

bool FAliasModelToTechSoftConverter::AddBRep(AlDagNode& DagNode, const FColor& Color, EAliasObjectReference InObjectReference)
{
#ifdef USE_TECHSOFT_SDK
	AlEdgeToTSCoEdge.Empty();

	AlMatrix4x4 AlMatrix;
	if (InObjectReference == EAliasObjectReference::ParentReference)
	{
		DagNode.localTransformationMatrix(AlMatrix);
	}

	TArray<A3DTopoFace*> TSFaces;
	TSFaces.Reserve(100);

	AlObjectType objectType = DagNode.type();
	switch (objectType)
	{

	case kShellNodeType:
	{
		if (AlShellNode* ShellNode = DagNode.asShellNodePtr())
		{
			TUniquePtr<AlShell> AliasShell(ShellNode->shell());
			if(AlIsValid(AliasShell.Get()))
			{
				for (TUniquePtr<AlTrimRegion> TrimRegion(AliasShell->firstTrimRegion()); TrimRegion.IsValid(); TrimRegion = TUniquePtr<AlTrimRegion>(TrimRegion->nextRegion()))
				{
					A3DTopoFace* TSFace = AddTrimRegion(*TrimRegion, Color, InObjectReference, AlMatrix);
					if (TSFace != nullptr)
					{
						TSFaces.Add(TSFace);
					}
				}
			}
		}
		break;
	}

	case kSurfaceNodeType:
	{
		if (AlSurfaceNode* SurfaceNode = DagNode.asSurfaceNodePtr())
		{
			TUniquePtr<AlSurface> AliasSurface(SurfaceNode->surface());
			if (AlIsValid(AliasSurface.Get()))
			{
				TUniquePtr<AlTrimRegion> TrimRegion(AliasSurface->firstTrimRegion());
				if (TrimRegion.IsValid())
				{
					for (; TrimRegion.IsValid(); TrimRegion = TUniquePtr<AlTrimRegion>(TrimRegion->nextRegion()))
					{
						A3DTopoFace* TSFace = AddTrimRegion(*TrimRegion, Color, InObjectReference, AlMatrix);
						if (TSFace != nullptr)
						{
							TSFaces.Add(TSFace);
						}
					}
					break;
				}

				A3DSurfBase* TSSurface = AliasToTechSoftUtils::AddNURBSSurface(*AliasSurface, InObjectReference, AlMatrix);
				if (TSSurface != nullptr)
				{
					A3DTopoFace* TSFace = CADLibrary::TechSoftUtils::CreateTopoFaceWithNaturalLoop(TSSurface);
					CADLibrary::TechSoftUtils::SetEntityGraphicsColor(TSFace, Color);

					if (TSFace != nullptr)
					{
						TSFaces.Add(TSFace);
					}
				}
			}
		}
		break;
	}
	default:
		break;
	}

	if (TSFaces.IsEmpty())
	{
		return false;
	}

	A3DTopoShell* TopoShellPtr = nullptr;
	{
		TArray<A3DUns8> FaceOrientations;
		FaceOrientations.Init(true, TSFaces.Num());

		CADLibrary::TUniqueTSObj<A3DTopoShellData> TopoShellData;
		TopoShellData->m_bClosed = false;
		TopoShellData->m_ppFaces = TSFaces.GetData();
		TopoShellData->m_uiFaceSize = TSFaces.Num();
		TopoShellData->m_pucOrientationWithShell = FaceOrientations.GetData();

		TopoShellPtr = CADLibrary::TechSoftInterface::CreateTopoShell(*TopoShellData);

		if (TopoShellPtr == nullptr)
		{
			return false;
		}
	}

	A3DRiRepresentationItem* RiRepresentationItem = CADLibrary::TechSoftUtils::CreateRIBRep(TopoShellPtr);

	if (RiRepresentationItem != nullptr)
	{
		RiRepresentationItems.Add(RiRepresentationItem);
		return true;
	}
#endif
	return false;
}

}
#endif
