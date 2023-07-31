// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "AliasBRepConverter.h"
#include "CADModelToCADKernelConverterBase.h"

#include "CADKernel/Core/Session.h"
#include "CADKernel/Core/Types.h"

class AlDagNode;
class AlShell;
class AlSurface;
class AlTrimBoundary;
class AlTrimCurve;
class AlTrimRegion;

typedef double AlMatrix4x4[4][4];

namespace UE::CADKernel
{
	class FShell;
	class FSurface;
	class FTopologicalEdge;
	class FTopologicalFace;
	class FTopologicalLoop;
}

namespace UE_DATASMITHWIRETRANSLATOR_NAMESPACE
{

class FAliasModelToCADKernelConverter : public FCADModelToCADKernelConverterBase, public IAliasBRepConverter
{

public:
	FAliasModelToCADKernelConverter(CADLibrary::FImportParameters InImportParameters)
		: FCADModelToCADKernelConverterBase(InImportParameters)
	{
	}

	virtual bool AddBRep(AlDagNode& DagNode, const FColor& Color, EAliasObjectReference ObjectReference) override;

protected:
	TSharedPtr<UE::CADKernel::FTopologicalEdge> AddEdge(const AlTrimCurve& TrimCurve, TSharedPtr<UE::CADKernel::FSurface>& CarrierSurface);

	TSharedPtr<UE::CADKernel::FTopologicalLoop> AddLoop(const AlTrimBoundary& TrimBoundary, TSharedPtr<UE::CADKernel::FSurface>& CarrierSurface, const bool bIsExternal);
	
	/**
	 * Build face's links with its neighbor have to be done after the loop is finalize.
	 * This is to avoid to link an edge with another and then to delete it...
	 */
	void LinkEdgesLoop(const AlTrimBoundary& TrimBoundary, UE::CADKernel::FTopologicalLoop& Loop);

	TSharedPtr<UE::CADKernel::FTopologicalFace> AddTrimRegion(const AlTrimRegion& InTrimRegion, EAliasObjectReference InObjectReference, const AlMatrix4x4& InAlMatrix, bool bInOrientation);
	void AddFace(const AlSurface& InSurface, EAliasObjectReference InObjectReference, const AlMatrix4x4& InAlMatrix, bool bInOrientation, TSharedRef<UE::CADKernel::FShell>& Shell);
	void AddShell(const AlShell& InShell, EAliasObjectReference InObjectReference, const AlMatrix4x4& InAlMatrix, bool bInOrientation, TSharedRef<UE::CADKernel::FShell>& Shell);

protected:
	int32 LastFaceId = 1;
	TMap<void*, TSharedPtr<UE::CADKernel::FTopologicalEdge>>  AlEdge2CADKernelEdge;
};

}