// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/IO/PCGExternalDataContext.h"

#if WITH_EDITOR
#include "AbcImportSettings.h"
#include "AbcUtilities.h"

THIRD_PARTY_INCLUDES_START
#pragma warning(push)
#pragma warning(disable:4005) // TEXT macro redefinition
#include <Alembic/AbcCoreOgawa/All.h>
#include "Alembic/AbcGeom/All.h"
#include "Alembic/AbcCoreFactory/IFactory.h"
#pragma warning(pop)
THIRD_PARTY_INCLUDES_END

#endif // WITH_EDITOR

struct FPCGLoadAlembicContext : public FPCGExternalDataContext
{
#if WITH_EDITOR
	// Used for alembic parsing
	/** Factory used to generate objects*/
	Alembic::AbcCoreFactory::IFactory Factory;
	/** Archive-typed ABC file */
	Alembic::Abc::IArchive Archive;
	/** Alembic typed root (top) object*/
	Alembic::Abc::IObject TopObject;
#endif // WITH_EDITOR
};

namespace PCGAlembicInterop
{
#if WITH_EDITOR
	void LoadFromAlembicFile(FPCGLoadAlembicContext* Context, const FString& FileName);
#endif
}