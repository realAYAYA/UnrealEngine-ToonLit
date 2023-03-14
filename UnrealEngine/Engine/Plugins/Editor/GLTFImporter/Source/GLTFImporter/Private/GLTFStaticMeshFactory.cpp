// Copyright Epic Games, Inc. All Rights Reserved.

#include "GLTFStaticMeshFactory.h"

#include "GLTFAsset.h"
#include "GLTFMeshFactory.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshAttributes.h"
#include "PackageTools.h"

namespace GLTF
{
	class FStaticMeshFactoryImpl : public GLTF::FBaseLogger
	{
	public:
		FStaticMeshFactoryImpl();

		const TArray<UStaticMesh*>& CreateMeshes(const GLTF::FAsset& Asset, UObject* ParentPackage, EObjectFlags Flags, bool bApplyPostEditChange);
		void FillMeshDescription(const GLTF::FMesh &Mesh, FMeshDescription* MeshDescription) { MeshFactory.FillMeshDescription(Mesh, MeshDescription); }

		void CleanUp();

	private:
		UStaticMesh* CreateMesh(const GLTF::FMesh& Mesh, UObject* ParentPackage, EObjectFlags Flags);

		void SetupMeshBuildSettings(int32 NumUVs, bool bMeshHasTagents, bool bMeshHasUVs, UStaticMesh& StaticMesh,
		                            FStaticMeshSourceModel& SourceModel);

		float GetUniformScale() const { return MeshFactory.GetUniformScale(); }
		void  SetUniformScale(float Scale) { MeshFactory.SetUniformScale(Scale); }

		void SetReserveSize(uint32 Size) { MeshFactory.SetReserveSize(Size); }

	private:
		bool                 bGenerateLightmapUVs;
		TArray<UStaticMesh*> StaticMeshes;

		FMeshFactory MeshFactory;
		friend class FStaticMeshFactory;
	};

	namespace
	{
		int32 GetNumUVs(const GLTF::FMesh& Mesh)
		{
			int32 NumUVs = 0;
			for (int32 UVIndex = 0; UVIndex < MAX_MESH_TEXTURE_COORDS_MD; ++UVIndex)
			{
				if (Mesh.HasTexCoords(UVIndex))
				{
					NumUVs++;
				}
				else
				{
					break;
				}
			}
			return NumUVs;
		}
	}

	FStaticMeshFactoryImpl::FStaticMeshFactoryImpl()
	    : bGenerateLightmapUVs(false) 
	{}

	UStaticMesh* FStaticMeshFactoryImpl::CreateMesh(const FMesh& Mesh, UObject* ParentPackage, EObjectFlags Flags)
	{
		check(!Mesh.Name.IsEmpty());

		const FString PackageName  = UPackageTools::SanitizePackageName(FPaths::Combine(ParentPackage->GetName(), Mesh.Name));
		UPackage*     AssetPackage = CreatePackage(*PackageName);
		UStaticMesh*  StaticMesh   = NewObject<UStaticMesh>(AssetPackage, *FPaths::GetBaseFilename(PackageName), Flags);

		FStaticMeshSourceModel& SourceModel = StaticMesh->AddSourceModel();

		// GLTF currently only supports LODs via MSFT_lod, for now use always 0
		const int32 LODIndex = 0;
		FMeshDescription* MeshDescription = StaticMesh->CreateMeshDescription(LODIndex);

		MeshFactory.FillMeshDescription(Mesh, MeshDescription);		
		Messages.Append(MeshFactory.GetLogMessages());

		if (Mesh.HasJointWeights())
		{
			Messages.Emplace(EMessageSeverity::Warning, TEXT("Mesh has joint weights which are not supported: ") + Mesh.Name);
		}

		const bool bRecomputeTangents = !Mesh.HasTangents();
		if (bRecomputeTangents)
		{
			FMeshBuildSettings& Settings = SourceModel.BuildSettings;
			Settings.bRecomputeTangents  = true;
		}

		for (int32 Index = 0; Index < Mesh.Primitives.Num(); ++Index)
		{
			const FPrimitive& Primitive = Mesh.Primitives[Index];
			const FName SlotName(*FString::FromInt(Primitive.MaterialIndex));
			const int32 MeshSlot = StaticMesh->GetStaticMaterials().Emplace(nullptr, SlotName, SlotName);
			StaticMesh->GetSectionInfoMap().Set(0, MeshSlot, FMeshSectionInfo(MeshSlot));
		}


		const int32 NumUVs = FMath::Max(1, GetNumUVs(Mesh)); // Duplicated from FillMeshDescription
		SetupMeshBuildSettings(NumUVs, Mesh.HasTangents(), Mesh.HasTexCoords(0), *StaticMesh, SourceModel);
		StaticMesh->CommitMeshDescription(LODIndex);

		return StaticMesh;
	}

	void FStaticMeshFactoryImpl::SetupMeshBuildSettings(int32 NumUVs, bool bMeshHasTagents, bool bMeshHasUVs, UStaticMesh& StaticMesh,
	                                                    FStaticMeshSourceModel& SourceModel)
	{
		FMeshBuildSettings& Settings = SourceModel.BuildSettings;
		if (bGenerateLightmapUVs)
		{
			// Generate a new UV set based off the highest index UV set in the mesh
			StaticMesh.SetLightMapCoordinateIndex(NumUVs);
			SourceModel.BuildSettings.SrcLightmapIndex     = NumUVs - 1;
			SourceModel.BuildSettings.DstLightmapIndex     = NumUVs;
			SourceModel.BuildSettings.bGenerateLightmapUVs = true;
		}
		else if (!bMeshHasUVs)
		{
			// Generate automatically a UV for correct lighting if mesh has none
			StaticMesh.SetLightMapCoordinateIndex(1);
			SourceModel.BuildSettings.SrcLightmapIndex     = 0;
			SourceModel.BuildSettings.DstLightmapIndex     = 1;
			SourceModel.BuildSettings.bGenerateLightmapUVs = true;
		}
		else
		{
			StaticMesh.SetLightMapCoordinateIndex(NumUVs - 1);
			SourceModel.BuildSettings.bGenerateLightmapUVs = false;
		}

		Settings.bRecomputeNormals  = false;
		Settings.bRecomputeTangents = !bMeshHasTagents;
		Settings.bUseMikkTSpace     = true;  // glTF spec defines that MikkTSpace algorithms should be used when tangents aren't defined
		Settings.bComputeWeightedNormals = true;

		Settings.bRemoveDegenerates        = false;
		Settings.bBuildReversedIndexBuffer = false;

		Settings.bUseHighPrecisionTangentBasis = false;
		Settings.bUseFullPrecisionUVs          = false;
	}

	inline const TArray<UStaticMesh*>& FStaticMeshFactoryImpl::CreateMeshes(const GLTF::FAsset& Asset, UObject* ParentPackage, EObjectFlags Flags,
	                                                                        bool bApplyPostEditChange)
	{
		const uint32 MeshCount = Asset.Meshes.Num();
		StaticMeshes.Empty();
		StaticMeshes.Reserve(MeshCount);

		Messages.Empty();
		for (const GLTF::FMesh& Mesh : Asset.Meshes)
		{
			UStaticMesh* StaticMesh = CreateMesh(Mesh, ParentPackage, Flags);
			if (!StaticMesh)
			{
				continue;
			}

			StaticMeshes.Add(StaticMesh);
			if (bApplyPostEditChange)
			{
				StaticMesh->MarkPackageDirty();
				StaticMesh->PostEditChange();
				FAssetRegistryModule::AssetCreated(StaticMesh);
			}
		}

		return StaticMeshes;
	}

	inline void FStaticMeshFactoryImpl::CleanUp()
	{
		StaticMeshes.Empty();

		MeshFactory.CleanUp();
	}

	//

	FStaticMeshFactory::FStaticMeshFactory()
	    : Impl(new FStaticMeshFactoryImpl())
	{
	}

	FStaticMeshFactory::~FStaticMeshFactory() {}

	const TArray<UStaticMesh*>& FStaticMeshFactory::CreateMeshes(const GLTF::FAsset& Asset, UObject* ParentPackage, EObjectFlags Flags,
	                                                             bool bApplyPostEditChange)
	{
		return Impl->CreateMeshes(Asset, ParentPackage, Flags, bApplyPostEditChange);
	}

	void FStaticMeshFactory::FillMeshDescription(const GLTF::FMesh &Mesh, FMeshDescription* MeshDescription)
	{
		Impl->FillMeshDescription(Mesh, MeshDescription);
	}


	const TArray<UStaticMesh*>& FStaticMeshFactory::GetMeshes() const
	{
		return Impl->StaticMeshes;
	}

	const TArray<FLogMessage>& FStaticMeshFactory::GetLogMessages() const
	{
		return Impl->GetLogMessages();
	}

	float FStaticMeshFactory::GetUniformScale() const
	{
		return Impl->GetUniformScale();
	}

	void FStaticMeshFactory::SetUniformScale(float Scale)
	{
		Impl->SetUniformScale(Scale);
	}

	bool FStaticMeshFactory::GetGenerateLightmapUVs() const
	{
		return Impl->bGenerateLightmapUVs;
	}

	void FStaticMeshFactory::SetGenerateLightmapUVs(bool bInGenerateLightmapUVs)
	{
		Impl->bGenerateLightmapUVs = bInGenerateLightmapUVs;
	}

	void FStaticMeshFactory::SetReserveSize(uint32 Size)
	{
		Impl->SetReserveSize(Size);
	}

	void FStaticMeshFactory::CleanUp()
	{
		Impl->CleanUp();
	}

}  //  namespace GLTF
