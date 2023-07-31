// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "CADModelToTechSoftConverterBase.h"

#include "AliasBRepConverter.h"

#include "CADKernel/Core/Session.h"
#include "CADKernel/Core/Types.h"

class AlDagNode;
class AlShell;
class AlSurface;
class AlTrimBoundary;
class AlTrimCurve;
class AlTrimRegion;

typedef void A3DTopoCoEdge;
typedef void A3DTopoFace;
typedef void A3DTopoLoop;
typedef void A3DTopoCoEdge;
typedef void A3DCrvBase;

typedef double AlMatrix4x4[4][4];

namespace UE_DATASMITHWIRETRANSLATOR_NAMESPACE
{

class FAliasModelToTechSoftConverter : public FCADModelToTechSoftConverterBase, public IAliasBRepConverter
{

public:
	FAliasModelToTechSoftConverter(CADLibrary::FImportParameters InImportParameters)
		: FCADModelToTechSoftConverterBase(InImportParameters)
	{
	}

	virtual bool AddBRep(AlDagNode& DagNode, const FColor& Color, EAliasObjectReference ObjectReference) override;

protected:
	TMap<void*, A3DTopoCoEdge*> AlEdgeToTSCoEdge;

	A3DTopoFace* AddTrimRegion(const AlTrimRegion& InTrimRegion, const FColor& Color, EAliasObjectReference InObjectReference, const AlMatrix4x4& InAlMatrix);
	A3DTopoLoop* CreateTopoLoop(const AlTrimBoundary& TrimBoundary);
	A3DTopoCoEdge* CreateEdge(const AlTrimCurve& TrimCurve);
	A3DCrvBase* CreateCurve(const AlTrimCurve& TrimCurve);

	void LinkEdgesLoop(const AlTrimBoundary& TrimBoundary);
};

}

