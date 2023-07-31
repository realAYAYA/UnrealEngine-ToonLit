// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "DatasmithMaxClassIDs.h"
#include "DatasmithMaxExporterDefines.h"
#include "DatasmithSceneFactory.h"
#include "DatasmithUtils.h"

#include "Templates/SharedPointer.h"

#include "Windows/AllowWindowsPlatformTypes.h"
MAX_INCLUDES_START
	#include "max.h"
	#include "iInstanceMgr.h"
	#include "iparamb2.h"
MAX_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"

class FMaxRendereableNode;
class FDatasmithMaxStaticMeshAttributes;

// Dummy function for calls to ProgressStart in the Max API
DWORD WINAPI DummyFunction(LPVOID Arg);

// This tell the exporter how to manage the instancing of a Geometry
enum class EMaxExporterInstanceMode
{
	NotInstanced,
	InstanceMaster,
	InstanceCopy,
	UnrealHISM
};

enum class EMaxTriMode
{
	TriNotChecked,
	TriEnabled,
	TriDisabled
};

enum class EMaxEntityType
{
	Geometry,
	Light,
	Camera,
	Dummy
};

enum class EMaxLightClass
{
	Unknown,
	TheaLightOmni,
	TheaLightSpot,
	TheaLightIES,
	TheaLightPlane,
	SpotLight,
	DirectLight,
	OmniLight,
	PhotometricLight,
	PhotoplaneLight,
	VRayLightIES,
	VRayLight,
	CoronaLight,
	ArnoldLight,
	SunEquivalent,
	SkyEquivalent,
	SkyPortal
};

class FDatasmithMaxSceneParser
{
public:
	static EMaxLightClass GetLightClass(INode* InNode);
	static bool CanBeTriMesh(Object* Obj);
	static const TArray< FString, TInlineAllocator< 4 > > CollisionNodesPrefixes; // List of supported mesh prefixes for Unreal collision

	FDatasmithMaxSceneParser();
	~FDatasmithMaxSceneParser();

	void ParseCurrentScene(bool bSelectedOnly, TSharedPtr< class FDatasmithMaxProgressManager >& ProgressManager);

	int GetRendereableNodesCount();

	EMaxEntityType GetEntityType(int NodeIndex);
	EMaxTriMode GetTriangulateMode(int NodeIndex);
	INode* GetNode(int NodeIndex);
	INode* GetCustomMeshNode(int NodeIndex);
	Mesh* GetMaxMesh(int NodeIndex);
	int32 GetRailCloneMeshIndex(int NodeIndex) const;
	TArray<Matrix3>* GetInstancesTransform(int NodeIndex);

	const TOptional<FDatasmithMaxStaticMeshAttributes>& GetDatasmithStaticMeshAttributes(int NodeIndex) const;
	EMaxExporterInstanceMode GetInstanceMode(int NodeIndex);
	INodeTab GetInstances(int NodeIndex);
	bool GetReadyToExport(int NodeIndex);
	void SetReadyToExport(int NodeIndex,bool ready);
	
	int GetMaterialsCount();
	Mtl* GetMaterial(int MaterialIndex);
	void RestoreMaterialNames();
	int GetTexturesCount();
	Texmap* GetTexture(int TextureIndex);

	int Status;
	bool bOnlySelection;

	static bool HasCollisionName(INode* Node);

private:
	void NodeEnum(INode* InNode, bool bSelectedOnly, TSharedPtr< class FDatasmithMaxProgressManager >& ProgressManager);
	void MaterialEnum(Mtl* Material, bool bAddMat);
	void LightTexEnum(INode* InNode);
	void TexEnum(Texmap* Texture);
	void NodeEnumInstanceClean();
	void ParseForestNode(INode* ForestNode);
	void ParseRailcloneNode(INode* RailCloneNode);
	bool GetActualInstances(INode *pINode, INodeTab* InstanceAndRef);


	TArray<FMaxRendereableNode> RenderableNodes;
	TArray<Mtl*> Materials;
	TArray<FString> MaterialNames;
	TArray<Texmap*> Textures;

	TSet<INode*> ToBeRemovedFromRenderableNodes;
	TSet<Mtl*> EncounteredMaterials;
	TSet<Texmap*> EncounteredTextures;
};
