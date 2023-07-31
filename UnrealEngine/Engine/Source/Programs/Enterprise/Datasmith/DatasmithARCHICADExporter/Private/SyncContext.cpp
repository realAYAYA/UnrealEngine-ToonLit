// Copyright Epic Games, Inc. All Rights Reserved.

#include "SyncContext.h"
#include "ElementID.h"

#include "DatasmithSceneExporter.h"

BEGIN_NAMESPACE_UE_AC

void FSyncContext::FStats::ResetAll()
{
	BodiesStats.Reset();
	PolygonsStats.Reset();
	EdgesStats.Reset();
	PolygonsCurved = 0;
	PolygonsComplex = 0;
	PolygonsConvex = 0;
	TotalTrianglePts = 0;
	TotalUVPts = 0;
	TotalTriangles = 0;
	TotalElements = 0;
	TotalElementsWithGeometry = 0;
	TotalElementsModified = 0;
	TotalOwnerCreated = 0;
	TotalActorsCreated = 0;
	TotalEmptyActorsCreated = 0;
	TotalMeshesCreated = 0;
	TotalMeshesReused = 0;
	TotalBugsCount = 0;
	TotalMeshClassesCreated = 0;
	TotalEmptyMeshClassesCreated = 0;
	TotalInstancesCreated = 0;
	TotalEmptyInstancesCreated = 0;
	TotalMeshClassesForgot = 0;
	TotalMeshClassesResactivated = 0;
}

void FSyncContext::FStats::Print()
{
#if UE_AC_DO_STATS
	UE_AC_ReportF("ARCHICAD Elements: Total=%d, With geometry=%d, Modified=%d\n", int(TotalElements),
				  int(TotalElementsWithGeometry), int(TotalElementsModified));
	UE_AC_ReportF("Datasmith Actors : Owner=%d, Created With Mesh=%d, Created Empty %d\n", int(TotalOwnerCreated),
				  int(TotalActorsCreated), int(TotalEmptyActorsCreated));
	UE_AC_ReportF("Datasmith Meshes : Created=%d, Reused=%d\n", int(TotalMeshesCreated), int(TotalMeshesReused));
	UE_AC_ReportF("Mesh Class :	Created=%d, Empty=%d, Forgot=%d, Reactivated=%d, Instances=%d (Empty=%d)\n",
				  int(TotalMeshClassesCreated), int(TotalEmptyMeshClassesCreated), int(TotalMeshClassesForgot),
				  int(TotalMeshClassesResactivated), int(TotalInstancesCreated), int(TotalEmptyInstancesCreated));
	if (TotalBugsCount != 0)
	{
		UE_AC_ReportF("Conversion bug count=%d\n", int(TotalBugsCount));
	}
	#if UE_AC_VERBOSEF_ON
	UE_AC_TraceF("Bodies %s\n", BodiesStats.asStrings().c_str());
	UE_AC_TraceF("Polygons %s\n", PolygonsStats.asStrings().c_str());
	UE_AC_TraceF("\tCurved=%d, Complex=%d --> Convex=%d\n", int(PolygonsCurved), int(PolygonsComplex),
				 int(PolygonsConvex));
	UE_AC_TraceF("\tTotal Triangles { N=%d, Pts=%d }, UVs { Pts=%d }\n", int(TotalTriangles), int(TotalTrianglePts),
				 int(TotalUVPts));
		#if 0
	UE_AC_TraceF("Edges %s\n", EdgesStats.asStrings().c_str());
		#endif
	#endif
#endif
}

FSyncContext::FSyncContext(bool bInIsSynchronizer, const ModelerAPI::Model& InModel, FSyncDatabase& InSyncDatabase,
						   FProgression* InProgression)
	: Model(InModel)
	, Progression(InProgression)
	, SyncDatabase(InSyncDatabase)
	, Stats(*new FStats)
	, bIsSynchronizer(bInIsSynchronizer)
{
}

FSyncContext::~FSyncContext()
{
	delete &Stats;
}

// Start the next phase
void FSyncContext::NewPhase(EPhaseStrId InPhaseId, int InMaxValue) const
{
	if (Progression != nullptr)
	{
		Progression->NewPhase(InPhaseId, InMaxValue);
	}
}

// Advance progression bar to the current value
void FSyncContext::NewCurrentValue(int InCurrentValue) const
{
	if (Progression != nullptr)
	{
		Progression->NewCurrentValue(InCurrentValue);
	}
}

END_NAMESPACE_UE_AC
