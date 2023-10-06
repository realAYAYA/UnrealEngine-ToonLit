// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#ifdef CADKERNEL_DEV
#include "CADKernel/Core/Parameter.h"
#include "CADKernel/Core/Parameters.h"
#endif

#include "CADKernel/Core/Types.h"
#include "CADKernel/Math/Point.h"

namespace UE::CADKernel
{
enum EVisuProperty
{
	BBox = 0,          // Green see CADKernelUI::properties    
	Point,             // Blue
	Element,           // Brown
	Curve,             // Blue
	Iso,               // Red
	ControlLine,       // Purple
	EdgeMesh,          // blue
	NodeMesh,          // blue
	BorderEdge,        // Green
	NonManifoldEdge,   // Red          
	BorderTopo,        // Dark brown
	ControlPoint,      // Purple		 
	YellowPoint,
	YellowCurve,
	BluePoint,
	BlueCurve,
	RedPoint,
	RedCurve,
	PurplePoint,
	PurpleCurve,
	GreenPoint,
	GreenCurve,
	PinkPoint,
	PinkCurve,
	OrangePoint,
	OrangeCurve,
	Last,
};

extern const TCHAR* VisuPropertyNames[];

class FMesh;

#ifdef CADKERNEL_DEV

class CADKERNEL_API FVisuParameters : public FParameters
{
public:

	FParameter ChordError;

	/**
	 *	Number of isos along U to display on a surface or topological face
	 */
	FParameter IsoUNumber;

	/**
	 *	Number of isos along V to display on a surface or topological face
	 */
	FParameter IsoVNumber;

	FParameter bDisplayCADOrient;
	FParameter bDisplayMeshOrient;
	FParameter bDisplayAxis;
	FParameter bDisplayNormals;

	FParameter NormalLength;

	FVisuParameters()
		: FParameters(10)
		, ChordError(TEXT("DiscretizationError"), 0.02, *this)
		, IsoUNumber(TEXT("IsoUNumber"), 3, *this)
		, IsoVNumber(TEXT("IsoVNumber"), 3, *this)
		, bDisplayCADOrient(TEXT("DisplayCadOrient"), false, *this)
		, bDisplayMeshOrient(TEXT("DisplayMeshOrient"), true, *this)
		, bDisplayAxis(TEXT("DisplayAxis"), false, *this)
		, bDisplayNormals(TEXT("DisplayNormals"), false, *this)
		, NormalLength(TEXT("NormalLength"), 10., *this)
	{}
};
#endif

class CADKERNEL_API FVisu
{
protected:
#ifdef CADKERNEL_DEV
	FVisuParameters Parameters;
#endif

	/**
	 * Number of graphic session opened
	 */
	int32 SessionNum;

	void NewSession()
	{
		ensureCADKernel(SessionNum >= 0);
		++SessionNum;
	}

	void EndSession()
	{
		--SessionNum;
		if (SessionNum < 0)
		{
			SessionNum = 0;
		}
	}

public:

	FVisu()
	{
		SessionNum = 0;
	}

	virtual ~FVisu() = default;

	int32 GetPropertyCount()
	{
		return EVisuProperty::Last;
	}

#ifdef CADKERNEL_DEV
	FVisuParameters* GetParameters()
	{
		return &Parameters;
	}
#endif

	virtual void NewDB(const TCHAR* InName)
	{}

	virtual void Open3DDebugSession(const TCHAR* SessionName, const TArray<FIdent>& Ids)
	{}

	virtual void Close3DDebugSession()
	{}

	virtual void Open3DDebugSegment(FIdent Ident)
	{}

	virtual void Close3DDebugSegment()
	{}

	virtual void UpdateViewer()
	{}

	virtual void DrawPoint(const FPoint& Point, EVisuProperty InProperty = EVisuProperty::BluePoint)
	{}

	virtual void DrawPoint(const FPoint2D& Point, EVisuProperty InProperty = EVisuProperty::BluePoint)
	{}

	virtual void DrawPoint(const FPointH& Point, EVisuProperty InProperty = EVisuProperty::BluePoint)
	{}

	virtual void DrawElement(int32 Dimension, TArray<FPoint>& Points, EVisuProperty InProperty = EVisuProperty::Element)
	{}

	virtual void DrawPolyline(const TArray<FPoint>& Points, EVisuProperty InProperty = EVisuProperty::BlueCurve)
	{}

	virtual void DrawPolyline(const TArray<FPoint2D>& Points, EVisuProperty InProperty = EVisuProperty::BlueCurve)
	{}

	virtual void DrawMesh(FIdent MeshId)
	{}
};

} // namespace UE::CADKernel

