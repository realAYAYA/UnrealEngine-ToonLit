// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utils/AddonTools.h"

#include "Map.h"

class IDatasmithBaseMaterialElement;

BEGIN_NAMESPACE_UE_AC

class FSyncContext;

enum ESided : bool
{
	kSingleSide = false,
	kDoubleSide = true
};

enum : int32
{
	kInvalidMaterialIndex = 0
};

class FMaterialKey
{
  public:
	FMaterialKey(GS::Int32 InACMaterialIndex, GS::Int32 InACTextureIndex, ESided InSided)
		: ACMaterialIndex(InACMaterialIndex)
		, ACTextureIndex(InACTextureIndex)
		, Sided(InSided)
	{
	}

	GS::Int32 ACMaterialIndex;
	GS::Int32 ACTextureIndex;
	ESided	  Sided;

	// Equality operator needed for use as FMap key
	bool operator==(const FMaterialKey& InOther) const
	{
		return ACMaterialIndex == InOther.ACMaterialIndex && ACTextureIndex == InOther.ACTextureIndex &&
			   Sided == InOther.Sided;
	}
};

inline uint32 GetTypeHash(const FMaterialKey& A)
{
	return FCrc::TypeCrc32(A.ACMaterialIndex, FCrc::TypeCrc32(A.ACTextureIndex, FCrc::TypeCrc32(A.Sided, 0)));
}

// Materials database
class FMaterialsDatabase
{
  public:
	// Class to permit sync of material
	class FMaterialSyncData
	{
	  public:
		// Constructor
		FMaterialSyncData() {}

		// Return the Datasmith Id (Name) {Material GUID + Texture GUID + "_S"}
		const FString& GetDatasmithId() const { return DatasmithId; }

		// Return the Datasmith Label (Displayable name) {Material name + Texture name + "_S"}
		const FString& GetDatasmithLabel() const { return DatasmithLabel; }

		TSharedPtr< IDatasmithUEPbrMaterialElement > Element;

		bool			   bIsInitialized = false;
		UInt64			   LastModificationStamp = 0;
		GS::Guid		   MaterialId; // Guid (real or simulated)
		FString			   DatasmithId;
		FString			   DatasmithLabel;
		bool			   bHasTexture = false;
		bool			   bIsDuplicate = false;
		bool			   bIdIsSynthetized = false;
		API_AttributeIndex MaterialIndex = kInvalidMaterialIndex;

		void Init(const FSyncContext& SyncContext, const FMaterialKey& MaterialKey);

		bool CheckModify(const FMaterialKey& MaterialKey);

		void Update(const FSyncContext& SyncContext, const FMaterialKey& MaterialKey);
	};

	// Constructor
	FMaterialsDatabase();

	// Destructor
	~FMaterialsDatabase();

	// Return true if at least one material have been modified
	bool CheckModify();

	// Scan all material and update modified ones
	void UpdateModified(const FSyncContext& SyncContext);

	// Reset
	void Clear();

	const FMaterialSyncData& GetMaterial(const FSyncContext& SyncContext, GS::Int32 inACMaterialIndex,
										 GS::Int32 inACTextureIndex, ESided InSided);

  private:
	typedef TMap< FMaterialKey, TUniquePtr< FMaterialSyncData > > MapSyncData; // Map Material key to Material sync data

	typedef TSet< FString > SetMaterialsNames;

	MapSyncData		  MapMaterials;
	SetMaterialsNames MaterialsNamesSet;
};

END_NAMESPACE_UE_AC
