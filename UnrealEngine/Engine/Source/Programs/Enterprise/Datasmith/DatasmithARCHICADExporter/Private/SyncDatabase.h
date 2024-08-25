// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utils/AddonTools.h"
#include "Utils/LibPartInfo.h"

#include "Lock.hpp"
#include "ModelElement.hpp"

#include "Map.h"
#include "List.h"

BEGIN_NAMESPACE_UE_AC

class FSyncContext;
class FSyncData;
class FMaterialsDatabase;
class FTexturesCache;
class FElementID;
class FLibPartInfo;
class FMeshClass;
class FSyncDatabase;

class FMeshDimensions
{
  public:
	FMeshDimensions()
		: Area(0.0f)
		, Width(0.0f)
		, Height(0.0f)
		, Depth(0.0f)
	{
	}

	FMeshDimensions(const IDatasmithMeshElement& InMesh)
		: Area(InMesh.GetArea())
		, Width(InMesh.GetWidth())
		, Height(InMesh.GetHeight())
		, Depth(InMesh.GetDepth())
	{
	}

	float Area;
	float Width;
	float Height;
	float Depth;
};

class FMeshClass
{
  public:
	// ARCHICAD 3D Element hash value
	GS::ULong Hash = 0;

	// Datasmith mesh element created for this class
	TSharedPtr< IDatasmithMeshElement > MeshElement;

	// Number of actor elements that are implementations of this class.
	GS::UInt32 InstancesCount = 1;

	// Next fields are for invariants tests

	// Type of first ARCHICAD 3D Element collected for this hash
	ModelerAPI::Element::Type ElementType;

	// Number of elements that define transformation.
	GS::UInt32 TransformCount = 0;

	// Return the mesh element created for first converted element.
	const TSharedPtr< IDatasmithMeshElement >& GetMeshElement() const { return MeshElement; }

	bool bMeshElementInitialized = false;

	// Define the mesh element for this hash value
	void SetMeshElement(const TSharedPtr< IDatasmithMeshElement >& InMeshElement);

	// Add an instance, waiting for the resulting mesh
	enum EBuildMesh
	{
		kDontBuild,
		kBuild
	};
	EBuildMesh AddInstance(FSyncData* InInstance, FSyncDatabase* IOSyncDatabase);

	// Pop an instance waiting for the resulting mesh
	FSyncData* PopInstance();

	// Set Mesh Element to the sync data
	void SetInstanceMesh(FSyncData* InInstance, FSyncDatabase* IOSyncDatabase) const;

	// Set Mesh Element to the sync data
	void SetWaitingInstanceMesh(FSyncDatabase* IOSyncDatabase);

	FVector Translation;
	FQuat Rotation;

  private:
	typedef TList< FSyncData* > FSyncDataList;
	FSyncDataList*				HeadInstances;

	// Control access on this object (for queue operations)
	static GS::Lock AccessControl;

	// Condition variable
	static GS::Condition CV;
};

class FMeshCacheIndexor
{
  public:
	// Constructor
	FMeshCacheIndexor(const TCHAR* InIndexFilePath);

	// Destructor
	~FMeshCacheIndexor();

	// Return the mesh dimension if mesh is already created.
	const FMeshDimensions* FindMesh(const TCHAR* InMeshName) const;

	// Add this mesh to the indexor
	void AddMesh(const IDatasmithMeshElement& InMesh);

	// If changed, save the indexor to the specified file.
	void SaveToFile();

	// Read the indexor from the specified file
	void ReadFromFile();

  private:
	// We index with mesh name (aka. Hash of the mesh), and we save it's dimensions
	typedef TMap< FString, TUniquePtr< FMeshDimensions > > MapName2Dimensions;

	FString IndexFilePath;

	// Indexor
	MapName2Dimensions Name2Dimensions;

	// Flag to know if we must save
	bool bChanged = false;

	// Control access on this object
	mutable GS::Lock AccessControl;

	// Condition variable
	GS::Condition AccessCondition;
};

// Class that maintain synchronization datas (SyncData, Material, Texture)
class FSyncDatabase
{
  public:
	// Constructor
	FSyncDatabase(const TCHAR* InSceneName, const TCHAR* InSceneLabel, const TCHAR* InAssetsPath,
				  const GS::UniString& InAssetsCache);

	// Destructor
	~FSyncDatabase();

	// SetSceneInfo
	void SetSceneInfo();

	// Synchronize
	void Synchronize(const FSyncContext& InSyncContext);

	// Get scene
	const TSharedRef< IDatasmithScene >& GetScene() const { return Scene; }

	// Return the asset file path
	const TCHAR* GetAssetsFolderPath() const;

	// Get access to material database
	FMaterialsDatabase& GetMaterialsDatabase() const { return *MaterialsDatabase; }

	// Get access to textures cache
	FTexturesCache& GetTexturesCache() const { return *TexturesCache; }

	// Before a scan we reset our sync data, so we can detect when an element has been modified or destroyed
	void ResetBeforeScan();

	// After a scan, but before syncing, we delete obsolete syncdata (and it's Datasmith Element)
	void CleanAfterScan();

	// Get existing sync data for the specified guid
	// If null, you must create it.
	FSyncData*& GetSyncData(const GS::Guid& InGuid);

	// Return scene sync data (Create it if not present)
	FSyncData& GetSceneSyncData();

	// Return layer sync data (Create it if not present)
	FSyncData& GetLayerSyncData(short InLayer);
#if AC_VERSION > 26
	FSyncData& GetLayerSyncData(const API_AttributeIndex& InLayer)
	{
		return GetLayerSyncData(short(InLayer.ToInt32_Deprecated()));
	}
#endif

	// Delete obsolete syncdata (and it's Datasmith Element)
	void DeleteSyncData(const GS::Guid& InGuid);

	// Return the name of the specified layer
	const FString& GetLayerName(short InLayerIndex);

	// Set the mesh in the handle and take care of mesh life cycle.
	bool SetMesh(TSharedPtr< IDatasmithMeshElement >* Handle, const TSharedPtr< IDatasmithMeshElement >& InMesh);

	// Return the libpart from it's index
	FLibPartInfo* GetLibPartInfo(GS::Int32 InIndex);

	// Return the libpart from it's unique id
	FLibPartInfo* GetLibPartInfo(const char* InUnID);

	FMeshClass* GetMeshClass(GS::ULong InHash) const;

	void AddInstance(GS::ULong InHash, TUniquePtr< FMeshClass >&& InInstance);

	// Return the cache path
	static GS::UniString GetCachePath();

#if PLATFORM_MAC
	// Change the cache path
	static void SetCachePath(GS::UniString& InCacheDirectory);
#endif

	FMeshCacheIndexor& GetMeshIndexor() { return MeshIndexor; }

  private:
	typedef TMap< FGuid, FSyncData* >							 FMapGuid2SyncData;
	typedef TMap< short, FString >								 FMapLayerIndex2Name;
	typedef TMap< GS::Int32, TUniquePtr< FLibPartInfo > >		 MapIndex2LibPart;
	typedef TMap< FGSUnID, FLibPartInfo* >						 MapUnId2LibPart;
	typedef GS::HashTable< GS::ULong, TUniquePtr< FMeshClass > > HashTableMeshClasses;

	// To take care of mesh life cycle.
	class FMeshInfo
	{
	  public:
		TSharedPtr< IDatasmithMeshElement > Mesh; // The mesh
		uint32								Count = 0; // Number of actors using this mesh

		FMeshInfo() {}
	};
	// Map mesh by their hash name.
	typedef TMap< FString, FMeshInfo > FMapHashToMeshInfo;

	void ResetMeshClasses();

	void CleanMeshClasses(const FSyncContext& InSyncContext);

	void ReportMeshClasses() const;

	// Scan all elements, to determine if they need to be synchronized
	UInt32 ScanElements(const FSyncContext& InSyncContext);

	// Scan all lights
	void ScanLights(FElementID& InElementID);

	// Scan all cameras
	void ScanCameras(const FSyncContext& InSyncContext);

	// The scene
	TSharedRef< IDatasmithScene > Scene;

	// Path where to save assets
	FString AssetsFolderPath;

	// Fast access to material
	FMaterialsDatabase* MaterialsDatabase;

	// Cache to have fast access to textures
	FTexturesCache* TexturesCache;

	// Map guid to sync data
	FMapGuid2SyncData ElementsSyncDataMap;

	// Map layer index to it's name
	FMapLayerIndex2Name LayerIndex2Name;

	// Map mesh name (hash) to mesh info
	FMapHashToMeshInfo HashToMeshInfo;
	GS::Lock		   HashToMeshInfoAccesControl;

	// Map lib part by index
	MapIndex2LibPart IndexToLibPart;

	// Map lib part by UnId
	MapUnId2LibPart UnIdToLibPart;

	HashTableMeshClasses MeshClasses;

	FMeshCacheIndexor MeshIndexor;
};

END_NAMESPACE_UE_AC
