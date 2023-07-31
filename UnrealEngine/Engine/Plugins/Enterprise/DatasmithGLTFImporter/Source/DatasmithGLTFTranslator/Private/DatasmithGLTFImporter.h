// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Logging/LogMacros.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "MeshDescription.h"

#include "GLTFLogger.h"

class UStaticMesh;
struct FDatasmithImportContext;
struct FDatasmithImportBaseOptions;
class IDatasmithActorElement;
class IDatasmithMeshActorElement;
class IDatasmithBaseMaterialElement;
class IDatasmithMeshElement;
class IDatasmithVariantElement;
class IDatasmithLevelVariantSetsElement;
class IDatasmithVariantSetElement;
class UDatasmithGLTFImportOptions;
class FDatasmithGLTFAnimationImporter;
class IDatasmithLevelSequenceElement;
class IDatasmithScene;

namespace GLTF
{
	class FFileReader;
	struct FAsset;
	class FMeshFactory;
	class FMaterialFactory;
}

DECLARE_LOG_CATEGORY_EXTERN(LogDatasmithGLTFImport, Log, All);

struct FGLTFImporterStats
{
	int32 MaterialCount;
	int32 MeshCount;
	int32 GeometryCount;
	int32 NodeCount;

	FGLTFImporterStats()
	{
		FMemory::Memzero(*this);
	}
};

class FDatasmithGLTFImporter : FNoncopyable
{
public:
	FDatasmithGLTFImporter(TSharedRef<IDatasmithScene>& OutScene, UDatasmithGLTFImportOptions* InOptions);
	~FDatasmithGLTFImporter();

	/** Updates the used import options to InOptions */
	void SetImportOptions(UDatasmithGLTFImportOptions* InOptions);

	/** Returns any logged messages and clears them afterwards. */
	const TArray<GLTF::FLogMessage>& GetLogMessages() const;

	/** Returns the loaded asset from the last scene file. */
	const GLTF::FAsset& GetAsset() const { return *GLTFAsset; }

	/** Open and load a scene file. */
	bool OpenFile(const FString& InFileName);

	/** Finalize import of the scene into the engine. */
	bool SendSceneToDatasmith();

	void GetGeometriesForMeshElementAndRelease(const TSharedRef<IDatasmithMeshElement> MeshElement, TArray<FMeshDescription>& OutMeshDescriptions);

	const TArray<TSharedRef<IDatasmithLevelSequenceElement>>& GetImportedSequences();

	/** Clean up any unused memory or data. */
	void UnloadScene();

private:
	TSharedPtr<IDatasmithActorElement> CreateCameraActor(int32 CameraIndex) const;
	TSharedPtr<IDatasmithActorElement> CreateLightActor(int32 CameraIndex) const;
	TSharedPtr<IDatasmithMeshActorElement> CreateStaticMeshActor(int32 MeshIndex);
	void CreateMaterialVariants(TSharedPtr<IDatasmithMeshActorElement> MeshActorElement, int32 MeshIndex);

	TSharedPtr<IDatasmithActorElement> ConvertNode(int32 NodeIndex);

	void AddActorElementChild(TSharedPtr<IDatasmithActorElement> ActorElement, const TSharedPtr<IDatasmithActorElement>& ChildNodeActor);

	void SetActorElementTransform(TSharedPtr<IDatasmithActorElement> ActorElement, const FTransform &Transform);

private:
	/** Output Datasmith scene */
	TSharedRef<IDatasmithScene> DatasmithScene;

	mutable TArray<GLTF::FLogMessage>           LogMessages;
	TUniquePtr<GLTF::FFileReader>               GLTFReader;
	TUniquePtr<GLTF::FAsset>                    GLTFAsset;
	TUniquePtr<GLTF::FMaterialFactory>          MaterialFactory;
	TUniquePtr<FDatasmithGLTFAnimationImporter> AnimationImporter;

	bool                               bGenerateLightmapUVs = false;
	float                              ImportScale = 100.f;
	bool                               bAnimationFrameRateFromFile = false;
	TSet<int32>                        ImportedMeshes;

	TMap<IDatasmithMeshElement*, int32> MeshElementToGLTFMeshIndex; // track where Datasmith element originated from to extract data later
	TMap<int32, TSharedRef<IDatasmithMeshElement>> GLTFMeshIndexToMeshElement;
	TMap<FString, TSharedRef<IDatasmithVariantElement>> VariantNameToVariantElement;
	TSharedPtr<IDatasmithLevelVariantSetsElement> VariantSets;
	TSharedPtr<IDatasmithVariantSetElement> VariantSet;

	const bool bTransformIsLocal = true; // GLTF nodes define transform in local space https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#transformations
};