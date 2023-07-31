// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithMaxSceneParser.h"

#include "DatasmithMaxAttributes.h"
#include "DatasmithMaxSceneExporter.h"
#include "DatasmithMaxProgressManager.h"
#include "DatasmithMaxLogger.h"
#include "DatasmithMaxWriter.h"

#include "Misc/Paths.h"
#include "Algo/Find.h"

#include "Windows/AllowWindowsPlatformTypes.h"

#if WITH_ITOO_INTERFACE
#pragma warning( push )
#pragma warning( disable: 4238 )
//#include "itoo/forestitreesinterface.h"
#include "itreesinterface.H"
#pragma warning( pop )

#include "ircinterface.H"
#endif // WITH_ITOO_INTERFACE

MAX_INCLUDES_START
#include "lslights.h"
#include "xref/iXrefObj.h"
MAX_INCLUDES_END

const TArray< FString, TInlineAllocator< 4 > > FDatasmithMaxSceneParser::CollisionNodesPrefixes = { TEXT("UBX"), TEXT("UCX"), TEXT("UCP"), TEXT("USP") };

class FMaxRendereableNode : public FNoncopyable
{
public:
	FMaxRendereableNode(INode* InNode, EMaxEntityType EntityType);
	FMaxRendereableNode(INode* InNode, EMaxEntityType EntityType, const TOptional<FDatasmithMaxStaticMeshAttributes>& InStaticMeshAttributes);

	INode* Node;
	EMaxExporterInstanceMode Instancemode;
	EMaxTriMode Triangulatemode;
	EMaxEntityType Entitytype;
	INodeTab Instances;
	//if 0 faces this becomes false
	bool bIsReadyToExport;

	//This is used for railclone
	TUniquePtr<Mesh> MaxMesh;
	//This is used for the forest pack. It allow to use a different node for the static mesh export
	INode* CustomMeshNode;
	//This is used only for the UnrealHISM
	int32 MeshIndex;
	TUniquePtr<TArray<Matrix3>> InstancesTransformPtr;

	//used for the mesh export when the datasmith attribute modifier is present
	TOptional<FDatasmithMaxStaticMeshAttributes> StaticMeshAttributes;

private:
	void SetTriangulateMode();
};

//***************************************************************************
// IInstanceMgr::GetInstanceMgr()->GetInstances() functions is finding
// any object references. This function collects actual instances :)
// returns whether multiple instances are found
//***************************************************************************
bool FDatasmithMaxSceneParser::GetActualInstances(INode* pINode, INodeTab* OnlyInstance)
{
	check(pINode);
	check(OnlyInstance);

	Object* Obj = pINode->GetObjOrWSMRef();
	INodeTab InstanceAndRef;
	if (IInstanceMgr::GetInstanceMgr()->GetInstances(*pINode, InstanceAndRef) < 2)
	{
		OnlyInstance->AppendNode(pINode);
		return false;
	}

	for (int k = 0; k < InstanceAndRef.Count(); k++)
	{
		if (Obj == InstanceAndRef[k]->GetObjOrWSMRef())
		{
			OnlyInstance->AppendNode(InstanceAndRef[k]);
		}
	}

	return OnlyInstance->Count() >= 2;
}

EMaxLightClass FDatasmithMaxSceneParser::GetLightClass(INode* Node)
{
	EMaxLightClass LightClass = EMaxLightClass::Unknown;
	ObjectState ObjState = Node->EvalWorldState(0);

	if (ObjState.obj->SuperClassID() == LIGHT_CLASS_ID)
	{
		LightObject* Light = (LightObject*)ObjState.obj;
		if (Light != nullptr)
		{
			Class_ID ClassID = Light->ClassID();
			if (ClassID == THEAOMNICLASS)
			{
				LightClass = EMaxLightClass::TheaLightOmni;
			}
			else if (ClassID == THEASPOTCLASS)
			{
				LightClass = EMaxLightClass::TheaLightSpot;
			}
			else if (ClassID == THEAIESCLASS)
			{
				LightClass = EMaxLightClass::TheaLightIES;
			}
			else if (ClassID == THEAPLANECLASS)
			{
				LightClass = EMaxLightClass::TheaLightPlane;
			}
			else if (ClassID == REGULARSPOTACLASS || ClassID == REGULARSPOTBCLASS)
			{
				LightClass = EMaxLightClass::SpotLight;
			}
			else if (ClassID == REGULARDIRECTACLASS || ClassID == REGULARDIRECTBCLASS)
			{
				LightClass = EMaxLightClass::DirectLight;

				INode *ParentNode = Node->GetParentNode();
				if (ParentNode)
				{
					ObjectState ParentObjectState = ParentNode->EvalWorldState(0);
					if (ParentObjectState.obj && ParentObjectState.obj->ClassID() == DAYLIGHTASSEMBLYCLASS)
					{
						LightClass = EMaxLightClass::SunEquivalent;
					}
				}
			}
			else if (ClassID == REGULAROMNICLASS)
			{
				LightClass = EMaxLightClass::OmniLight;
			}
			else if (Light->IsSubClassOf(LIGHTSCAPE_LIGHT_CLASS))
			{
				LightClass = EMaxLightClass::PhotometricLight;
			}
			else if (ClassID == PHOTOPLANEACLASS || ClassID == PHOTOPLANEBCLASS)
			{
				LightClass = EMaxLightClass::PhotoplaneLight;
			}
			else if (ClassID == VRAYLIGHTIESCLASS)
			{
				LightClass = EMaxLightClass::VRayLightIES;
			}
			else if (ClassID == VRAYLIGHTCLASS)
			{
				LightClass = EMaxLightClass::VRayLight;
			}
			else if (ClassID == CORONALIGHTCLASS)
			{
				LightClass = EMaxLightClass::CoronaLight;
			}
			else if (ClassID == ARNOLDLIGHTCLASS)
			{
				LightClass = EMaxLightClass::ArnoldLight;
			}
			else if (ClassID == SUNLIGHTACLASS ||
				ClassID == SUNLIGHTBCLASS ||
				ClassID == VRAYSUNCLASS ||
				ClassID == SUNPOSITIONERCLASS ||
				ClassID == CORONASUNCLASS ||
				ClassID == CORONASUNCLASSB)
			{
				LightClass = EMaxLightClass::SunEquivalent;
			}
			else if (ClassID == SKYLIGHTCLASS ||
				ClassID == IESSKYLIGHTCLASS ||
				ClassID == MRSKYLIGHTCLASS)
			{
				LightClass = EMaxLightClass::SkyEquivalent;
			}
			else if (ClassID == MRPORTALLIGHTCLASS)
			{
				LightClass = EMaxLightClass::SkyPortal;
			}
		}
	}

	if (LightClass == EMaxLightClass::Unknown)
	{
		DatasmithMaxLogger::Get().AddUnsupportedLight(Node);
	}

	return LightClass;
}

bool FDatasmithMaxSceneParser::CanBeTriMesh(Object* Obj)
{
	if (Obj == NULL)
	{
		return false;
	}

	int CanConvertToTri = Obj->CanConvertToType(Class_ID(TRIOBJ_CLASS_ID, 0));
	if (CanConvertToTri == 0)
	{
		return false;
	}

	TriObject* Tri = (TriObject*)Obj->ConvertToType(GetCOREInterface()->GetTime(), Class_ID(TRIOBJ_CLASS_ID, 0));
	if (Tri == NULL)
	{
		return false;
	}

	bool bResult = true;
	if (Tri->GetMesh().getNumFaces() == 0)
	{
		bResult = false;
	}

	if (Obj != Tri)
	{
		Tri->DeleteMe();
	}

	return bResult;
}

bool FDatasmithMaxSceneParser::HasCollisionName(INode* Node)
{
	FString NodeName = Node->GetName();
	FString LeftString, RightString;

	if ( NodeName.Split( TEXT("_"), &LeftString, &RightString ) )
	{
		if ( CollisionNodesPrefixes.Find( LeftString ) != INDEX_NONE )
		{
			return true;
		}
	}
	return false;
}

FMaxRendereableNode::FMaxRendereableNode(INode* InNode, EMaxEntityType Mode)
	: Node(InNode)
	, Instancemode(EMaxExporterInstanceMode::NotInstanced)
	, Triangulatemode(EMaxTriMode::TriNotChecked)
	, Entitytype(Mode)
	, Instances()
	, bIsReadyToExport(true)
	, MaxMesh(nullptr)
	, CustomMeshNode(nullptr)
	, MeshIndex(-1)
	, InstancesTransformPtr()
	, StaticMeshAttributes()
{
	SetTriangulateMode();
}

FMaxRendereableNode::FMaxRendereableNode(INode* InNode, EMaxEntityType EntityType, const TOptional<FDatasmithMaxStaticMeshAttributes>& InStaticMeshAttributes)
	: Node(InNode)
	, Instancemode(EMaxExporterInstanceMode::NotInstanced)
	, Triangulatemode(EMaxTriMode::TriNotChecked)
	, Entitytype(EntityType)
	, Instances()
	, bIsReadyToExport(true)
	, MaxMesh(nullptr)
	, CustomMeshNode(nullptr)
	, MeshIndex(-1)
	, InstancesTransformPtr()
	, StaticMeshAttributes(InStaticMeshAttributes)
{
	SetTriangulateMode();
}




void FMaxRendereableNode::SetTriangulateMode()
{
	if (Entitytype == EMaxEntityType::Geometry)
	{
		if (Node)
		{
			Object* Obj = Node->EvalWorldState(0).obj;
			if (FDatasmithMaxSceneParser::CanBeTriMesh(Obj))
			{
				Triangulatemode = EMaxTriMode::TriEnabled;
			}
			else
			{
				Triangulatemode = EMaxTriMode::TriDisabled;
			}
		}
	}
}

FDatasmithMaxSceneParser::FDatasmithMaxSceneParser()
{
	Status = 1;
	bOnlySelection = false;
}

FDatasmithMaxSceneParser::~FDatasmithMaxSceneParser()
{
}

DWORD WINAPI DummyFunction(LPVOID Arg)
{
	return (0);
}

void FDatasmithMaxSceneParser::ParseCurrentScene(bool bSelectedOnly, TSharedPtr< FDatasmithMaxProgressManager >& ProgressManager)
{
	bOnlySelection = bSelectedOnly;
	RenderableNodes.Empty();
	Materials.Empty();
	MaterialNames.Empty();
	EncounteredMaterials.Empty();
	EncounteredTextures.Empty();
	Textures.Empty();

	if (bSelectedOnly == false && GetCOREInterface()->GetUseEnvironmentMap())
	{
		TexEnum(GetCOREInterface()->GetEnvironmentMap()); //environment is not associated to any Node
	}

	ProgressManager->SetProgressStart(0);
	ProgressManager->SetProgressEnd(100);
	ProgressManager->SetMainMessage(TEXT("Parsing scene objects"));
	NodeEnum(GetCOREInterface()->GetRootNode(), bSelectedOnly, ProgressManager);
	NodeEnumInstanceClean();
}

int FDatasmithMaxSceneParser::GetRendereableNodesCount()
{
	return (int)RenderableNodes.Num();
}

EMaxEntityType FDatasmithMaxSceneParser::GetEntityType(int NodeIndex)
{
	return RenderableNodes[NodeIndex].Entitytype;
}

INode* FDatasmithMaxSceneParser::GetNode(int NodeIndex)
{
	return RenderableNodes[NodeIndex].Node;
}

INode* FDatasmithMaxSceneParser::GetCustomMeshNode(int NodeIndex)
{
	return RenderableNodes[NodeIndex].CustomMeshNode;
}

Mesh* FDatasmithMaxSceneParser::GetMaxMesh(int NodeIndex)
{
	return RenderableNodes[NodeIndex].MaxMesh.Get();
}

int32 FDatasmithMaxSceneParser::GetRailCloneMeshIndex(int NodeIndex) const
{
	return RenderableNodes[NodeIndex].MeshIndex;
}


TArray<Matrix3>* FDatasmithMaxSceneParser::GetInstancesTransform(int NodeIndex)
{
	return RenderableNodes[NodeIndex].InstancesTransformPtr.Get();
}

const TOptional<FDatasmithMaxStaticMeshAttributes>& FDatasmithMaxSceneParser::GetDatasmithStaticMeshAttributes(int NodeIndex) const
{
	return RenderableNodes[NodeIndex].StaticMeshAttributes;
}

EMaxExporterInstanceMode FDatasmithMaxSceneParser::GetInstanceMode(int NodeIndex)
{
	return RenderableNodes[NodeIndex].Instancemode;
}

INodeTab FDatasmithMaxSceneParser::GetInstances(int NodeIndex)
{
	return RenderableNodes[NodeIndex].Instances;
}

bool FDatasmithMaxSceneParser::GetReadyToExport(int NodeIndex)
{
	return RenderableNodes[NodeIndex].bIsReadyToExport;
}

void FDatasmithMaxSceneParser::SetReadyToExport(int NodeIndex, bool ready)
{
	RenderableNodes[NodeIndex].bIsReadyToExport=ready;
}

int FDatasmithMaxSceneParser::GetMaterialsCount()
{
	return (int)Materials.Num();
}

Mtl* FDatasmithMaxSceneParser::GetMaterial(int MaterialIndex)
{
	return Materials[MaterialIndex];
}

void FDatasmithMaxSceneParser::RestoreMaterialNames()
{
	for (int i = 0; i < MaterialNames.Num(); i++)
	{
		Materials[i]->SetName(*MaterialNames[i]);
	}
}

int FDatasmithMaxSceneParser::GetTexturesCount()
{
	return (int)Textures.Num();
}

Texmap* FDatasmithMaxSceneParser::GetTexture(int TextureIndex)
{
	return Textures[TextureIndex];
}

void FDatasmithMaxSceneParser::MaterialEnum(Mtl* Material, bool bAddMaterial)
{
	if (Material == NULL)
	{
		return;
	}


	if (EncounteredMaterials.Contains(Material))
	{
		return;
	}


	if (FDatasmithMaxMatHelper::GetMaterialClass(Material) == EDSMaterialType::XRefMat)
	{
		MaterialEnum(FDatasmithMaxMatHelper::GetRenderedXRefMaterial(Material), true);
	}
	else if (FDatasmithMaxMatHelper::GetMaterialClass(Material) == EDSMaterialType::MultiMat)
	{
		for (int i = 0; i < Material->NumSubMtls(); i++)
		{
			MaterialEnum(Material->GetSubMtl(i), true);
		}
	}
	else
	{
		if (bAddMaterial)
		{
			int DuplicateCount = 0;
			FString ProposedName = Material->GetName().data();
			MaterialNames.Add(*ProposedName);
			FDatasmithUtils::SanitizeNameInplace(ProposedName);
			for (int i = 0; i < Materials.Num(); i++)
			{
				if (ProposedName == FDatasmithUtils::SanitizeName(Materials[i]->GetName().data()))
				{
					DuplicateCount++;
					ProposedName = FDatasmithUtils::SanitizeName(Material->GetName().data()) + TEXT("_(") + FString::FromInt(DuplicateCount) + TEXT(")");
				}
			}
			Material->SetName(*ProposedName);
			EncounteredMaterials.Add(Material);
			Materials.Add(Material);
		}

		bool bAddRecursively = Material->ClassID() == THEARANDOMCLASS || Material->ClassID() == VRAYBLENDMATCLASS || Material->ClassID() == CORONALAYERMATCLASS;
		for (int i = 0; i < Material->NumSubMtls(); i++)
		{
			MaterialEnum(Material->GetSubMtl(i), bAddRecursively);
		}

		for (int i = 0; i < Material->NumSubTexmaps(); i++)
		{
			Texmap* SubTexture = Material->GetSubTexmap(i);
			if (SubTexture != NULL)
			{
				TexEnum(SubTexture);
			}
		}
	}
}

void FDatasmithMaxSceneParser::LightTexEnum(INode* Node)
{
	ObjectState ObjState = Node->EvalWorldState(0);
	LightObject* Light = (LightObject*)ObjState.obj;

	if (Light == NULL)
	{
		return;
	}

	int NumParamBlocks = Light->NumParamBlocks();

	for (int j = 0; j < NumParamBlocks; j++)
	{
		IParamBlock2* ParamBlock2 = Light->GetParamBlockByID((short)j);
		if (ParamBlock2)
		{
			ParamBlockDesc2* ParamBlockDesc = ParamBlock2->GetDesc();

			if (ParamBlockDesc != NULL)
			{
				for (int i = 0; i < ParamBlockDesc->count; i++)
				{
					const ParamDef& ParamDefinition = ParamBlockDesc->paramdefs[i];

					if (FCString::Stricmp(ParamDefinition.int_name, TEXT("texmap")) == 0)
					{
						Texmap* Texture = ParamBlock2->GetTexmap(ParamDefinition.ID, GetCOREInterface()->GetTime());
						if (Texture != NULL)
						{
							TexEnum(Texture);
						}
					}
				}
			}

			ParamBlock2->ReleaseDesc();
		}
	}
}

void FDatasmithMaxSceneParser::TexEnum(Texmap* Texture)
{
	if (Texture == NULL)
	{
		return;
	}

	if (EncounteredTextures.Contains(Texture))
	{
		return;
	}

	EncounteredTextures.Add(Texture);
	Textures.Add(Texture);

	for (int i = 0; i < Texture->NumSubTexmaps(); i++)
	{
		Texmap *SubTexture = Texture->GetSubTexmap(i);
		if (SubTexture != NULL)
		{
			TexEnum(SubTexture);
		}
	}
}

void FDatasmithMaxSceneParser::NodeEnum(INode* Node, bool bSelectedOnly, TSharedPtr< FDatasmithMaxProgressManager >& ProgressManager)
{
	// nodes comming from XRef Scenes/Objects could be null
	if (Node == NULL)
	{
		return;
	}

	// Is the Node hidden?
	BOOL bIsNodeHidden = Node->IsNodeHidden(TRUE);

	if (bIsNodeHidden == false)
	{
		for (int XRefChild = 0; XRefChild < Node->GetXRefFileCount(); ++XRefChild)
		{
			FString Path = FDatasmithMaxSceneExporter::GetActualPath(Node->GetXRefFile(XRefChild).GetFileName());
			if (FPaths::FileExists(Path) == false)
			{
				FString Error = FString("XRefScene file \"") + FPaths::GetCleanFilename(*Path) + FString("\" cannot be found");
				DatasmithMaxLogger::Get().AddMissingAssetError(*Error);
			}
			else
			{
				NodeEnum(Node->GetXRefTree(XRefChild), bSelectedOnly, ProgressManager);
			}
		}
	}

	// Get the ObjectState.
	// The ObjectState is the structure Other flows up the pipeline.
	// It contains a matrix, a material index, some flags for channels,
	// and a pointer to the object in the pipeline.
	ObjectState ObjState = Node->EvalWorldState(0);

	if (ObjState.obj == NULL)
	{
		int NumChildren = Node->NumberOfChildren();
		int LastDisplayedProgress = -1;
		for (int ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
		{
			int Progress = (ChildIndex * 100) / NumChildren;
			if (Progress != LastDisplayedProgress)
			{
				LastDisplayedProgress = Progress;
				FString Msg = FString::Printf(TEXT("%d%%"), Progress).Left(255);
				// Show progress bar without the Cancel button
				ProgressManager->ProgressEvent(Progress * 0.01f, *Msg);
			}

			NodeEnum(Node->GetChildNode(ChildIndex), bSelectedOnly, ProgressManager);
		}
		return;
	}

	if (bIsNodeHidden == false)
	{
		if (ObjState.obj->ClassID() == XREFOBJ_CLASS_ID)
		{
			//max2017 and newer versions allow developers to get the file, with previous versions you only can retrieve the active one
			#ifdef MAX_RELEASE_R19
				FString Path = FDatasmithMaxSceneExporter::GetActualPath(((IXRefObject8*)ObjState.obj)->GetFile(FALSE).GetFileName());
			#else
				FString Path = FDatasmithMaxSceneExporter::GetActualPath(((IXRefObject8*)ObjState.obj)->GetActiveFile().GetFileName());
			#endif
			if (FPaths::FileExists(Path) == false)
			{
				FString Error = FString("XRefObj file \"") + FPaths::GetCleanFilename(*Path) + FString("\" cannot be found");
				DatasmithMaxLogger::Get().AddMissingAssetError(*Error);
				bIsNodeHidden = true;
			}
		}
	}

	// Examine the superclass ID in order to figure out what kind
	// of object we are dealing with.

	if ( (!bSelectedOnly || Node->Selected() != 0) && bIsNodeHidden == false)
	{
		switch (ObjState.obj->SuperClassID())
		{
			case CAMERA_CLASS_ID:
			{
				RenderableNodes.Emplace(Node, EMaxEntityType::Camera);
				break;
			}

			case LIGHT_CLASS_ID:
			{
				EMaxLightClass lightClass = GetLightClass(Node);
				if (lightClass != EMaxLightClass::Unknown)
				{
					RenderableNodes.Emplace(Node, EMaxEntityType::Light);
					LightTexEnum(Node);
				}
				break;
			}

			case SHAPE_CLASS_ID:
			{
				if ( !ObjState.obj->IsRenderable() || !Node->Renderable() )
				{
					if (Node->NumberOfChildren() > 0)
					{
						RenderableNodes.Emplace(Node, EMaxEntityType::Dummy);
					}
					break;
				}
				RenderableNodes.Emplace(Node, EMaxEntityType::Geometry);
				MaterialEnum(Node->GetMtl(), true);
				break;
			}

			case GEOMOBJECT_CLASS_ID:
			{
				Class_ID ClassID = ObjState.obj->ClassID();

				if (ClassID == Class_ID(TARGET_CLASS_ID, 0x0))
				{
					RenderableNodes.Emplace(Node, EMaxEntityType::Dummy);
				}
				else if ( !ObjState.obj->IsRenderable() || !Node->Renderable())
				{
					if (Node->NumberOfChildren() > 0)
					{
						RenderableNodes.Emplace(Node, EMaxEntityType::Dummy);
					}
				}
				else if (ClassID == VRAYPLANE_CLASS_ID ||
					ClassID == VRAYFUR_CLASS_ID ||
					ClassID == VRAYSPHERE_CLASS_ID ||
					ClassID == MENTALPROXY_CLASS_ID ||
					ClassID == PFLOWEVENT_CLASS_ID ||
					ClassID == POPULATE_CLASS_ID)
				{
					if (Node->NumberOfChildren() > 0)
					{
						RenderableNodes.Emplace(Node, EMaxEntityType::Dummy);
					}
				}
				else if (ClassID == ITOOFOREST_CLASS_ID)
				{
					ParseForestNode(Node);
				}
				else if (ClassID == RAILCLONE_CLASS_ID)
				{
					ParseRailcloneNode(Node);
				}
				else if ( !HasCollisionName(Node) )
				{
					RenderableNodes.Emplace(Node, EMaxEntityType::Geometry, FDatasmithMaxStaticMeshAttributes::ExtractStaticMeshAttributes(Node));

					const FMaxRendereableNode& RenderedNode = RenderableNodes.Last();
					if (RenderedNode.StaticMeshAttributes && RenderedNode.StaticMeshAttributes->GetCustomCollisonNode())
					{
						ToBeRemovedFromRenderableNodes.Add(RenderedNode.StaticMeshAttributes->GetCustomCollisonNode());
					}

					if (!RenderedNode.StaticMeshAttributes || RenderedNode.StaticMeshAttributes->GetExportMode() == EStaticMeshExportMode::Default)
					{
						MaterialEnum(Node->GetMtl(), true);
					}
				}
				break;
			}

			case HELPER_CLASS_ID:
			{
				RenderableNodes.Emplace(Node, EMaxEntityType::Dummy);
				break;
			}
			default:
			{
				if (Node->NumberOfChildren() > 0)
				{
					RenderableNodes.Emplace(Node, EMaxEntityType::Dummy);
				}
				break;
			}
		}
	}

	// For each child of this Node, we recurse into ourselves
	// until no more children are found.
	for (int ChildIndex = 0; ChildIndex < Node->NumberOfChildren(); ChildIndex++)
	{
		NodeEnum(Node->GetChildNode(ChildIndex), bSelectedOnly, ProgressManager);
	}
}

void FDatasmithMaxSceneParser::NodeEnumInstanceClean()
{
	for (int CurrentNode = 0; CurrentNode < RenderableNodes.Num(); ++CurrentNode)
	{
		FMaxRendereableNode& RenderableNode = RenderableNodes[CurrentNode];
		if (RenderableNode.Instancemode != EMaxExporterInstanceMode::UnrealHISM)
		{
			if (GetActualInstances(RenderableNode.Node, &RenderableNode.Instances))
			{
				RenderableNode.Instancemode = EMaxExporterInstanceMode::InstanceMaster;

				// Remove duplicated Instances nodes from the RenderableNodes without preserving the order.
				// The RenderableNodes order only affects the order of the StaticMeshes and ActorMeshes in the .udatasmith file
				for (int32 h = RenderableNodes.Num() - 1; h > CurrentNode; --h)
				{
					if (RenderableNode.Instances.Contains(RenderableNodes[h].Node))
					{
						bool bAllowShrink = false; // As we keep references on elements of this array, we cannot allow a realloc.
						RenderableNodes.RemoveAtSwap(h, 1, bAllowShrink);
					}
				}
			}
			else
			{
				RenderableNode.Instancemode = EMaxExporterInstanceMode::NotInstanced;
			}
		}

		if (ToBeRemovedFromRenderableNodes.Contains(RenderableNode.Node))
		{
			if (RenderableNode.StaticMeshAttributes)
			{
				FString Error = FString(TEXT("The datasmith attributes aren't supported for a collision mesh. (")) + FString((TCHAR*) RenderableNode.Node->GetName()) + FString(")");
				DatasmithMaxLogger::Get().AddGeneralError(*Error);
			}

			RenderableNodes.RemoveAtSwap(CurrentNode);

			// We don't want to jump over the node that was swap in the place of the removed node
			--CurrentNode;
			continue;
		}
	}
}

#if WITH_ITOO_INTERFACE

void FDatasmithMaxSceneParser::ParseForestNode(INode* ForestNode)
{
	TimeValue CurrentTime = GetCOREInterface()->GetTime();

	ITreesInterface* ITrees = GetTreesInterface(ForestNode->GetObjectRef());
	ITrees->IForestRenderBegin(CurrentTime);

	TMap<int, FMaxRendereableNode*> AlreadyAddedSpec;
	int NumInstances;
	TForestInstance* ForestInstance = (TForestInstance*)ITrees->IForestGetRenderNodes(NumInstances);

	ulong ForestHandle = ForestNode->GetHandle();
	FString ForestName = ForestNode->GetName();
	Matrix3 ForestMatrix = ForestNode->GetNodeTM(CurrentTime);
	if (ForestInstance && NumInstances)
	{
		for (int i = 0; i < NumInstances; i++, ForestInstance++)
		{
			if (ForestInstance->node != NULL)
			{
				int VirtualMaster = ITrees->IForestGetSpecID(i);
				if (!AlreadyAddedSpec.Find(VirtualMaster))
				{
					RenderableNodes.Emplace(ForestNode, EMaxEntityType::Geometry, FDatasmithMaxStaticMeshAttributes::ExtractStaticMeshAttributes(ForestInstance->node));
					RenderableNodes.Last().Instancemode = EMaxExporterInstanceMode::UnrealHISM;
					RenderableNodes.Last().bIsReadyToExport = true;
					RenderableNodes.Last().InstancesTransformPtr = MakeUnique<TArray<Matrix3>>();
					AlreadyAddedSpec.Add(VirtualMaster, &RenderableNodes.Last());
					RenderableNodes.Last().MeshIndex = AlreadyAddedSpec.Num();
					RenderableNodes.Last().CustomMeshNode = ForestInstance->node;

					MaterialEnum(ForestInstance->node->GetMtl(), true);
				}

				 AlreadyAddedSpec[VirtualMaster]->InstancesTransformPtr->Emplace(ForestInstance->tm);

			}
		}
	}
	ITrees->IForestClearRenderNodes();
	ITrees->IForestRenderEnd(CurrentTime);
}

void FDatasmithMaxSceneParser::ParseRailcloneNode(INode* RailCloneNode)
{
	if (RailCloneNode == nullptr)
	{
		return;
	}

	TimeValue CurrentTime = GetCOREInterface()->GetTime();

	IRCStaticInterface* RCStaticInterface = GetRCStaticInterface();
	if (!RCStaticInterface)
	{
		return;
	}

	RCStaticInterface->IRCRegisterEngine();
	IRCInterface* RCInterface = GetRCInterface(RailCloneNode->GetObjectRef());
	if (!RCInterface)
	{
		return;
	}

	RCInterface->IRCRenderBegin(CurrentTime);

	int NumInstances;
	TRCInstance* RCInstance = (TRCInstance *)RCInterface->IRCGetInstances(NumInstances);

	if (RCInstance && NumInstances > 0)
	{
		MaterialEnum(RailCloneNode->GetMtl(), true);
		int32 NextMeshIndex = 0;
		TMap<Mesh*, int32> RenderableNodeIndicesMap;
		for (int j = 0; j < NumInstances; j++, RCInstance++)
		{
			if (RCInstance && RCInstance->mesh)
			{
				if (int32* RenderableNodeIndexPtr = RenderableNodeIndicesMap.Find(RCInstance->mesh))
				{
					RenderableNodes[*RenderableNodeIndexPtr].InstancesTransformPtr->Emplace(RCInstance->tm);
				}
				else
				{
					RenderableNodeIndicesMap.Add(RCInstance->mesh, RenderableNodes.Num());
					FMaxRendereableNode& RenderableNode = RenderableNodes.Emplace_GetRef(RailCloneNode, EMaxEntityType::Geometry);
					RenderableNode.Instancemode = EMaxExporterInstanceMode::UnrealHISM;
					RenderableNode.bIsReadyToExport = true;
					RenderableNode.MaxMesh = MakeUnique<Mesh>(*RCInstance->mesh);
					RenderableNode.MeshIndex = NextMeshIndex++;
					RenderableNode.InstancesTransformPtr = MakeUnique<TArray<Matrix3>>();

					RenderableNode.InstancesTransformPtr->Emplace(RCInstance->tm);
				}
			}
		}
	}

	RCInterface->IRCClearInstances();
	RCInterface->IRCClearMeshes();
	RCInterface->IRCRenderEnd(CurrentTime);
}

#else // WITH_ITOO_INTERFACE

void FDatasmithMaxSceneParser::ParseForestNode(INode*) {}
void FDatasmithMaxSceneParser::ParseRailcloneNode(INode*) {}

#endif // WITH_ITOO_INTERFACE

#include "Windows/HideWindowsPlatformTypes.h"
