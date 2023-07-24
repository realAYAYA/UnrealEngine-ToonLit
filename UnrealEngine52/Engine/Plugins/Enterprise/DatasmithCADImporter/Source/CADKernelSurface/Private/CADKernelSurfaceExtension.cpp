// Copyright Epic Games, Inc. All Rights Reserved.

#include "CADKernelSurfaceExtension.h"

#include "CADKernelTools.h"
#include "CADData.h"
#include "CADOptions.h"
#include "DatasmithPayload.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "MeshDescriptionHelper.h"
#include "Misc/FileHelper.h"
#include "StaticMeshAttributes.h"
#include "UObject/EnterpriseObjectVersion.h"

#include "CADKernel/Core/CADKernelArchive.h"
#include "CADKernel/Core/Entity.h"
#include "CADKernel/Core/Session.h"
#include "CADKernel/Core/Types.h"
#include "CADKernel/Mesh/Meshers/ParametricMesher.h"
#include "CADKernel/Mesh/Structure/ModelMesh.h"
#include "CADKernel/Topo/Model.h"
#include "CADKernel/Topo/Body.h"

void UCADKernelParametricSurfaceData::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FEnterpriseObjectVersion::GUID);
	Super::Serialize(Ar);
	Ar << RawData;
}

bool UCADKernelParametricSurfaceData::Tessellate(UStaticMesh& StaticMesh, const FDatasmithRetessellationOptions& RetessellateOptions)
{
	using namespace UE::CADKernel;

	bool bSuccessfulTessellation = false;

#if WITH_EDITOR
	CADLibrary::FImportParameters ImportParameters;
	ImportParameters.SetModelCoordinateSystem((FDatasmithUtils::EModelCoordSystem) SceneParameters.ModelCoordSys);
	ImportParameters.SetTesselationParameters(RetessellateOptions.ChordTolerance, RetessellateOptions.MaxEdgeLength, RetessellateOptions.NormalTolerance, (CADLibrary::EStitchingTechnique) RetessellateOptions.StitchingTechnique);

	CADLibrary::FMeshParameters CadMeshParameters;
	CadMeshParameters.bNeedSwapOrientation = MeshParameters.bNeedSwapOrientation;
	CadMeshParameters.bIsSymmetric = MeshParameters.bIsSymmetric;
	CadMeshParameters.SymmetricNormal = (FVector3f) MeshParameters.SymmetricNormal;
	CadMeshParameters.SymmetricOrigin = (FVector3f) MeshParameters.SymmetricOrigin;

	CADLibrary::FMeshConversionContext MeshConversionContext(ImportParameters, CadMeshParameters);

	// Previous MeshDescription is get to be able to create a new one with the same order of PolygonGroup (the matching of color and partition is currently based on their order)
	if (FMeshDescription* DestinationMeshDescription = StaticMesh.GetMeshDescription(0))
	{
		FMeshDescription MeshDescription;
		FStaticMeshAttributes MeshDescriptionAttributes(MeshDescription);
		MeshDescriptionAttributes.Register();

		if (RetessellateOptions.RetessellationRule == EDatasmithCADRetessellationRule::SkipDeletedSurfaces)
		{
			CADLibrary::GetExistingPatches(*DestinationMeshDescription, MeshConversionContext.PatchesToMesh);
		}

		const double GeometricTolerance = 0.01; // mm
		TSharedRef<FSession> CADKernelSession = MakeShared<FSession>(GeometricTolerance);
		CADKernelSession->AddDatabase(RawData);

		FModel& CADKernelModel = CADKernelSession->GetModel();
		TArray<TSharedPtr<FBody>> CADKernelBodies = CADKernelModel.GetBodies();
		if (CADKernelBodies.Num() != 1)
		{
			return bSuccessfulTessellation;
		}

		if(CADLibrary::FCADKernelTools::Tessellate(CADKernelModel, MeshConversionContext, MeshDescription))
		{
			// To update the SectionInfoMap 
			TPolygonGroupAttributesConstRef<FName> MaterialSlotNames = MeshDescriptionAttributes.GetPolygonGroupMaterialSlotNames();
			FMeshSectionInfoMap& SectionInfoMap = StaticMesh.GetSectionInfoMap();

			for (FPolygonGroupID PolygonGroupID : MeshDescription.PolygonGroups().GetElementIDs())
			{
				FMeshSectionInfo Section = SectionInfoMap.Get(0, PolygonGroupID.GetValue());
				int32 MaterialIndex = StaticMesh.GetMaterialIndex(MaterialSlotNames[PolygonGroupID]);
				if (MaterialIndex < 0)
				{
					MaterialIndex = 0;
				}
				Section.MaterialIndex = MaterialIndex;
				SectionInfoMap.Set(0, PolygonGroupID.GetValue(), Section);
			}
			*DestinationMeshDescription = MoveTemp(MeshDescription);
			bSuccessfulTessellation = true;
		}
	}
#endif 

	return bSuccessfulTessellation;
}

namespace CADKernelSurface
{
	void AddSurfaceDataForMesh(const TCHAR* CADKernelArchive, const CADLibrary::FImportParameters& InSceneParameters, const CADLibrary::FMeshParameters& InMeshParameters, const FDatasmithTessellationOptions& InTessellationOptions, FDatasmithMeshElementPayload& OutMeshPayload)
	{
		// Store CADKernel archive if provided
		UCADKernelParametricSurfaceData* CADKernelData = Datasmith::MakeAdditionalData<UCADKernelParametricSurfaceData>();
		if (CADKernelData->SetFile(CADKernelArchive))
		{
			CADKernelData->SetImportParameters(InSceneParameters);
			CADKernelData->SetMeshParameters(InMeshParameters);
			CADKernelData->SetLastTessellationOptions(InTessellationOptions);

			OutMeshPayload.AdditionalData.Add(CADKernelData);
		}
	}

}
