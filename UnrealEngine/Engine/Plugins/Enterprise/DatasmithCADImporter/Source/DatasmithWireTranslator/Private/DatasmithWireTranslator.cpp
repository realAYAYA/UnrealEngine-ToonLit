// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithWireTranslator.h"

#include "CADOptions.h"
#include "Containers/List.h"
#include "DatasmithImportOptions.h"
#include "DatasmithSceneFactory.h"
#include "DatasmithTranslator.h"
#include "DatasmithUtils.h"
#include "DatasmithWireTranslatorModule.h"
#include "HAL/ConsoleManager.h"
#include "IDatasmithSceneElements.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "OpenModelUtils.h"
#include "Utility/DatasmithMeshHelper.h"

#include "StaticMeshDescription.h"
#include "StaticMeshOperations.h"

#if WITH_EDITOR
#include "Editor.h"
#include "IMessageLogListing.h"
#include "Logging/TokenizedMessage.h"
#include "MessageLogModule.h"
#include "Modules/ModuleManager.h"
#endif

#include "AliasBrepConverter.h"
#include "AliasModelToCADKernelConverter.h"
#include "AliasModelToTechSoftConverter.h" // requires Techsoft as public dependency
#include "CADInterfacesModule.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif

#ifdef USE_OPENMODEL
#include <AlChannel.h>
#include <AlDagNode.h>
#include <AlGroupNode.h>
#include <AlLayer.h>
#include <AlLinkItem.h>
#include <AlList.h>
#include <AlMesh.h>
#include <AlMeshNode.h>
#include <AlPersistentID.h>
#include <AlRetrieveOptions.h>
#include <AlShader.h>
#include <AlShadingFieldItem.h>
#include <AlShell.h>
#include <AlShellNode.h>
#include <AlSurface.h>
#include <AlSurfaceNode.h>
#include <AlTesselate.h>
#include <AlTrimRegion.h>
#include <AlTM.h>
#include <AlUniverse.h>
#endif

DEFINE_LOG_CATEGORY_STATIC(LogDatasmithWireTranslator, Log, All);

#define LOCTEXT_NAMESPACE "DatasmithWireTranslator"

#define WRONG_VERSION_TEXT "Unsupported version of Alias detected. Please upgrade to Alias 2021.3 (or later version)."
#define CAD_INTERFACE_UNAVAILABLE "CAD Interface module is unavailable. Meshing will be done by Alias."

#ifdef OPEN_MODEL_2023_0

static bool bGAliasSewByMaterial = false;
FAutoConsoleVariableRef GAliasSewByMaterial(
	TEXT("ds.CADTranslator.Alias.SewByMaterial"),
	bGAliasSewByMaterial,
	TEXT("Enable Sew action merges BReps according to their material i.e. only BReps associated with same material can be merged together.\
Default is disable\n"),
ECVF_Default);

static bool bGAliasLayerAsActor = false;
FAutoConsoleVariableRef GAliasLayerAsActor(
	TEXT("ds.CADTranslator.Alias.LayersAsActors"),
	bGAliasLayerAsActor,
	TEXT("If true, the first level of actors in the outliner is the layers.\
Default is false\n"),
ECVF_Default);

#endif 


namespace UE_DATASMITHWIRETRANSLATOR_NAMESPACE
{

static bool bGSewByMaterial = false;
static bool bGLayersAsActors = false;

static const FColor DefaultColor = FColor(200, 200, 200);

#ifdef USE_OPENMODEL

const uint64 LibAliasNext_Version = 0xffffffffffffffffull;
const uint64 LibAlias2023_1_0_Version = 8162778619576619;
const uint64 LibAlias2023_0_0_Version = 8162774324609149;
const uint64 LibAlias2022_2_0_Version = 7881307937833405;
const uint64 LibAlias2022_1_0_Version = 7881303642865885;
const uint64 LibAlias2022_0_1_Version = 7881299347964005;
const uint64 LibAlias2021_3_2_Version = 7599833027117059;
const uint64 LibAlias2021_3_1_Version = 7599824433840131;
const uint64 LibAlias2021_3_0_Version = 7599824424206339;
const uint64 LibAlias2021_Version = 7599824377020416;
const uint64 LibAlias2020_Version = 7318349414924288;
const uint64 LibAlias2019_Version = 5000000000000000;

#if defined(OPEN_MODEL_2020)
const uint64 LibAliasVersionMin = LibAlias2019_Version;
const uint64 LibAliasVersionMax = LibAlias2021_3_0_Version;
const FString AliasVersionChar = TEXT("AliasStudio 2020, Model files");
#elif defined(OPEN_MODEL_2021_3)
const uint64 LibAliasVersionMin = LibAlias2021_3_0_Version;
const uint64 LibAliasVersionMax = LibAlias2022_0_1_Version;
const FString  AliasVersionChar = TEXT("AliasStudio 2021.3, Model files");
#elif defined(OPEN_MODEL_2022)
const uint64 LibAliasVersionMin = LibAlias2022_0_1_Version;
const uint64 LibAliasVersionMax = LibAlias2022_1_0_Version;
const FString AliasVersionChar = TEXT("AliasStudio 2022, Model files");
#elif defined(OPEN_MODEL_2022_1)
const uint64 LibAliasVersionMin = LibAlias2022_1_0_Version;
const uint64 LibAliasVersionMax = LibAlias2022_2_0_Version;
const FString AliasVersionChar = TEXT("AliasStudio 2022.1, Model files");
#elif defined(OPEN_MODEL_2022_2)
const uint64 LibAliasVersionMin = LibAlias2022_2_0_Version;
const uint64 LibAliasVersionMax = LibAlias2023_0_0_Version;
const FString AliasVersionChar = TEXT("AliasStudio 2022.2, Model files");
#elif defined(OPEN_MODEL_2023_0)
const uint64 LibAliasVersionMin = LibAlias2023_0_0_Version;
const uint64 LibAliasVersionMax = LibAlias2023_1_0_Version;
const FString AliasVersionChar = TEXT("AliasStudio 2023.0, Model files");
#elif defined(OPEN_MODEL_2023_1)
const uint64 LibAliasVersionMin = LibAlias2023_1_0_Version;
const uint64 LibAliasVersionMax = LibAliasNext_Version;
const FString AliasVersionChar = TEXT("AliasStudio 2023.1, Model files");
#endif

// Alias material management (to allow sew of BReps of different materials):
// To be compatible with "Retessellate" function, Alias material management as to be the same than CAD (TechSoft) import. 
// As a reminder: the name and slot of UE Material from CAD is based on CAD material/color data i.e. RGBA Color components => "UE Material slot" (int32) and "UE Material name" (FString = FString::FromInt("UE Material slot"))
// UE Material Label is free.
// 
// During Retessellate step, Color/Material of each CAD Faces are known, so "UE Material slot" can be deduced.
// 
// For Alias import:
// Alias BRep is exported in CAD modeler (CADKernel, TechSoft, ...)
// Material is build in UE, 
// From an Alias Material, a unique Color is generated.
// This Color is associated to the BRep Shell/face in the CAD modeler
// The name and slot of the associated UE Material is defined from this color
// So at the retessellate step, nothing changes from the CAD Retessellate process
// 
// The unique Color of an Alias Material is defined as follows:
// TypeHash(Alias Material Name) => uint24 == 3 uint8 => RGB components of the color  

FColor CreateShaderColorFromShaderName(const FString& ShaderName)
{
	const uint32 ShaderHash = GetTypeHash(*ShaderName);
	const uint32 Red = (ShaderHash & 0xff000000) >> 24;
	const uint32 Green = (ShaderHash & 0x00ff0000) >> 16;
	const uint32 Blue = (ShaderHash & 0x0000ff00) >> 8;
	return FColor(Red, Green, Blue);
}

int32 CreateShaderId(const FColor& ShaderColor)
{
	return FMath::Abs((int32)GetTypeHash(ShaderColor));
}

class BodyData
{
private:
	TArray<TPair<TSharedPtr<AlDagNode>, FColor>> Shells;
	TSet<FString> ShaderNames;

	FString Label;
	FString LayerName;
	bool bCadData = true;

public:

	BodyData(const FString& InLayerName, bool bInCadData)
		: bCadData(bInCadData)
	{
		LayerName = InLayerName;
	};

	bool IsCadData() const
	{
		return bCadData;
	}

	void SetLabel(const FString& InLabel)
	{
		Label = InLabel;
	}

	FColor LastColor;
	bool bWarningNotLogYet = true;
	void Add(const TSharedPtr<AlDagNode>& Node, FColor Color, const FString& ShaderName)
	{
		using namespace CADLibrary;
		if (ShaderNames.Num() == FImportParameters::GMaxMaterialCountPerMesh)
		{
			if (!ShaderNames.Contains(ShaderName))
			{
			UE_LOG(LogDatasmithWireTranslator, Warning, TEXT("The main UE5 rendering systems do not support more than 256 materials per mesh and the limit has been defined to %d materials. Only the first %d materials are kept. The others are replaced by the last one"), FImportParameters::GMaxMaterialCountPerMesh, FImportParameters::GMaxMaterialCountPerMesh);
				bWarningNotLogYet = false;
				Color = LastColor;
			}
		}
		else
		{
			ShaderNames.Add(ShaderName);
			LastColor = Color;
		}
		Shells.Emplace(Node, Color);
	}

	void Reserve(int32 MaxElement)
	{
		Shells.Reserve(MaxElement);
		ShaderNames.Reserve(MaxElement);
	}

	const TSet<FString>& GetShaderNames() const
	{
		return ShaderNames;
	}

	const FString& GetLayerName() const
	{
		return LayerName;
	}

	const TArray<TPair<TSharedPtr<AlDagNode>, FColor>>& GetShells() const
	{
		return Shells;
	}

	// Generates BodyData's unique id from AlDagNode objects
	uint32 GetUuid(const uint32& ParentUuid)
	{
		if (Shells.Num() == 0)
		{
			return ParentUuid;
		}

		if (Shells.Num() > 1)
		{
			Shells.Sort([&](const TPair<TSharedPtr<AlDagNode>, FColor>& NodeA, const TPair<TSharedPtr<AlDagNode>, FColor>& NodeB) { return OpenModelUtils::GetAlDagNodeUuid(*NodeA.Key) < OpenModelUtils::GetAlDagNodeUuid(*NodeB.Key); });
		}

		uint32 BodyUUID = 0;
		for (const TPair<TSharedPtr<AlDagNode>, FColor>& DagNode : Shells)
		{
			BodyUUID = HashCombine(BodyUUID, OpenModelUtils::GetAlDagNodeUuid(*DagNode.Key));
		}

		return HashCombine(ParentUuid, BodyUUID);
	}
};

uint32 GetSceneFileHash(const FString& FullPath, const FString& FileName)
{
	FFileStatData FileStatData = IFileManager::Get().GetStatData(*FullPath);

	int64 FileSize = FileStatData.FileSize;
	FDateTime ModificationTime = FileStatData.ModificationTime;

	uint32 FileHash = GetTypeHash(FileName);
	FileHash = HashCombine(FileHash, GetTypeHash(FileSize));
	FileHash = HashCombine(FileHash, GetTypeHash(ModificationTime));

	return FileHash;
}

class FWireTranslatorImpl
{
public:
	FWireTranslatorImpl(const FString& InSceneFullName, TSharedRef<IDatasmithScene> InScene)
		: DatasmithScene(InScene)
		, SceneName(FPaths::GetBaseFilename(InSceneFullName))
		, CurrentPath(FPaths::GetPath(InSceneFullName))
		, SceneFullPath(InSceneFullName)
		, SceneFileHash(0)
		, AlRootNode(nullptr)
	{
		// Set ProductName, ProductVersion in DatasmithScene for Analytics purpose
		uint64 AliasFileVersion = FPlatformMisc::GetFileVersion(TEXT("libalias_api.dll"));

		DatasmithScene->SetHost(TEXT("Alias"));
		DatasmithScene->SetVendor(TEXT("Autodesk"));
		DatasmithScene->SetProductName(TEXT("Alias Tools"));

		if (AliasFileVersion < LibAlias2020_Version)
		{
			DatasmithScene->SetExporterSDKVersion(TEXT("2019"));
			DatasmithScene->SetProductVersion(TEXT("Alias 2019"));
		}
		else if (AliasFileVersion < LibAlias2021_Version)
		{
			DatasmithScene->SetExporterSDKVersion(TEXT("2020"));
			DatasmithScene->SetProductVersion(TEXT("Alias 2020"));
		}
		else if (AliasFileVersion < LibAlias2021_3_0_Version)
		{
			DatasmithScene->SetExporterSDKVersion(TEXT("2021.0"));
			DatasmithScene->SetProductVersion(TEXT("Alias 2021.0"));
		}
		else if (AliasFileVersion < LibAlias2021_3_1_Version)
		{
			DatasmithScene->SetExporterSDKVersion(TEXT("2021.3.0"));
			DatasmithScene->SetProductVersion(TEXT("Alias 2021.3.0"));
		}
		else if (AliasFileVersion < LibAlias2021_3_2_Version)
		{
			DatasmithScene->SetExporterSDKVersion(TEXT("2021.3.1"));
			DatasmithScene->SetProductVersion(TEXT("Alias 2021.3.1"));
		}
		else if (AliasFileVersion < LibAlias2022_0_1_Version)
		{
			DatasmithScene->SetExporterSDKVersion(TEXT("2021.3.2"));
			DatasmithScene->SetProductVersion(TEXT("Alias 2021.3.2"));
		}
		else if (AliasFileVersion < LibAlias2022_1_0_Version)
		{
			DatasmithScene->SetExporterSDKVersion(TEXT("2022"));
			DatasmithScene->SetProductVersion(TEXT("Alias 2022"));
		}
		else if (AliasFileVersion < LibAlias2022_2_0_Version)
		{
			DatasmithScene->SetExporterSDKVersion(TEXT("2022.1"));
			DatasmithScene->SetProductVersion(TEXT("Alias 2022.1"));
		}
		else
		{
			DatasmithScene->SetExporterSDKVersion(TEXT("2022.2"));
			DatasmithScene->SetProductVersion(TEXT("Alias 2022.2"));
		}

		CADLibrary::FImportParameters ImportParameters;
		if (CADLibrary::FImportParameters::bGDisableCADKernelTessellation)
		{
			TSharedRef<FAliasModelToTechSoftConverter> AliasToTechSoftConverter = MakeShared<FAliasModelToTechSoftConverter>(ImportParameters);
			CADModelConverter = AliasToTechSoftConverter;
			AliasBRepConverter = AliasToTechSoftConverter;
		}
		else
		{
			TSharedRef<FAliasModelToCADKernelConverter> AliasToCADKernelConverter = MakeShared<FAliasModelToCADKernelConverter>(ImportParameters);
			CADModelConverter = AliasToCADKernelConverter;
			AliasBRepConverter = AliasToCADKernelConverter;
		}

		IConsoleVariable* CVarDSAliasSewByMaterialEnabled = IConsoleManager::Get().FindConsoleVariable(TEXT("ds.CADTranslator.Alias.SewByMaterial"));
		if (CVarDSAliasSewByMaterialEnabled)
		{
			bGSewByMaterial = CVarDSAliasSewByMaterialEnabled->GetBool();
		}

		IConsoleVariable* CVarDSAliasLayersAsActorsEnabled = IConsoleManager::Get().FindConsoleVariable(TEXT("ds.CADTranslator.Alias.LayersAsActors"));
		if (CVarDSAliasLayersAsActorsEnabled)
		{
			bGLayersAsActors = CVarDSAliasLayersAsActorsEnabled->GetBool();
		}
	}

	~FWireTranslatorImpl()
	{
		AlUniverse::deleteAll();
	}

	bool Read();
	TOptional<FMeshDescription> GetMeshDescription(TSharedRef<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& OutMeshParameters);
	TOptional<FMeshDescription> GetMeshDescription(TSharedRef<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters, TSharedRef<BodyData> BodyTemp);

	void SetTessellationOptions(const FDatasmithTessellationOptions& Options);
	void SetOutputPath(const FString& Path) { OutputPath = Path; }

	bool LoadStaticMesh(const TSharedRef<IDatasmithMeshElement> MeshElement, FDatasmithMeshElementPayload& OutMeshPayload, const FDatasmithTessellationOptions& InTessellationOptions);

private:

	struct FDagNodeInfo
	{
		uint32 Uuid;  // Use for actor name
		FString Label;
		TSharedPtr<IDatasmithActorElement> ActorElement;
		FString LayerName;
		bool bLayerIsVisible = true;

		TMap<FString, TSharedPtr<IDatasmithActorElement>> LayerToActorElement;
		FDagNodeInfo* ParentInfo = nullptr;

		FDagNodeInfo(FDagNodeInfo& InParentInfo)
			: ParentInfo(&InParentInfo)
		{}

		FDagNodeInfo()
			: ParentInfo(nullptr)
		{}

		void ExtractDagNodeInfo(AlDagNode& CurrentNode);
		void ExtractDagNodeInfo(BodyData& CurrentNode);
		void ExtractDagNodeLayer(const AlDagNode& InDagNode);
	};

	bool GetDagLeaves();
	bool GetShader();
	bool RecurseDagForLeaves(const TSharedPtr<AlDagNode>& DagNode, FDagNodeInfo& ParentInfo);
	void RecurseDagForLeavesNoMerge(const TSharedPtr<AlDagNode>& DagNode, FDagNodeInfo& ParentInfo);
	void DagForLeavesNoMerge(const TSharedPtr<AlDagNode>& DagNode, FDagNodeInfo& ParentInfo);
	bool ProcessAlGroupNode(AlDagNode& GroupNode, FDagNodeInfo& ParentInfo);
	void ProcessAlShellNode(const TSharedPtr<AlDagNode>& ShellNode, FDagNodeInfo& ParentInfo, const FString& ShaderName);
	void ProcessBodyNode(const TSharedPtr<BodyData>& Body, FDagNodeInfo& ParentInfo);
	TSharedPtr<IDatasmithMeshElement> FindOrAddMeshElement(const TSharedPtr<BodyData>& Body, const FDagNodeInfo& ParentInfo);
	TSharedPtr<IDatasmithMeshElement> FindOrAddMeshElement(const TSharedPtr<AlDagNode>& ShellNode, const FDagNodeInfo& ParentInfo, const FString& ShaderName);
	TSharedPtr<IDatasmithActorElement> FindOrAddParentActor(FDagNodeInfo& ParentInfo, const FString& LayerName);
	TSharedPtr<IDatasmithActorElement> FindOrAddLayerActor(const FString& LayerName);

	void RecurseDeleteEmptyActor(TSharedPtr<IDatasmithActorElement> Actor);

	TOptional<FMeshDescription> GetMeshOfShellNode(AlDagNode& DagNode, TSharedRef<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters);
	TOptional<FMeshDescription> GetMeshOfNodeMesh(AlDagNode& DagNode, TSharedRef<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters, AlMatrix4x4* AlMeshInvGlobalMatrix = nullptr);
	TOptional<FMeshDescription> GetMeshOfShellBody(TSharedRef<BodyData> DagNode, TSharedRef<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters);
	TOptional<FMeshDescription> GetMeshOfMeshBody(TSharedRef<BodyData> DagNode, TSharedRef<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters);

	void AddNodeInBodyGroup(TSharedPtr<AlDagNode>& DagNode, const FString& ShaderName, TMap<uint32, TSharedPtr<BodyData>>& ShellToProcess, bool bIsAPatch, uint32 MaxSize);

	TOptional<FMeshDescription> MeshDagNodeWithExternalMesher(AlDagNode& DagNode, TSharedRef<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters);
	TOptional<FMeshDescription> MeshDagNodeWithExternalMesher(TSharedRef<BodyData> DagNode, TSharedRef<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters);

	TOptional<FMeshDescription> ImportMesh(AlMesh& Mesh, TSharedRef<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters);

	FORCEINLINE bool IsTransparent(FColor& TransparencyColor)
	{
		float Opacity = 1.0f - ((float)(TransparencyColor.R + TransparencyColor.G + TransparencyColor.B)) / 765.0f;
		return !FMath::IsNearlyEqual(Opacity, 1.0f);
	}

	FORCEINLINE bool GetCommonParameters(AlShadingFields Field, double Value, FColor& Color, FColor& TransparencyColor, FColor& IncandescenceColor, double GlowIntensity)
	{
		switch (Field)
		{
		case AlShadingFields::kFLD_SHADING_COMMON_COLOR_R:
			Color.R = (uint8)Value;
			return true;
		case AlShadingFields::kFLD_SHADING_COMMON_COLOR_G:
			Color.G = (uint8)Value;
			return true;
		case  AlShadingFields::kFLD_SHADING_COMMON_COLOR_B:
			Color.B = (uint8)Value;
			return true;
		case AlShadingFields::kFLD_SHADING_COMMON_INCANDESCENCE_R:
			IncandescenceColor.R = (uint8)Value;
			return true;
		case AlShadingFields::kFLD_SHADING_COMMON_INCANDESCENCE_G:
			IncandescenceColor.G = (uint8)Value;
			return true;
		case AlShadingFields::kFLD_SHADING_COMMON_INCANDESCENCE_B:
			IncandescenceColor.B = (uint8)Value;
			return true;
		case  AlShadingFields::kFLD_SHADING_COMMON_TRANSPARENCY_R:
			TransparencyColor.R = (uint8)Value;
			return true;
		case  AlShadingFields::kFLD_SHADING_COMMON_TRANSPARENCY_G:
			TransparencyColor.G = (uint8)Value;
			return true;
		case  AlShadingFields::kFLD_SHADING_COMMON_TRANSPARENCY_B:
			TransparencyColor.B = (uint8)Value;
			return true;
		case AlShadingFields::kFLD_SHADING_COMMON_GLOW_INTENSITY:
			GlowIntensity = Value;
			return true;
		default:
			return false;
		}
	}

	void AddAlBlinnParameters(const TUniquePtr<AlShader>& Shader, TSharedRef<IDatasmithUEPbrMaterialElement> MaterialElement);
	void AddAlLambertParameters(const TUniquePtr<AlShader>& Shader, TSharedRef<IDatasmithUEPbrMaterialElement> MaterialElement);
	void AddAlLightSourceParameters(const TUniquePtr<AlShader>& Shader, TSharedRef<IDatasmithUEPbrMaterialElement> MaterialElement);
	void AddAlPhongParameters(const TUniquePtr<AlShader>& Shader, TSharedRef<IDatasmithUEPbrMaterialElement> MaterialElement);

private:
	TSharedRef<IDatasmithScene> DatasmithScene;
	FString SceneName;
	FString CurrentPath;
	FString OutputPath;
	FString SceneFullPath;

	// Hash value of the scene file used to check if the file has been modified for re-import
	uint32 SceneFileHash;

	TSharedPtr<AlDagNode> AlRootNode;

	/** Table of correspondence between mesh identifier and associated Datasmith mesh element */
	TMap<uint32, TSharedPtr<IDatasmithMeshElement>> ShellUuidToMeshElementMap;
	TMap<uint32, TSharedPtr<IDatasmithMeshElement>> BodyUuidToMeshElementMap;

	/** Datasmith mesh elements to OpenModel objects */
	TMap<IDatasmithMeshElement*, TSharedPtr<AlDagNode>> MeshElementToAlDagNodeMap;

	TMap<IDatasmithMeshElement*, TSharedPtr<BodyData>> MeshElementToBodyMap;

	TMap<IDatasmithMeshElement*, FString> MeshElementToShaderName;
	TMap<FString, FColor> ShaderNameToColor;
	TMap<FString, TSharedPtr<IDatasmithMaterialIDElement>> ShaderNameToUEMaterialId;

	TMap<FString, TSharedPtr<IDatasmithActorElement>> LayerNameToActor;

	FDatasmithTessellationOptions TessellationOptions;

	TSharedPtr<CADLibrary::ICADModelConverter> CADModelConverter;
	TSharedPtr<IAliasBRepConverter> AliasBRepConverter;

};

void FWireTranslatorImpl::SetTessellationOptions(const FDatasmithTessellationOptions& Options)
{
	TessellationOptions = Options;
	CADModelConverter->SetImportParameters(Options.ChordTolerance, Options.MaxEdgeLength, Options.NormalTolerance, (CADLibrary::EStitchingTechnique)Options.StitchingTechnique);
	SceneFileHash = HashCombine(Options.GetHash(), GetSceneFileHash(SceneFullPath, SceneName));
}

bool FWireTranslatorImpl::Read()
{
	// Initialize Alias.
	AlUniverse::initialize();

	if (AlUniverse::retrieve(TCHAR_TO_UTF8(*SceneFullPath)) != sSuccess)
	{
		return false;
	}

	AlRetrieveOptions options;
	AlUniverse::retrieveOptions(options);

	// Make material
	if (!GetShader())
	{
		return false;
	}

	// Parse and extract the DAG leaf nodes.
	// Note that Alias file unit is cm like UE
	return GetDagLeaves();
}

void FWireTranslatorImpl::AddAlBlinnParameters(const TUniquePtr<AlShader>& Shader, TSharedRef<IDatasmithUEPbrMaterialElement> MaterialElement)
{
	// Default values for a Blinn material
	FColor Color(145, 148, 153);
	FColor TransparencyColor(0, 0, 0);
	FColor IncandescenceColor(0, 0, 0);
	FColor SpecularColor(38, 38, 38);
	double Diffuse = 1.0;
	double GlowIntensity = 0.0;
	double Gloss = 0.8;
	double Eccentricity = 0.35;
	double Specularity = 1.0;
	double Reflectivity = 0.5;
	double SpecularRolloff = 0.5;

	AlList* List = Shader->fields();
	for (AlShadingFieldItem* Item = static_cast<AlShadingFieldItem*>(List->first()); Item; Item = Item->nextField())
	{
		double Value = 0.0f;
		statusCode ErrorCode = Shader->parameter(Item->field(), Value);
		if (ErrorCode != 0)
		{
			continue;
		}

		if (GetCommonParameters(Item->field(), Value, Color, TransparencyColor, IncandescenceColor, GlowIntensity))
		{
			continue;
		}

		switch (Item->field())
		{
		case AlShadingFields::kFLD_SHADING_BLINN_DIFFUSE:
			Diffuse = Value;
			break;
		case AlShadingFields::kFLD_SHADING_BLINN_GLOSS_:
			Gloss = Value;
			break;
		case AlShadingFields::kFLD_SHADING_BLINN_SPECULAR_R:
			SpecularColor.R = (uint8)(255.f * Value);
			break;
		case AlShadingFields::kFLD_SHADING_BLINN_SPECULAR_G:
			SpecularColor.G = (uint8)(255.f * Value);;
			break;
		case AlShadingFields::kFLD_SHADING_BLINN_SPECULAR_B:
			SpecularColor.B = (uint8)(255.f * Value);;
			break;
		case AlShadingFields::kFLD_SHADING_BLINN_SPECULARITY_:
			Specularity = Value;
			break;
		case AlShadingFields::kFLD_SHADING_BLINN_SPECULAR_ROLLOFF:
			SpecularRolloff = Value;
			break;
		case AlShadingFields::kFLD_SHADING_BLINN_ECCENTRICITY:
			Eccentricity = Value;
			break;
		case AlShadingFields::kFLD_SHADING_BLINN_REFLECTIVITY:
			Reflectivity = Value;
			break;
		}
	}

	bool bIsTransparent = IsTransparent(TransparencyColor);

	// Construct parameter expressions
	IDatasmithMaterialExpressionScalar* DiffuseExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	DiffuseExpression->GetScalar() = Diffuse;
	DiffuseExpression->SetName(TEXT("Diffuse"));

	IDatasmithMaterialExpressionScalar* GlossExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	GlossExpression->GetScalar() = Gloss;
	GlossExpression->SetName(TEXT("Gloss"));

	IDatasmithMaterialExpressionColor* SpecularColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
	SpecularColorExpression->SetName(TEXT("SpecularColor"));
	SpecularColorExpression->GetColor() = FLinearColor::FromSRGBColor(SpecularColor);

	IDatasmithMaterialExpressionScalar* SpecularityExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	SpecularityExpression->GetScalar() = Specularity * 0.3;
	SpecularityExpression->SetName(TEXT("Specularity"));

	IDatasmithMaterialExpressionScalar* SpecularRolloffExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	SpecularRolloffExpression->GetScalar() = SpecularRolloff;
	SpecularRolloffExpression->SetName(TEXT("SpecularRolloff"));

	IDatasmithMaterialExpressionScalar* EccentricityExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	EccentricityExpression->GetScalar() = Eccentricity;
	EccentricityExpression->SetName(TEXT("Eccentricity"));

	IDatasmithMaterialExpressionScalar* ReflectivityExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	ReflectivityExpression->GetScalar() = Reflectivity;
	ReflectivityExpression->SetName(TEXT("Reflectivity"));

	IDatasmithMaterialExpressionColor* ColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
	ColorExpression->SetName(TEXT("Color"));
	ColorExpression->GetColor() = FLinearColor::FromSRGBColor(Color);

	IDatasmithMaterialExpressionColor* IncandescenceColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
	IncandescenceColorExpression->SetName(TEXT("IncandescenceColor"));
	IncandescenceColorExpression->GetColor() = FLinearColor::FromSRGBColor(IncandescenceColor);

	IDatasmithMaterialExpressionColor* TransparencyColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
	TransparencyColorExpression->SetName(TEXT("TransparencyColor"));
	TransparencyColorExpression->GetColor() = FLinearColor::FromSRGBColor(TransparencyColor);

	IDatasmithMaterialExpressionScalar* GlowIntensityExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	GlowIntensityExpression->GetScalar() = GlowIntensity;
	GlowIntensityExpression->SetName(TEXT("GlowIntensity"));

	// Create aux expressions
	IDatasmithMaterialExpressionGeneric* ColorSpecLerp = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	ColorSpecLerp->SetExpressionName(TEXT("LinearInterpolate"));

	IDatasmithMaterialExpressionScalar* ColorSpecLerpValue = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	ColorSpecLerpValue->GetScalar() = 0.96f;

	IDatasmithMaterialExpressionGeneric* ColorMetallicLerp = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	ColorMetallicLerp->SetExpressionName(TEXT("LinearInterpolate"));

	IDatasmithMaterialExpressionGeneric* DiffuseLerp = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	DiffuseLerp->SetExpressionName(TEXT("LinearInterpolate"));

	IDatasmithMaterialExpressionScalar* DiffuseLerpA = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	DiffuseLerpA->GetScalar() = 0.04f;

	IDatasmithMaterialExpressionScalar* DiffuseLerpB = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	DiffuseLerpB->GetScalar() = 1.0f;

	IDatasmithMaterialExpressionGeneric* BaseColorMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	BaseColorMultiply->SetExpressionName(TEXT("Multiply"));

	IDatasmithMaterialExpressionGeneric* BaseColorAdd = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	BaseColorAdd->SetExpressionName(TEXT("Add"));

	IDatasmithMaterialExpressionGeneric* BaseColorTransparencyMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	BaseColorTransparencyMultiply->SetExpressionName(TEXT("Multiply"));

	IDatasmithMaterialExpressionGeneric* IncandescenceMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	IncandescenceMultiply->SetExpressionName(TEXT("Multiply"));

	IDatasmithMaterialExpressionGeneric* IncandescenceScaleMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	IncandescenceScaleMultiply->SetExpressionName(TEXT("Multiply"));

	IDatasmithMaterialExpressionScalar* IncandescenceScale = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	IncandescenceScale->GetScalar() = 100.0f;

	IDatasmithMaterialExpressionGeneric* EccentricityMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	EccentricityMultiply->SetExpressionName(TEXT("Multiply"));

	IDatasmithMaterialExpressionGeneric* EccentricityOneMinus = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	EccentricityOneMinus->SetExpressionName(TEXT("OneMinus"));

	IDatasmithMaterialExpressionGeneric* RoughnessOneMinus = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	RoughnessOneMinus->SetExpressionName(TEXT("OneMinus"));

	IDatasmithMaterialExpressionScalar* FresnelExponent = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	FresnelExponent->GetScalar() = 4.0f;

	IDatasmithMaterialExpressionFunctionCall* FresnelFunc = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionFunctionCall>();
	FresnelFunc->SetFunctionPathName(TEXT("/Engine/Functions/Engine_MaterialFunctions02/Fresnel_Function.Fresnel_Function"));

	IDatasmithMaterialExpressionGeneric* FresnelLerp = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	FresnelLerp->SetExpressionName(TEXT("LinearInterpolate"));

	IDatasmithMaterialExpressionScalar* FresnelLerpA = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	FresnelLerpA->GetScalar() = 1.0f;

	IDatasmithMaterialExpressionScalar* SpecularPowerExp = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	SpecularPowerExp->GetScalar() = 0.5f;

	IDatasmithMaterialExpressionGeneric* Power = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	Power->SetExpressionName(TEXT("Power"));

	IDatasmithMaterialExpressionGeneric* FresnelMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	FresnelMultiply->SetExpressionName(TEXT("Multiply"));

	IDatasmithMaterialExpressionGeneric* TransparencyOneMinus = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	TransparencyOneMinus->SetExpressionName(TEXT("OneMinus"));

	IDatasmithMaterialExpressionFunctionCall* BreakFloat3 = nullptr;
	IDatasmithMaterialExpressionGeneric* AddRG = nullptr;
	IDatasmithMaterialExpressionGeneric* AddRGB = nullptr;
	IDatasmithMaterialExpressionGeneric* Divide = nullptr;
	IDatasmithMaterialExpressionScalar* DivideConstant = nullptr;
	if (bIsTransparent)
	{
		BreakFloat3 = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionFunctionCall>();
		BreakFloat3->SetFunctionPathName(TEXT("/Engine/Functions/Engine_MaterialFunctions02/Utility/BreakFloat3Components.BreakFloat3Components"));

		AddRG = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		AddRG->SetExpressionName(TEXT("Add"));

		AddRGB = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		AddRGB->SetExpressionName(TEXT("Add"));

		Divide = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		Divide->SetExpressionName(TEXT("Divide"));

		DivideConstant = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		DivideConstant->GetScalar() = 3.0f;
	}

	// Connect expressions
	SpecularColorExpression->ConnectExpression(*ColorSpecLerp->GetInput(0));
	ColorExpression->ConnectExpression(*ColorSpecLerp->GetInput(1));
	ColorSpecLerpValue->ConnectExpression(*ColorSpecLerp->GetInput(2));

	ColorExpression->ConnectExpression(*ColorMetallicLerp->GetInput(0));
	ColorSpecLerp->ConnectExpression(*ColorMetallicLerp->GetInput(1));
	GlossExpression->ConnectExpression(*ColorMetallicLerp->GetInput(2));

	DiffuseLerpA->ConnectExpression(*DiffuseLerp->GetInput(0));
	DiffuseLerpB->ConnectExpression(*DiffuseLerp->GetInput(1));
	DiffuseExpression->ConnectExpression(*DiffuseLerp->GetInput(2));

	ColorMetallicLerp->ConnectExpression(*BaseColorMultiply->GetInput(0));
	DiffuseLerp->ConnectExpression(*BaseColorMultiply->GetInput(1));

	BaseColorMultiply->ConnectExpression(*BaseColorAdd->GetInput(0));
	IncandescenceColorExpression->ConnectExpression(*BaseColorAdd->GetInput(1));

	BaseColorAdd->ConnectExpression(*BaseColorTransparencyMultiply->GetInput(0));
	TransparencyOneMinus->ConnectExpression(*BaseColorTransparencyMultiply->GetInput(1));

	GlowIntensityExpression->ConnectExpression(*IncandescenceScaleMultiply->GetInput(0));
	IncandescenceScale->ConnectExpression(*IncandescenceScaleMultiply->GetInput(1));

	BaseColorTransparencyMultiply->ConnectExpression(*IncandescenceMultiply->GetInput(0));
	IncandescenceScaleMultiply->ConnectExpression(*IncandescenceMultiply->GetInput(1));

	EccentricityExpression->ConnectExpression(*EccentricityOneMinus->GetInput(0));

	EccentricityOneMinus->ConnectExpression(*EccentricityMultiply->GetInput(0));
	SpecularityExpression->ConnectExpression(*EccentricityMultiply->GetInput(1));

	EccentricityMultiply->ConnectExpression(*RoughnessOneMinus->GetInput(0));

	FresnelExponent->ConnectExpression(*FresnelFunc->GetInput(3));

	SpecularRolloffExpression->ConnectExpression(*Power->GetInput(0));
	SpecularPowerExp->ConnectExpression(*Power->GetInput(1));

	FresnelLerpA->ConnectExpression(*FresnelLerp->GetInput(0));
	FresnelFunc->ConnectExpression(*FresnelLerp->GetInput(1));
	Power->ConnectExpression(*FresnelLerp->GetInput(2));

	FresnelLerp->ConnectExpression(*FresnelMultiply->GetInput(0));
	ReflectivityExpression->ConnectExpression(*FresnelMultiply->GetInput(1));

	TransparencyColorExpression->ConnectExpression(*TransparencyOneMinus->GetInput(0));

	if (bIsTransparent)
	{
		TransparencyOneMinus->ConnectExpression(*BreakFloat3->GetInput(0));

		BreakFloat3->ConnectExpression(*AddRG->GetInput(0), 0);
		BreakFloat3->ConnectExpression(*AddRG->GetInput(1), 1);

		AddRG->ConnectExpression(*AddRGB->GetInput(0));
		BreakFloat3->ConnectExpression(*AddRGB->GetInput(1), 2);

		AddRGB->ConnectExpression(*Divide->GetInput(0));
		DivideConstant->ConnectExpression(*Divide->GetInput(1));
	}

	// Connect material outputs
	MaterialElement->GetBaseColor().SetExpression(BaseColorTransparencyMultiply);
	MaterialElement->GetMetallic().SetExpression(GlossExpression);
	MaterialElement->GetSpecular().SetExpression(FresnelMultiply);
	MaterialElement->GetRoughness().SetExpression(RoughnessOneMinus);
	MaterialElement->GetEmissiveColor().SetExpression(IncandescenceMultiply);

	if (bIsTransparent)
	{
		MaterialElement->GetOpacity().SetExpression(Divide);
		MaterialElement->SetParentLabel(TEXT("M_DatasmithAliasBlinnTransparent"));
	}
	else
	{
		MaterialElement->SetParentLabel(TEXT("M_DatasmithAliasBlinn"));
	}

}

void FWireTranslatorImpl::AddAlLambertParameters(const TUniquePtr<AlShader>& Shader, TSharedRef<IDatasmithUEPbrMaterialElement> MaterialElement)
{
	// Default values for a Lambert material
	FColor Color(145, 148, 153);
	FColor TransparencyColor(0, 0, 0);
	FColor IncandescenceColor(0, 0, 0);
	double Diffuse = 1.0;
	double GlowIntensity = 0.0;

	AlList* List = Shader->fields();
	for (AlShadingFieldItem* Item = static_cast<AlShadingFieldItem*>(List->first()); Item; Item = Item->nextField())
	{
		double Value = 0.0f;
		statusCode ErrorCode = Shader->parameter(Item->field(), Value);
		if (ErrorCode != 0)
		{
			continue;
		}

		if (GetCommonParameters(Item->field(), Value, Color, TransparencyColor, IncandescenceColor, GlowIntensity))
		{
			continue;
		}

		switch (Item->field())
		{
		case AlShadingFields::kFLD_SHADING_LAMBERT_DIFFUSE:
			Diffuse = Value;
			break;
		}
	}

	bool bIsTransparent = IsTransparent(TransparencyColor);

	// Construct parameter expressions
	IDatasmithMaterialExpressionScalar* DiffuseExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	DiffuseExpression->GetScalar() = Diffuse;
	DiffuseExpression->SetName(TEXT("Diffuse"));

	IDatasmithMaterialExpressionColor* ColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
	ColorExpression->SetName(TEXT("Color"));
	ColorExpression->GetColor() = FLinearColor::FromSRGBColor(Color);

	IDatasmithMaterialExpressionColor* IncandescenceColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
	IncandescenceColorExpression->SetName(TEXT("IncandescenceColor"));
	IncandescenceColorExpression->GetColor() = FLinearColor::FromSRGBColor(IncandescenceColor);

	IDatasmithMaterialExpressionColor* TransparencyColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
	TransparencyColorExpression->SetName(TEXT("TransparencyColor"));
	TransparencyColorExpression->GetColor() = FLinearColor::FromSRGBColor(TransparencyColor);

	IDatasmithMaterialExpressionScalar* GlowIntensityExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	GlowIntensityExpression->GetScalar() = GlowIntensity;
	GlowIntensityExpression->SetName(TEXT("GlowIntensity"));

	// Create aux expressions
	IDatasmithMaterialExpressionGeneric* DiffuseLerp = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	DiffuseLerp->SetExpressionName(TEXT("LinearInterpolate"));

	IDatasmithMaterialExpressionScalar* DiffuseLerpA = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	DiffuseLerpA->GetScalar() = 0.04f;

	IDatasmithMaterialExpressionScalar* DiffuseLerpB = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	DiffuseLerpB->GetScalar() = 1.0f;

	IDatasmithMaterialExpressionGeneric* BaseColorMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	BaseColorMultiply->SetExpressionName(TEXT("Multiply"));

	IDatasmithMaterialExpressionGeneric* BaseColorAdd = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	BaseColorAdd->SetExpressionName(TEXT("Add"));

	IDatasmithMaterialExpressionGeneric* BaseColorTransparencyMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	BaseColorTransparencyMultiply->SetExpressionName(TEXT("Multiply"));

	IDatasmithMaterialExpressionGeneric* IncandescenceMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	IncandescenceMultiply->SetExpressionName(TEXT("Multiply"));

	IDatasmithMaterialExpressionGeneric* IncandescenceScaleMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	IncandescenceScaleMultiply->SetExpressionName(TEXT("Multiply"));

	IDatasmithMaterialExpressionScalar* IncandescenceScale = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	IncandescenceScale->GetScalar() = 100.0f;

	IDatasmithMaterialExpressionGeneric* TransparencyOneMinus = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	TransparencyOneMinus->SetExpressionName(TEXT("OneMinus"));

	IDatasmithMaterialExpressionFunctionCall* BreakFloat3 = nullptr;
	IDatasmithMaterialExpressionGeneric* AddRG = nullptr;
	IDatasmithMaterialExpressionGeneric* AddRGB = nullptr;
	IDatasmithMaterialExpressionGeneric* Divide = nullptr;
	IDatasmithMaterialExpressionScalar* DivideConstant = nullptr;
	if (bIsTransparent)
	{
		BreakFloat3 = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionFunctionCall>();
		BreakFloat3->SetFunctionPathName(TEXT("/Engine/Functions/Engine_MaterialFunctions02/Utility/BreakFloat3Components.BreakFloat3Components"));

		AddRG = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		AddRG->SetExpressionName(TEXT("Add"));

		AddRGB = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		AddRGB->SetExpressionName(TEXT("Add"));

		Divide = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		Divide->SetExpressionName(TEXT("Divide"));

		DivideConstant = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		DivideConstant->GetScalar() = 3.0f;
	}

	// Connect expressions
	DiffuseLerpA->ConnectExpression(*DiffuseLerp->GetInput(0));
	DiffuseLerpB->ConnectExpression(*DiffuseLerp->GetInput(1));
	DiffuseExpression->ConnectExpression(*DiffuseLerp->GetInput(2));

	ColorExpression->ConnectExpression(*BaseColorMultiply->GetInput(0));
	DiffuseLerp->ConnectExpression(*BaseColorMultiply->GetInput(1));

	BaseColorMultiply->ConnectExpression(*BaseColorAdd->GetInput(0));
	IncandescenceColorExpression->ConnectExpression(*BaseColorAdd->GetInput(1));

	BaseColorAdd->ConnectExpression(*BaseColorTransparencyMultiply->GetInput(0));
	TransparencyOneMinus->ConnectExpression(*BaseColorTransparencyMultiply->GetInput(1));

	GlowIntensityExpression->ConnectExpression(*IncandescenceScaleMultiply->GetInput(0));
	IncandescenceScale->ConnectExpression(*IncandescenceScaleMultiply->GetInput(1));

	BaseColorTransparencyMultiply->ConnectExpression(*IncandescenceMultiply->GetInput(0));
	IncandescenceScaleMultiply->ConnectExpression(*IncandescenceMultiply->GetInput(1));

	TransparencyColorExpression->ConnectExpression(*TransparencyOneMinus->GetInput(0));

	if (bIsTransparent)
	{
		TransparencyOneMinus->ConnectExpression(*BreakFloat3->GetInput(0));

		BreakFloat3->ConnectExpression(*AddRG->GetInput(0), 0);
		BreakFloat3->ConnectExpression(*AddRG->GetInput(1), 1);

		AddRG->ConnectExpression(*AddRGB->GetInput(0));
		BreakFloat3->ConnectExpression(*AddRGB->GetInput(1), 2);

		AddRGB->ConnectExpression(*Divide->GetInput(0));
		DivideConstant->ConnectExpression(*Divide->GetInput(1));
	}

	// Connect material outputs
	MaterialElement->GetBaseColor().SetExpression(BaseColorTransparencyMultiply);
	MaterialElement->GetEmissiveColor().SetExpression(IncandescenceMultiply);
	if (bIsTransparent)
	{
		MaterialElement->GetOpacity().SetExpression(Divide);
		MaterialElement->SetParentLabel(TEXT("M_DatasmithAliasLambertTransparent"));
	}
	else {
		MaterialElement->SetParentLabel(TEXT("M_DatasmithAliasLambert"));
	}

}

void FWireTranslatorImpl::AddAlLightSourceParameters(const TUniquePtr<AlShader>& Shader, TSharedRef<IDatasmithUEPbrMaterialElement> MaterialElement)
{
	// Default values for a LightSource material
	FColor Color(145, 148, 153);
	FColor TransparencyColor(0, 0, 0);
	FColor IncandescenceColor(0, 0, 0);
	double GlowIntensity = 0.0;

	AlList* List = Shader->fields();
	for (AlShadingFieldItem* Item = static_cast<AlShadingFieldItem*>(List->first()); Item; Item = Item->nextField())
	{
		double Value = 0.0f;
		statusCode ErrorCode = Shader->parameter(Item->field(), Value);
		if (ErrorCode != 0)
		{
			continue;
		}

		GetCommonParameters(Item->field(), Value, Color, TransparencyColor, IncandescenceColor, GlowIntensity);
	}

	bool bIsTransparent = IsTransparent(TransparencyColor);

	// Construct parameter expressions
	IDatasmithMaterialExpressionColor* ColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
	ColorExpression->SetName(TEXT("Color"));
	ColorExpression->GetColor() = FLinearColor::FromSRGBColor(Color);

	IDatasmithMaterialExpressionColor* IncandescenceColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
	IncandescenceColorExpression->SetName(TEXT("IncandescenceColor"));
	IncandescenceColorExpression->GetColor() = FLinearColor::FromSRGBColor(IncandescenceColor);

	IDatasmithMaterialExpressionColor* TransparencyColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
	TransparencyColorExpression->SetName(TEXT("TransparencyColor"));
	TransparencyColorExpression->GetColor() = FLinearColor::FromSRGBColor(TransparencyColor);

	IDatasmithMaterialExpressionScalar* GlowIntensityExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	GlowIntensityExpression->GetScalar() = GlowIntensity;
	GlowIntensityExpression->SetName(TEXT("GlowIntensity"));

	// Create aux expressions
	IDatasmithMaterialExpressionGeneric* BaseColorAdd = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	BaseColorAdd->SetExpressionName(TEXT("Add"));

	IDatasmithMaterialExpressionGeneric* BaseColorTransparencyMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	BaseColorTransparencyMultiply->SetExpressionName(TEXT("Multiply"));

	IDatasmithMaterialExpressionGeneric* IncandescenceMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	IncandescenceMultiply->SetExpressionName(TEXT("Multiply"));

	IDatasmithMaterialExpressionGeneric* IncandescenceScaleMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	IncandescenceScaleMultiply->SetExpressionName(TEXT("Multiply"));

	IDatasmithMaterialExpressionScalar* IncandescenceScale = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	IncandescenceScale->GetScalar() = 100.0f;

	IDatasmithMaterialExpressionGeneric* TransparencyOneMinus = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	TransparencyOneMinus->SetExpressionName(TEXT("OneMinus"));

	IDatasmithMaterialExpressionFunctionCall* BreakFloat3 = nullptr;
	IDatasmithMaterialExpressionGeneric* AddRG = nullptr;
	IDatasmithMaterialExpressionGeneric* AddRGB = nullptr;
	IDatasmithMaterialExpressionGeneric* Divide = nullptr;
	IDatasmithMaterialExpressionScalar* DivideConstant = nullptr;
	if (bIsTransparent)
	{
		BreakFloat3 = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionFunctionCall>();
		BreakFloat3->SetFunctionPathName(TEXT("/Engine/Functions/Engine_MaterialFunctions02/Utility/BreakFloat3Components.BreakFloat3Components"));

		AddRG = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		AddRG->SetExpressionName(TEXT("Add"));

		AddRGB = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		AddRGB->SetExpressionName(TEXT("Add"));

		Divide = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		Divide->SetExpressionName(TEXT("Divide"));

		DivideConstant = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		DivideConstant->GetScalar() = 3.0f;
	}

	// Connect expressions
	ColorExpression->ConnectExpression(*BaseColorAdd->GetInput(0));
	IncandescenceColorExpression->ConnectExpression(*BaseColorAdd->GetInput(1));

	BaseColorAdd->ConnectExpression(*BaseColorTransparencyMultiply->GetInput(0));
	TransparencyOneMinus->ConnectExpression(*BaseColorTransparencyMultiply->GetInput(1));

	GlowIntensityExpression->ConnectExpression(*IncandescenceScaleMultiply->GetInput(0));
	IncandescenceScale->ConnectExpression(*IncandescenceScaleMultiply->GetInput(1));

	BaseColorTransparencyMultiply->ConnectExpression(*IncandescenceMultiply->GetInput(0));
	IncandescenceScaleMultiply->ConnectExpression(*IncandescenceMultiply->GetInput(1));

	TransparencyColorExpression->ConnectExpression(*TransparencyOneMinus->GetInput(0));

	if (bIsTransparent)
	{
		TransparencyOneMinus->ConnectExpression(*BreakFloat3->GetInput(0));

		BreakFloat3->ConnectExpression(*AddRG->GetInput(0), 0);
		BreakFloat3->ConnectExpression(*AddRG->GetInput(1), 1);

		AddRG->ConnectExpression(*AddRGB->GetInput(0));
		BreakFloat3->ConnectExpression(*AddRGB->GetInput(1), 2);

		AddRGB->ConnectExpression(*Divide->GetInput(0));
		DivideConstant->ConnectExpression(*Divide->GetInput(1));
	}

	// Connect material outputs
	MaterialElement->GetBaseColor().SetExpression(BaseColorTransparencyMultiply);
	MaterialElement->GetEmissiveColor().SetExpression(IncandescenceMultiply);

	if (bIsTransparent)
	{
		MaterialElement->GetOpacity().SetExpression(Divide);
		MaterialElement->SetParentLabel(TEXT("M_DatasmithAliasLightSourceTransparent"));
	}
	else {
		MaterialElement->SetParentLabel(TEXT("M_DatasmithAliasLightSource"));
	}

}

void FWireTranslatorImpl::AddAlPhongParameters(const TUniquePtr<AlShader>& Shader, TSharedRef<IDatasmithUEPbrMaterialElement> MaterialElement)
{
	// Default values for a Phong material
	FColor Color(145, 148, 153);
	FColor TransparencyColor(0, 0, 0);
	FColor IncandescenceColor(0, 0, 0);
	FColor SpecularColor(38, 38, 38);
	double Diffuse = 1.0;
	double GlowIntensity = 0.0;
	double Gloss = 0.8;
	double Shinyness = 20.0;
	double Specularity = 1.0;
	double Reflectivity = 0.5;

	AlList* List = Shader->fields();
	for (AlShadingFieldItem* Item = static_cast<AlShadingFieldItem*>(List->first()); Item; Item = Item->nextField())
	{
		double Value = 0.0f;
		statusCode ErrorCode = Shader->parameter(Item->field(), Value);
		if (ErrorCode != 0)
		{
			continue;
		}

		if (GetCommonParameters(Item->field(), Value, Color, TransparencyColor, IncandescenceColor, GlowIntensity))
		{
			continue;
		}

		switch (Item->field())
		{
		case AlShadingFields::kFLD_SHADING_PHONG_DIFFUSE:
			Diffuse = Value;
			break;
		case AlShadingFields::kFLD_SHADING_PHONG_GLOSS_:
			Gloss = Value;
			break;
		case AlShadingFields::kFLD_SHADING_PHONG_SPECULAR_R:
			SpecularColor.R = (uint8)(255.f * Value);;
			break;
		case AlShadingFields::kFLD_SHADING_PHONG_SPECULAR_G:
			SpecularColor.G = (uint8)(255.f * Value);;
			break;
		case AlShadingFields::kFLD_SHADING_PHONG_SPECULAR_B:
			SpecularColor.B = (uint8)(255.f * Value);;
			break;
		case AlShadingFields::kFLD_SHADING_PHONG_SPECULARITY_:
			Specularity = Value;
			break;
		case AlShadingFields::kFLD_SHADING_PHONG_SHINYNESS:
			Shinyness = Value;
			break;
		case AlShadingFields::kFLD_SHADING_PHONG_REFLECTIVITY:
			Reflectivity = Value;
			break;
		}
	}

	bool bIsTransparent = IsTransparent(TransparencyColor);

	// Construct parameter expressions
	IDatasmithMaterialExpressionScalar* DiffuseExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	DiffuseExpression->GetScalar() = Diffuse;
	DiffuseExpression->SetName(TEXT("Diffuse"));

	IDatasmithMaterialExpressionScalar* GlossExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	GlossExpression->GetScalar() = Gloss;
	GlossExpression->SetName(TEXT("Gloss"));

	IDatasmithMaterialExpressionColor* SpecularColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
	SpecularColorExpression->SetName(TEXT("SpecularColor"));
	SpecularColorExpression->GetColor() = FLinearColor::FromSRGBColor(SpecularColor);

	IDatasmithMaterialExpressionScalar* SpecularityExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	SpecularityExpression->GetScalar() = Specularity * 0.3;
	SpecularityExpression->SetName(TEXT("Specularity"));

	IDatasmithMaterialExpressionScalar* ShinynessExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	ShinynessExpression->GetScalar() = Shinyness;
	ShinynessExpression->SetName(TEXT("Shinyness"));

	IDatasmithMaterialExpressionScalar* ReflectivityExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	ReflectivityExpression->GetScalar() = Reflectivity;
	ReflectivityExpression->SetName(TEXT("Reflectivity"));

	IDatasmithMaterialExpressionColor* ColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
	ColorExpression->SetName(TEXT("Color"));
	ColorExpression->GetColor() = FLinearColor::FromSRGBColor(Color);

	IDatasmithMaterialExpressionColor* IncandescenceColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
	IncandescenceColorExpression->SetName(TEXT("IncandescenceColor"));
	IncandescenceColorExpression->GetColor() = FLinearColor::FromSRGBColor(IncandescenceColor);

	IDatasmithMaterialExpressionColor* TransparencyColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
	TransparencyColorExpression->SetName(TEXT("TransparencyColor"));
	TransparencyColorExpression->GetColor() = FLinearColor::FromSRGBColor(TransparencyColor);

	IDatasmithMaterialExpressionScalar* GlowIntensityExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	GlowIntensityExpression->GetScalar() = GlowIntensity;
	GlowIntensityExpression->SetName(TEXT("GlowIntensity"));

	// Create aux expressions
	IDatasmithMaterialExpressionGeneric* ColorSpecLerp = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	ColorSpecLerp->SetExpressionName(TEXT("LinearInterpolate"));

	IDatasmithMaterialExpressionScalar* ColorSpecLerpValue = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	ColorSpecLerpValue->GetScalar() = 0.96f;

	IDatasmithMaterialExpressionGeneric* ColorMetallicLerp = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	ColorMetallicLerp->SetExpressionName(TEXT("LinearInterpolate"));

	IDatasmithMaterialExpressionGeneric* DiffuseLerp = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	DiffuseLerp->SetExpressionName(TEXT("LinearInterpolate"));

	IDatasmithMaterialExpressionScalar* DiffuseLerpA = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	DiffuseLerpA->GetScalar() = 0.04f;

	IDatasmithMaterialExpressionScalar* DiffuseLerpB = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	DiffuseLerpB->GetScalar() = 1.0f;

	IDatasmithMaterialExpressionGeneric* BaseColorMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	BaseColorMultiply->SetExpressionName(TEXT("Multiply"));

	IDatasmithMaterialExpressionGeneric* BaseColorAdd = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	BaseColorAdd->SetExpressionName(TEXT("Add"));

	IDatasmithMaterialExpressionGeneric* BaseColorTransparencyMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	BaseColorTransparencyMultiply->SetExpressionName(TEXT("Multiply"));

	IDatasmithMaterialExpressionGeneric* IncandescenceMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	IncandescenceMultiply->SetExpressionName(TEXT("Multiply"));

	IDatasmithMaterialExpressionGeneric* IncandescenceScaleMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	IncandescenceScaleMultiply->SetExpressionName(TEXT("Multiply"));

	IDatasmithMaterialExpressionScalar* IncandescenceScale = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	IncandescenceScale->GetScalar() = 100.0f;

	IDatasmithMaterialExpressionGeneric* ShinynessSubtract = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	ShinynessSubtract->SetExpressionName(TEXT("Subtract"));

	IDatasmithMaterialExpressionScalar* ShinynessSubtract2 = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	ShinynessSubtract2->GetScalar() = 2.0f;

	IDatasmithMaterialExpressionGeneric* ShinynessDivide = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	ShinynessDivide->SetExpressionName(TEXT("Divide"));

	IDatasmithMaterialExpressionScalar* ShinynessDivide98 = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
	ShinynessDivide98->GetScalar() = 98.0f;

	IDatasmithMaterialExpressionGeneric* SpecularityMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	SpecularityMultiply->SetExpressionName(TEXT("Multiply"));

	IDatasmithMaterialExpressionGeneric* RoughnessOneMinus = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	RoughnessOneMinus->SetExpressionName(TEXT("OneMinus"));

	IDatasmithMaterialExpressionGeneric* TransparencyOneMinus = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
	TransparencyOneMinus->SetExpressionName(TEXT("OneMinus"));

	IDatasmithMaterialExpressionFunctionCall* BreakFloat3 = nullptr;
	IDatasmithMaterialExpressionGeneric* AddRG = nullptr;
	IDatasmithMaterialExpressionGeneric* AddRGB = nullptr;
	IDatasmithMaterialExpressionGeneric* Divide = nullptr;
	IDatasmithMaterialExpressionScalar* DivideConstant = nullptr;
	if (bIsTransparent)
	{
		BreakFloat3 = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionFunctionCall>();
		BreakFloat3->SetFunctionPathName(TEXT("/Engine/Functions/Engine_MaterialFunctions02/Utility/BreakFloat3Components.BreakFloat3Components"));

		AddRG = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		AddRG->SetExpressionName(TEXT("Add"));

		AddRGB = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		AddRGB->SetExpressionName(TEXT("Add"));

		Divide = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		Divide->SetExpressionName(TEXT("Divide"));

		DivideConstant = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		DivideConstant->GetScalar() = 3.0f;
	}

	// Connect expressions
	SpecularColorExpression->ConnectExpression(*ColorSpecLerp->GetInput(0));
	ColorExpression->ConnectExpression(*ColorSpecLerp->GetInput(1));
	ColorSpecLerpValue->ConnectExpression(*ColorSpecLerp->GetInput(2));

	ColorExpression->ConnectExpression(*ColorMetallicLerp->GetInput(0));
	ColorSpecLerp->ConnectExpression(*ColorMetallicLerp->GetInput(1));
	GlossExpression->ConnectExpression(*ColorMetallicLerp->GetInput(2));

	DiffuseLerpA->ConnectExpression(*DiffuseLerp->GetInput(0));
	DiffuseLerpB->ConnectExpression(*DiffuseLerp->GetInput(1));
	DiffuseExpression->ConnectExpression(*DiffuseLerp->GetInput(2));

	ColorMetallicLerp->ConnectExpression(*BaseColorMultiply->GetInput(0));
	DiffuseLerp->ConnectExpression(*BaseColorMultiply->GetInput(1));

	BaseColorMultiply->ConnectExpression(*BaseColorAdd->GetInput(0));
	IncandescenceColorExpression->ConnectExpression(*BaseColorAdd->GetInput(1));

	BaseColorAdd->ConnectExpression(*BaseColorTransparencyMultiply->GetInput(0));
	TransparencyOneMinus->ConnectExpression(*BaseColorTransparencyMultiply->GetInput(1));

	GlowIntensityExpression->ConnectExpression(*IncandescenceScaleMultiply->GetInput(0));
	IncandescenceScale->ConnectExpression(*IncandescenceScaleMultiply->GetInput(1));

	BaseColorTransparencyMultiply->ConnectExpression(*IncandescenceMultiply->GetInput(0));
	IncandescenceScaleMultiply->ConnectExpression(*IncandescenceMultiply->GetInput(1));

	ShinynessExpression->ConnectExpression(*ShinynessSubtract->GetInput(0));
	ShinynessSubtract2->ConnectExpression(*ShinynessSubtract->GetInput(1));

	ShinynessSubtract->ConnectExpression(*ShinynessDivide->GetInput(0));
	ShinynessDivide98->ConnectExpression(*ShinynessDivide->GetInput(1));

	ShinynessDivide->ConnectExpression(*SpecularityMultiply->GetInput(0));
	SpecularityExpression->ConnectExpression(*SpecularityMultiply->GetInput(1));

	SpecularityMultiply->ConnectExpression(*RoughnessOneMinus->GetInput(0));

	TransparencyColorExpression->ConnectExpression(*TransparencyOneMinus->GetInput(0));

	if (bIsTransparent)
	{
		TransparencyOneMinus->ConnectExpression(*BreakFloat3->GetInput(0));

		BreakFloat3->ConnectExpression(*AddRG->GetInput(0), 0);
		BreakFloat3->ConnectExpression(*AddRG->GetInput(1), 1);

		AddRG->ConnectExpression(*AddRGB->GetInput(0));
		BreakFloat3->ConnectExpression(*AddRGB->GetInput(1), 2);

		AddRGB->ConnectExpression(*Divide->GetInput(0));
		DivideConstant->ConnectExpression(*Divide->GetInput(1));
	}

	// Connect material outputs
	MaterialElement->GetBaseColor().SetExpression(BaseColorTransparencyMultiply);
	MaterialElement->GetMetallic().SetExpression(GlossExpression);
	MaterialElement->GetSpecular().SetExpression(ReflectivityExpression);
	MaterialElement->GetRoughness().SetExpression(RoughnessOneMinus);
	MaterialElement->GetEmissiveColor().SetExpression(IncandescenceMultiply);
	if (bIsTransparent)
	{
		MaterialElement->GetOpacity().SetExpression(Divide);
		MaterialElement->SetParentLabel(TEXT("M_DatasmithAliasPhongTransparent"));
	}
	else {
		MaterialElement->SetParentLabel(TEXT("M_DatasmithAliasPhong"));
	}
}

// Make material
bool FWireTranslatorImpl::GetShader()
{
	for (TUniquePtr<AlShader> Shader(AlUniverse::firstShader()); Shader.IsValid(); Shader = TUniquePtr<AlShader>(AlUniverse::nextShader(Shader.Get())))
	{
		const FString ShaderName = UTF8_TO_TCHAR(Shader->name());
		const FString ShaderModelName = Shader->shadingModel();

		const FColor Color = CreateShaderColorFromShaderName(ShaderName);
		ShaderNameToColor.Add(ShaderName, Color);

		const int32 ShaderId = CreateShaderId(Color);
		const FString MaterialElementName = FString::FromInt(ShaderId);

		TSharedRef<IDatasmithUEPbrMaterialElement> MaterialElement = FDatasmithSceneFactory::CreateUEPbrMaterial(*MaterialElementName);
		MaterialElement->SetLabel(*ShaderName);

		if (ShaderModelName.Equals(TEXT("BLINN")))
		{
			AddAlBlinnParameters(Shader, MaterialElement);
		}
		else if (ShaderModelName.Equals(TEXT("LAMBERT")))
		{
			AddAlLambertParameters(Shader, MaterialElement);
		}
		else if (ShaderModelName.Equals(TEXT("LIGHTSOURCE")))
		{
			AddAlLightSourceParameters(Shader, MaterialElement);
		}
		else if (ShaderModelName.Equals(TEXT("PHONG")))
		{
			AddAlPhongParameters(Shader, MaterialElement);
		}

		DatasmithScene->AddMaterial(MaterialElement);

		TSharedPtr<IDatasmithMaterialIDElement> MaterialIDElement = FDatasmithSceneFactory::CreateMaterialId(MaterialElement->GetName());
		ShaderNameToUEMaterialId.Add(*ShaderName, MaterialIDElement);
	}
	return true;
}

bool FWireTranslatorImpl::GetDagLeaves()
{
	FDagNodeInfo RootContainer;
	AlRootNode = TSharedPtr<AlDagNode>(AlUniverse::firstDagNode());
	if (AlRootNode.IsValid())
	{
		TArray<TSharedPtr<IDatasmithActorElement>> ActorsToRemove;
		if (RecurseDagForLeaves(AlRootNode, RootContainer))
		{
			// Delete empty branches
			for (int32 Index = DatasmithScene->GetActorsCount() - 1; Index >= 0; --Index)
			{
				TSharedPtr<IDatasmithActorElement> Actor = DatasmithScene->GetActor(Index);
				if (Actor.IsValid() && !Actor->IsA(EDatasmithElementType::StaticMeshActor))
				{
					RecurseDeleteEmptyActor(Actor);
					if (Actor->GetChildrenCount() == 0)
					{
						ActorsToRemove.Add(Actor);
					}
				}
			}
			for (const TSharedPtr<IDatasmithActorElement>& Actor : ActorsToRemove)
			{
				DatasmithScene->RemoveActor(Actor, EDatasmithActorRemovalRule::RemoveChildren);
			}
			return true;
		}
	}

	return false;
}

void FWireTranslatorImpl::RecurseDeleteEmptyActor(TSharedPtr<IDatasmithActorElement> Actor)
{
	for (int32 Index = Actor->GetChildrenCount() - 1; Index >= 0; --Index)
	{
		TSharedPtr<IDatasmithActorElement> ActorChild = Actor->GetChild(Index);
		if (ActorChild.IsValid() && !ActorChild->IsA(EDatasmithElementType::StaticMeshActor))
		{
			RecurseDeleteEmptyActor(ActorChild);
			if (ActorChild->GetChildrenCount() == 0)
			{
				Actor->RemoveChild(ActorChild);
			}
		}
	}
}


void FWireTranslatorImpl::FDagNodeInfo::ExtractDagNodeLayer(const AlDagNode& InDagNode)
{
	AlLayer* LayerPtr = InDagNode.layer();
	if (LayerPtr)
	{
		TUniquePtr<AlLayer> Layer(LayerPtr);
		LayerName = UTF8_TO_TCHAR(Layer->name());
		bLayerIsVisible = !Layer->invisible();
		if (ActorElement.IsValid())
		{
			ActorElement->SetLayer(*LayerName);
			LayerToActorElement.Add(LayerName, ActorElement);
		}
	}
}

void FWireTranslatorImpl::FDagNodeInfo::ExtractDagNodeInfo(AlDagNode& CurrentNode)
{
	Label = UTF8_TO_TCHAR(CurrentNode.name());
	uint32 ThisGroupNodeUuid = OpenModelUtils::GetAlDagNodeUuid(CurrentNode);
	Uuid = HashCombine(ParentInfo ? ParentInfo->Uuid : 1, ThisGroupNodeUuid);
}

void FWireTranslatorImpl::FDagNodeInfo::ExtractDagNodeInfo(BodyData& CurrentNode)
{
	if (!ParentInfo)
	{
		return;
	}
	Label = ParentInfo->Label;
	CurrentNode.SetLabel(ParentInfo->Label);
	Uuid = CurrentNode.GetUuid(ParentInfo->Uuid);
}

TSharedPtr<IDatasmithActorElement> FWireTranslatorImpl::FindOrAddLayerActor(const FString& LayerName)
{
	if (!bGLayersAsActors || LayerName.IsEmpty())
	{
		return TSharedPtr<IDatasmithActorElement>();
	}

	TSharedPtr<IDatasmithActorElement>* LayerActor = LayerNameToActor.Find(LayerName);
	if (LayerActor)
	{
		return *LayerActor;
	}

	uint32 LayerUuid = GetTypeHash(LayerName);
	TSharedRef<IDatasmithActorElement> NewLayerActor = FDatasmithSceneFactory::CreateActor(*OpenModelUtils::UuidToString(LayerUuid));
	NewLayerActor->SetLabel(*LayerName);
	NewLayerActor->SetLayer(*LayerName);
	DatasmithScene->AddActor(NewLayerActor);
	LayerNameToActor.Add(LayerName, NewLayerActor);
	return NewLayerActor;
}

TSharedPtr<IDatasmithActorElement> FWireTranslatorImpl::FindOrAddParentActor(FDagNodeInfo& ParentInfo, const FString& LayerName)
{
	if (!ParentInfo.ActorElement.IsValid())
	{
		if (!LayerName.IsEmpty())
		{
			return FindOrAddLayerActor(LayerName);
		}
		return TSharedPtr<IDatasmithActorElement>();
	}

	if (!bGLayersAsActors)
	{
		return ParentInfo.ActorElement;
	}

	TSharedPtr<IDatasmithActorElement>* ActorParent = ParentInfo.LayerToActorElement.Find(LayerName);
	if (ActorParent)
	{
		return *ActorParent;
	}

	uint32 LayerActorUuid = HashCombine(ParentInfo.Uuid, GetTypeHash(LayerName));

	TSharedPtr<IDatasmithActorElement> LayerActorParent = FDatasmithSceneFactory::CreateActor(*OpenModelUtils::UuidToString(LayerActorUuid));
	LayerActorParent->SetLabel(*ParentInfo.Label);
	LayerActorParent->SetLayer(*LayerName);

	ParentInfo.LayerToActorElement.Add(LayerName, LayerActorParent);

	TSharedPtr<IDatasmithActorElement> GrandParentActor = FindOrAddParentActor(*ParentInfo.ParentInfo, LayerName);
	if (GrandParentActor.IsValid())
	{
		GrandParentActor->AddChild(LayerActorParent);
	}
	else
	{
		TSharedPtr<IDatasmithActorElement> LayerActor = FindOrAddLayerActor(LayerName);
		LayerActor->AddChild(LayerActorParent);
	}

	return LayerActorParent;
}

bool FWireTranslatorImpl::ProcessAlGroupNode(AlDagNode& GroupNode, FDagNodeInfo& ParentInfo)
{
	AlGroupNode* AlGroup = GroupNode.asGroupNodePtr();
	if (!AlIsValid(AlGroup))
	{
		return false;
	}

	AlDagNode* AlChildPtr = AlGroup->childNode();
	if (!AlIsValid(AlChildPtr))
	{
		return false;
	}

	TSharedPtr<AlDagNode> ChildNode(AlChildPtr);

	FDagNodeInfo ThisGroupNodeInfo(ParentInfo);
	ThisGroupNodeInfo.ExtractDagNodeInfo(GroupNode);

	ThisGroupNodeInfo.ActorElement = FDatasmithSceneFactory::CreateActor(*OpenModelUtils::UuidToString(ThisGroupNodeInfo.Uuid));
	ThisGroupNodeInfo.ActorElement->SetLabel(*ThisGroupNodeInfo.Label);
	ThisGroupNodeInfo.ExtractDagNodeLayer(GroupNode);

	// Apply local transform to actor element
	OpenModelUtils::SetActorTransform(ThisGroupNodeInfo.ActorElement, GroupNode);

	TSharedPtr<IDatasmithActorElement> ParentActorElement = FindOrAddParentActor(ParentInfo, ThisGroupNodeInfo.LayerName);
	if (ParentActorElement.IsValid())
	{
		ParentActorElement->AddChild(ThisGroupNodeInfo.ActorElement);
	}
	else
	{
		DatasmithScene->AddActor(ThisGroupNodeInfo.ActorElement);
	}

	RecurseDagForLeaves(ChildNode, ThisGroupNodeInfo);

	return true;
}

TSharedPtr<IDatasmithMeshElement> FWireTranslatorImpl::FindOrAddMeshElement(const TSharedPtr<BodyData>& Body, const FDagNodeInfo& NodeInfo)
{
	TSharedPtr<IDatasmithMeshElement>* MeshElementPtr = BodyUuidToMeshElementMap.Find(NodeInfo.Uuid);
	if (MeshElementPtr != nullptr)
	{
		return *MeshElementPtr;
	}

	TSharedPtr<IDatasmithMeshElement> MeshElement = FDatasmithSceneFactory::CreateMesh(*OpenModelUtils::UuidToString(NodeInfo.Uuid));
	MeshElement->SetLabel(*NodeInfo.Label);
	MeshElement->SetLightmapSourceUV(-1);

	const TSet<FString>& ShaderNames = Body->GetShaderNames();
	for (const FString& ShaderName : ShaderNames)
	{
		TSharedPtr<IDatasmithMaterialIDElement> MaterialElement = ShaderNameToUEMaterialId[ShaderName];
		if (MaterialElement.IsValid())
		{
			const FColor* ShaderColor = ShaderNameToColor.Find(ShaderName);
			if (ShaderColor)
			{
				int32 ShaderId = CreateShaderId(*ShaderColor);
				MeshElement->SetMaterial(MaterialElement->GetName(), ShaderId);
			}
		}
	}

	DatasmithScene->AddMesh(MeshElement);

	ShellUuidToMeshElementMap.Add(NodeInfo.Uuid, MeshElement);
	MeshElementToBodyMap.Add(MeshElement.Get(), Body);

	BodyUuidToMeshElementMap.Add(NodeInfo.Uuid, MeshElement);

	return MeshElement;
}

TSharedPtr<IDatasmithMeshElement> FWireTranslatorImpl::FindOrAddMeshElement(const TSharedPtr<AlDagNode>& ShellNode, const FDagNodeInfo& ShellNodeInfo, const FString& ShaderName)
{
	uint32 ShellUuid = OpenModelUtils::GetAlDagNodeUuid(*ShellNode);

	// Look if geometry has not been already processed, return it if found
	TSharedPtr<IDatasmithMeshElement>* MeshElementPtr = ShellUuidToMeshElementMap.Find(ShellUuid);
	if (MeshElementPtr != nullptr)
	{
		return *MeshElementPtr;
	}

	TSharedPtr<IDatasmithMeshElement> MeshElement = FDatasmithSceneFactory::CreateMesh(*OpenModelUtils::UuidToString(ShellNodeInfo.Uuid));
	MeshElement->SetLabel(*ShellNodeInfo.Label);
	MeshElement->SetLightmapSourceUV(-1);

	// Set MeshElement FileHash used for re-import task
	FMD5 MD5; // unique Value that define the mesh
	MD5.Update(reinterpret_cast<const uint8*>(&SceneFileHash), sizeof SceneFileHash);
	// MeshActor Name
	MD5.Update(reinterpret_cast<const uint8*>(&ShellUuid), sizeof ShellUuid);
	FMD5Hash Hash;
	Hash.Set(MD5);
	MeshElement->SetFileHash(Hash);

	if (ShaderName.Len())
	{
		TSharedPtr<IDatasmithMaterialIDElement> MaterialElement = ShaderNameToUEMaterialId[ShaderName];
		if (MaterialElement.IsValid())
		{
			const FColor* ShaderColor = ShaderNameToColor.Find(ShaderName);
			if (ShaderColor)
			{
				int32 ShaderId = CreateShaderId(*ShaderColor);
				MeshElement->SetMaterial(MaterialElement->GetName(), ShaderId);
			}
		}

	}

	DatasmithScene->AddMesh(MeshElement);

	ShellUuidToMeshElementMap.Add(ShellUuid, MeshElement);
	MeshElementToAlDagNodeMap.Add(MeshElement.Get(), ShellNode);
	MeshElementToShaderName.Add(MeshElement.Get(), ShaderName);

	return MeshElement;
}

void FWireTranslatorImpl::ProcessAlShellNode(const TSharedPtr<AlDagNode>& ShellNode, FDagNodeInfo& ParentInfo, const FString& ShaderName)
{
	FDagNodeInfo ShellInfo(ParentInfo);
	ShellInfo.ExtractDagNodeInfo(*ShellNode);

	TSharedPtr<IDatasmithMeshElement> MeshElement = FindOrAddMeshElement(ShellNode, ShellInfo, ShaderName);
	if (!MeshElement.IsValid())
	{
		return;
	}

	TSharedPtr<IDatasmithMeshActorElement> ActorElement = FDatasmithSceneFactory::CreateMeshActor(*OpenModelUtils::UuidToString(ShellInfo.Uuid));
	if (!ActorElement.IsValid())
	{
		return;
	}

	ActorElement->SetLabel(*ShellInfo.Label);
	ActorElement->SetStaticMeshPathName(MeshElement->GetName());
	ShellInfo.ActorElement = ActorElement;

	ShellInfo.ExtractDagNodeLayer(*ShellNode);
	if (!ShellInfo.bLayerIsVisible)
	{
		return;
	}

	OpenModelUtils::SetActorTransform(ShellInfo.ActorElement, *ShellNode);

	// Apply materials on the current part
	if (ShaderName.Len())
	{
		const TSharedPtr<IDatasmithMaterialIDElement>& MaterialIDElement = ShaderNameToUEMaterialId[ShaderName];
		if (MaterialIDElement.IsValid())
		{
			for (int32 Index = 0; Index < MeshElement->GetMaterialSlotCount(); ++Index)
			{
				MaterialIDElement->SetId(MeshElement->GetMaterialSlotAt(Index)->GetId());
				ActorElement->AddMaterialOverride(MaterialIDElement);
			}
		}
	}

	TSharedPtr<IDatasmithActorElement> ParentElement = FindOrAddParentActor(ParentInfo, ShellInfo.LayerName);
	if (ParentElement.IsValid())
	{
		ParentElement->AddChild(ActorElement);
	}
	else
	{
		DatasmithScene->AddActor(ActorElement);
	}
}

void FWireTranslatorImpl::ProcessBodyNode(const TSharedPtr<BodyData>& Body, FDagNodeInfo& ParentInfo)
{

	if (!Body.IsValid())
	{
		return;
	}

	if (Body->GetShells().Num() == 1)
	{
		DagForLeavesNoMerge(Body->GetShells()[0].Key, ParentInfo);
		return;
	}

	FDagNodeInfo ShellInfo(ParentInfo);
	ShellInfo.ExtractDagNodeInfo(*Body);

	TSharedPtr<IDatasmithMeshElement> MeshElement = FindOrAddMeshElement(Body, ShellInfo);
	if (!MeshElement.IsValid())
	{
		return;
	}

	TSharedPtr<IDatasmithMeshActorElement> ActorElement = FDatasmithSceneFactory::CreateMeshActor(*OpenModelUtils::UuidToString(ShellInfo.Uuid));
	if (!ActorElement.IsValid())
	{
		return;
	}

	ActorElement->SetLabel(ShellInfo.Label.IsEmpty() ? *Body->GetLayerName() : *ShellInfo.Label);
	ActorElement->SetStaticMeshPathName(MeshElement->GetName());
	ShellInfo.ActorElement = ActorElement;

	ActorElement->SetLayer(*Body->GetLayerName());

	// Apply materials on the current part
	int32 Index = 0;
	const TSet<FString>& ShaderNames = Body->GetShaderNames();
	for (const FString& ShaderName : ShaderNames)
	{
		TSharedPtr<IDatasmithMaterialIDElement> MaterialIDElement = ShaderNameToUEMaterialId[ShaderName];
		if (MaterialIDElement.IsValid())
		{
			MaterialIDElement->SetId(MeshElement->GetMaterialSlotAt(Index)->GetId());
			ActorElement->AddMaterialOverride(MaterialIDElement);
			Index++;
		}
	}

	TSharedPtr<IDatasmithActorElement> ParentElement = FindOrAddParentActor(ParentInfo, Body->GetLayerName());
	if (ParentElement.IsValid())
	{
		ParentElement->AddChild(ActorElement);
	}
	else
	{
		DatasmithScene->AddActor(ActorElement);
	}

	return;
}

TSharedPtr<AlDagNode> GetNextNode(const TSharedPtr<AlDagNode>& DagNode)
{
	// Grab the next sibling before deleting the node.
	AlDagNode* SiblingNode = DagNode->nextNode();
	if (AlIsValid(SiblingNode))
	{
		return TSharedPtr<AlDagNode>(SiblingNode);
	}
	else
	{
		return TSharedPtr<AlDagNode>();
	}
}

uint32 GetBodyGroupUuid(const FString& ShaderName, const FString& LayerName, bool bCadData)
{
	uint32 Uuid = HashCombine(GetTypeHash(LayerName), GetTypeHash(bCadData));
	if (bGSewByMaterial)
	{
		Uuid = HashCombine(GetTypeHash(ShaderName), Uuid);
	}
	return Uuid;
}

uint32 GetPatchCount(const TUniquePtr<AlShell>& Shell)
{
	uint32 PatchCount = 0;
	TUniquePtr<AlTrimRegion> TrimRegion(Shell->firstTrimRegion());
	while (TrimRegion.IsValid())
	{
		PatchCount++;
		TrimRegion = TUniquePtr<AlTrimRegion>(TrimRegion->nextRegion());
	}
	return PatchCount;
}

void FWireTranslatorImpl::AddNodeInBodyGroup(TSharedPtr<AlDagNode>& DagNode, const FString& ShaderName, TMap<uint32, TSharedPtr<BodyData>>& ShellToProcess, bool bIsAPatch, uint32 MaxSize)
{
	FString LayerName;

	AlLayer* LayerPtr = DagNode->layer();
	if (AlIsValid(LayerPtr))
	{
		const TUniquePtr<AlLayer> Layer(LayerPtr);
		LayerName = UTF8_TO_TCHAR(Layer->name());
		if (Layer->invisible())
		{
			return;
		}
	}

	uint32 SetId = GetBodyGroupUuid(ShaderName, LayerName, bIsAPatch);

	TSharedPtr<BodyData> Body;
	if (TSharedPtr<BodyData>* PBody = ShellToProcess.Find(SetId))
	{
		Body = *PBody;
	}
	else
	{
		TSharedRef<BodyData> BodyRef = MakeShared<BodyData>(LayerName, bIsAPatch);
		ShellToProcess.Add(SetId, BodyRef);
		BodyRef->Reserve(MaxSize);
		Body = BodyRef;
	}

	const FColor& Color = ShaderNameToColor[ShaderName];
	Body->Add(DagNode, Color, ShaderName);
}

bool FWireTranslatorImpl::RecurseDagForLeaves(const TSharedPtr<AlDagNode>& FirstDagNode, FDagNodeInfo& ParentInfo)
{
	if (TessellationOptions.StitchingTechnique != EDatasmithCADStitchingTechnique::StitchingSew)
	{
		RecurseDagForLeavesNoMerge(FirstDagNode, ParentInfo);
		return true;
	}

	TSharedPtr<AlDagNode> DagNode = FirstDagNode;
	uint32 MaxSize = 0;
	while (DagNode)
	{
		MaxSize++;
		DagNode = GetNextNode(DagNode);
	}

	DagNode = FirstDagNode;

	TMap<uint32, TSharedPtr<BodyData>> ShellToProcess;

	const char* ShaderName = nullptr;

	while (DagNode)
	{
		AlObjectType objectType = DagNode->type();

		// Process the current node.
		switch (objectType)
		{
			// Push all leaf nodes into 'leaves'
		case kShellNodeType:
		{
			AlShellNode* ShellNode = DagNode->asShellNodePtr();
			if (!AlIsValid(ShellNode))
			{
				break;
			}

			AlShell* ShellPtr = ShellNode->shell();
			if (!AlIsValid(ShellPtr))
			{
				break;
			}

			TUniquePtr<AlShell> Shell(ShellPtr);
			uint32 NbPatch = GetPatchCount(Shell);

			TUniquePtr<AlShader> Shader(Shell->firstShader());
			if (Shader.IsValid())
			{
				ShaderName = Shader->name();
			}

			if (NbPatch == 1)
			{
				AddNodeInBodyGroup(DagNode, ShaderName, ShellToProcess, true, MaxSize);
			}
			else
			{
				ProcessAlShellNode(DagNode, ParentInfo, ShaderName);
			}
			break;
		}

		case kSurfaceNodeType:
		{
			AlSurfaceNode* SurfaceNode = DagNode->asSurfaceNodePtr();
			TUniquePtr<AlSurface> Surface(SurfaceNode->surface());
			if (Surface.IsValid())
			{
				TUniquePtr<AlShader> Shader(Surface->firstShader());
				if (Shader.IsValid())
				{
					ShaderName = Shader->name();
				}
			}
			AddNodeInBodyGroup(DagNode, ShaderName, ShellToProcess, true, MaxSize);
			break;
		}

		case kMeshNodeType:
		{
			AlMeshNode* MeshNode = DagNode->asMeshNodePtr();
			TUniquePtr<AlMesh> Mesh(MeshNode->mesh());
			if (Mesh.IsValid())
			{
				TUniquePtr<AlShader> Shader(Mesh->firstShader());
				if (Shader.IsValid())
				{
					ShaderName = Shader->name();
				}
			}
			AddNodeInBodyGroup(DagNode, ShaderName, ShellToProcess, false, MaxSize);
			break;
		}

		// Traverse down through groups
		case kGroupNodeType:
		{
			ProcessAlGroupNode(*DagNode, ParentInfo);
			break;
		}

		default:
		{
			break;
		}
		}

		DagNode = GetNextNode(DagNode);
	}

	for (const TPair<uint32, TSharedPtr<BodyData>>& Body : ShellToProcess)
	{
		ProcessBodyNode(Body.Value, ParentInfo);
	}
	return true;
}

void FWireTranslatorImpl::RecurseDagForLeavesNoMerge(const TSharedPtr<AlDagNode>& FirstDagNode, FDagNodeInfo& ParentInfo)
{
	TSharedPtr<AlDagNode> DagNode = FirstDagNode;
	while (DagNode)
	{
		DagForLeavesNoMerge(DagNode, ParentInfo);
		DagNode = GetNextNode(DagNode);
	}
}

void FWireTranslatorImpl::DagForLeavesNoMerge(const TSharedPtr<AlDagNode>& DagNode, FDagNodeInfo& ParentInfo)
{
	const char* ShaderName = nullptr;

	// Process the current node.
	AlObjectType objectType = DagNode->type();
	switch (objectType)
	{
		// Push all leaf nodes into 'leaves'
	case kShellNodeType:
	{
		AlShellNode* ShellNode = DagNode->asShellNodePtr();
		TUniquePtr<AlShell> Shell(ShellNode->shell());
		if (Shell.IsValid())
		{
			TUniquePtr<AlShader> Shader(Shell->firstShader());
			if (Shader.IsValid())
			{
				ShaderName = Shader->name();
			}

			ProcessAlShellNode(DagNode, ParentInfo, ShaderName);
		}
		break;
	}
	case kSurfaceNodeType:
	{
		AlSurfaceNode* SurfaceNode = DagNode->asSurfaceNodePtr();
		TUniquePtr<AlSurface> Surface(SurfaceNode->surface());
		if (Surface.IsValid())
		{
			TUniquePtr<AlShader> Shader(Surface->firstShader());
			if (Shader.IsValid())
			{
				ShaderName = Shader->name();
			}
		}
		ProcessAlShellNode(DagNode, ParentInfo, ShaderName);
		break;
	}

	case kMeshNodeType:
	{
		AlMeshNode* MeshNode = DagNode->asMeshNodePtr();
		TUniquePtr<AlMesh> Mesh(MeshNode->mesh());
		if (Mesh.IsValid())
		{
			TUniquePtr<AlShader> Shader(Mesh->firstShader());
			if (Shader.IsValid())
			{
				ShaderName = Shader->name();
			}
		}
		ProcessAlShellNode(DagNode, ParentInfo, ShaderName);
		break;
	}

	// Traverse down through groups
	case kGroupNodeType:
	{
		ProcessAlGroupNode(*DagNode, ParentInfo);
		break;
	}

	default:
	{
		break;
	}

	}
}

TOptional<FMeshDescription> FWireTranslatorImpl::MeshDagNodeWithExternalMesher(AlDagNode& DagNode, TSharedRef<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters)
{
	// Wire unit is cm
	CADModelConverter->InitializeProcess();

	EAliasObjectReference ObjectReference = EAliasObjectReference::LocalReference;

	if (MeshParameters.bIsSymmetric)
	{
		// All actors of a Alias symmetric layer are defined in the world Reference i.e. they have identity transform. So Mesh actor has to be defined in the world reference.
		ObjectReference = EAliasObjectReference::WorldReference;
	}

	const FColor* ColorPtr = nullptr;
	const FString* ShaderName = MeshElementToShaderName.Find(&MeshElement.Get());
	if (ShaderName)
	{
		ColorPtr = ShaderNameToColor.Find(*ShaderName);
	}
	if (!ColorPtr) // Should never happen
	{
		ColorPtr = &DefaultColor;
	}

	AliasBRepConverter->AddBRep(DagNode, *ColorPtr, ObjectReference);

	CADModelConverter->RepairTopology();

	CADModelConverter->SaveModel(*OutputPath, MeshElement);

	FMeshDescription MeshDescription;
	DatasmithMeshHelper::PrepareAttributeForStaticMesh(MeshDescription);

	bool bRet = CADModelConverter->Tessellate(MeshParameters, MeshDescription);
	if (!bRet)
	{
		const TCHAR* StaticMeshLable = MeshElement->GetLabel();
		const TCHAR* StaticMeshName = MeshElement->GetName();
		UE_LOG(LogDatasmithWireTranslator, Warning, TEXT("Failed to generate the mesh of \"%s\" (%s) StaticMesh."), *StaticMeshLable, *StaticMeshName);
	}

	return MoveTemp(MeshDescription);
}

TOptional<FMeshDescription> FWireTranslatorImpl::MeshDagNodeWithExternalMesher(TSharedRef<BodyData> Body, TSharedRef<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters)
{
	// Wire unit is cm
	CADModelConverter->InitializeProcess();

	EAliasObjectReference ObjectReference = EAliasObjectReference::LocalReference;
	if (MeshParameters.bIsSymmetric)
	{
		// All actors of a Alias symmetric layer are defined in the world Reference i.e. they have identity transform. So Mesh actor has to be defined in the world reference.
		ObjectReference = EAliasObjectReference::WorldReference;
	}
	else if (TessellationOptions.StitchingTechnique == EDatasmithCADStitchingTechnique::StitchingSew)
	{
		// In the case of StitchingSew, AlDagNode children of a GroupNode are merged together. To be merged, they have to be defined in the reference of parent GroupNode.
		ObjectReference = EAliasObjectReference::ParentReference;
	}

	for (const TPair<TSharedPtr<AlDagNode>, FColor>& DagNode : Body->GetShells())
	{
		AliasBRepConverter->AddBRep(*DagNode.Key, DagNode.Value, ObjectReference);
	}

	CADModelConverter->RepairTopology();

	CADModelConverter->SaveModel(*OutputPath, MeshElement);

	FMeshDescription MeshDescription;
	DatasmithMeshHelper::PrepareAttributeForStaticMesh(MeshDescription);

	CADModelConverter->Tessellate(MeshParameters, MeshDescription);

	return MoveTemp(MeshDescription);
}

TOptional<FMeshDescription> FWireTranslatorImpl::GetMeshOfShellNode(AlDagNode& DagNode, TSharedRef<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters)
{
	if (CADModelConverter->IsSessionValid())
	{
		TOptional<FMeshDescription> UEMesh = MeshDagNodeWithExternalMesher(DagNode, MeshElement, MeshParameters);
		return UEMesh;
	}
	else
	{
		AlMatrix4x4 AlMatrix;
		DagNode.inverseGlobalTransformationMatrix(AlMatrix);
		// TODO: the best way, should be to don't have to apply inverse global transform to the generated mesh
		TSharedPtr<AlDagNode> TesselatedNode = OpenModelUtils::TesselateDagLeaf(DagNode, ETesselatorType::Fast, TessellationOptions.ChordTolerance);
		if (TesselatedNode.IsValid())
		{
			// Get the meshes from the dag nodes. Note that removing the mesh's DAG.
			// will also removes the meshes, so we have to do it later.
			TOptional<FMeshDescription> UEMesh = GetMeshOfNodeMesh(*TesselatedNode, MeshElement, MeshParameters, &AlMatrix);
			return UEMesh;
		}
	}
	return TOptional<FMeshDescription>();
}

TOptional<FMeshDescription> FWireTranslatorImpl::GetMeshOfShellBody(TSharedRef<BodyData> Body, TSharedRef<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters)
{
	TOptional<FMeshDescription> UEMesh = MeshDagNodeWithExternalMesher(Body, MeshElement, MeshParameters);
	return UEMesh;
}

TOptional<FMeshDescription> FWireTranslatorImpl::GetMeshOfMeshBody(TSharedRef<BodyData> Body, TSharedRef<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters)
{
	FMeshDescription MeshDescription;
	DatasmithMeshHelper::PrepareAttributeForStaticMesh(MeshDescription);
	MeshDescription.Empty();

	for (const TPair<TSharedPtr<AlDagNode>, FColor>& DagNode : Body->GetShells())
	{
		AlMeshNode* MeshNode = DagNode.Key->asMeshNodePtr();
		if (!AlIsValid(MeshNode))
		{
			continue;
		}

		AlMesh* MeshPtr = MeshNode->mesh();
		if (!AlIsValid(MeshPtr))
		{
			continue;
		}
		TUniquePtr<AlMesh> Mesh(MeshPtr);

		AlMatrix4x4 AlMatrix;
		DagNode.Key->localTransformationMatrix(AlMatrix);

		Mesh->transform(AlMatrix);

		const FColor& ShaderColor = DagNode.Value;
		const int32 ShaderId = CreateShaderId(ShaderColor);
		const FString SlotMaterialName = FString::FromInt(ShaderId);

		const bool bMerge = true;
		OpenModelUtils::TransferAlMeshToMeshDescription(*MeshPtr, *SlotMaterialName, MeshDescription, MeshParameters, bMerge);
	}

	// Build edge meta data
	FStaticMeshOperations::DetermineEdgeHardnessesFromVertexInstanceNormals(MeshDescription);

	return MoveTemp(MeshDescription);
}

TOptional<FMeshDescription> FWireTranslatorImpl::GetMeshOfNodeMesh(AlDagNode& TesselatedNode, TSharedRef<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters, AlMatrix4x4* AlMeshInvGlobalMatrix)
{
	AlMeshNode* MeshNode = TesselatedNode.asMeshNodePtr();
	if (!AlIsValid(MeshNode))
	{
		return TOptional<FMeshDescription>();
	}

	AlMesh* Mesh = MeshNode->mesh();
	if (!AlIsValid(Mesh))
	{
		return TOptional<FMeshDescription>();
	}

	TUniquePtr<AlMesh> SharedMesh(Mesh);
	if (AlMeshInvGlobalMatrix != nullptr)
	{
		SharedMesh->transform(*AlMeshInvGlobalMatrix);
	}

	return ImportMesh(*Mesh, MeshElement, MeshParameters);
}

TOptional<FMeshDescription> FWireTranslatorImpl::GetMeshDescription(TSharedRef<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters, TSharedRef<BodyData> Body)
{
	if (Body->GetShells().Num() == 0)
	{
		return TOptional<FMeshDescription>();
	}

	const TArray<TPair<TSharedPtr<AlDagNode>, FColor>>& Shells = Body->GetShells();
	TSharedPtr<AlDagNode> DagNode = Shells[0].Key;
	AlLayer* LayerPtr = DagNode->layer();
	if (AlIsValid(LayerPtr))
	{
		const TUniquePtr<AlLayer> Layer(LayerPtr);
		if (LayerPtr->isSymmetric())
		{
			MeshParameters.bIsSymmetric = true;
			double Normal[3], Origin[3];
			LayerPtr->symmetricNormal(Normal[0], Normal[1], Normal[2]);
			LayerPtr->symmetricOrigin(Origin[0], Origin[1], Origin[2]);

			MeshParameters.SymmetricOrigin.X = (float)Origin[0];
			MeshParameters.SymmetricOrigin.Y = (float)Origin[1];
			MeshParameters.SymmetricOrigin.Z = (float)Origin[2];
			MeshParameters.SymmetricNormal.X = (float)Normal[0];
			MeshParameters.SymmetricNormal.Y = (float)Normal[1];
			MeshParameters.SymmetricNormal.Z = (float)Normal[2];
		}
	}

	if (Body->IsCadData())
	{
		return GetMeshOfShellBody(Body, MeshElement, MeshParameters);
	}
	else
	{
		return GetMeshOfMeshBody(Body, MeshElement, MeshParameters);
	}
}

TOptional<FMeshDescription> FWireTranslatorImpl::GetMeshDescription(TSharedRef<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters)
{
	TSharedPtr<AlDagNode>* DagNodeTemp = MeshElementToAlDagNodeMap.Find(&MeshElement.Get());
	if (DagNodeTemp == nullptr || !DagNodeTemp->IsValid())
	{
		TSharedPtr<BodyData>* BodyTemp = MeshElementToBodyMap.Find(&MeshElement.Get());
		if (BodyTemp != nullptr && BodyTemp->IsValid())
		{
			return GetMeshDescription(MeshElement, MeshParameters, (*BodyTemp).ToSharedRef());
		}
		return TOptional<FMeshDescription>();
	}

	AlDagNode& DagNode = **DagNodeTemp;
	AlObjectType objectType = DagNode.type();

	if (objectType == kShellNodeType || objectType == kSurfaceNodeType || objectType == kMeshNodeType)
	{
		boolean bAlOrientation;
		DagNode.getSurfaceOrientation(bAlOrientation);
		MeshParameters.bNeedSwapOrientation = (bool)bAlOrientation;

		AlLayer* LayerPtr = DagNode.layer();
		if (AlIsValid(LayerPtr))
		{
			const TUniquePtr<AlLayer> Layer(LayerPtr);
			if (LayerPtr->isSymmetric())
			{
				MeshParameters.bIsSymmetric = true;
				double Normal[3], Origin[3];
				LayerPtr->symmetricNormal(Normal[0], Normal[1], Normal[2]);
				LayerPtr->symmetricOrigin(Origin[0], Origin[1], Origin[2]);

				MeshParameters.SymmetricOrigin.X = (float)Origin[0];
				MeshParameters.SymmetricOrigin.Y = (float)Origin[1];
				MeshParameters.SymmetricOrigin.Z = (float)Origin[2];
				MeshParameters.SymmetricNormal.X = (float)Normal[0];
				MeshParameters.SymmetricNormal.Y = (float)Normal[1];
				MeshParameters.SymmetricNormal.Z = (float)Normal[2];
			}
		}
	}

	switch (objectType)
	{
	case kShellNodeType:
	case kSurfaceNodeType:
	{
		return GetMeshOfShellNode(DagNode, MeshElement, MeshParameters);
		break;
	}

	case kMeshNodeType:
	{
		return GetMeshOfNodeMesh(DagNode, MeshElement, MeshParameters);
		break;
	}

	default:
		break;
	}

	return TOptional<FMeshDescription>();
}


// Note that Alias file unit is cm like UE
TOptional<FMeshDescription> FWireTranslatorImpl::ImportMesh(AlMesh& InMesh, TSharedRef<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& InMeshParameters)
{
	FMeshDescription MeshDescription;
	DatasmithMeshHelper::PrepareAttributeForStaticMesh(MeshDescription);

	const FColor* ShaderColorPtr = nullptr;
	const FString* ShaderName = MeshElementToShaderName.Find(&MeshElement.Get());
	if (ShaderName)
	{
		ShaderColorPtr = ShaderNameToColor.Find(*ShaderName);
	}
	if (!ShaderColorPtr) // Should never happen
	{
		ShaderColorPtr = &DefaultColor;
	}
	int32 ShaderId = CreateShaderId(*ShaderColorPtr);
	FString SlotMaterialName = FString::FromInt(ShaderId);

	const bool bMerge = false;
	OpenModelUtils::TransferAlMeshToMeshDescription(InMesh, *SlotMaterialName, MeshDescription, InMeshParameters, bMerge);

	// Build edge meta data
	FStaticMeshOperations::DetermineEdgeHardnessesFromVertexInstanceNormals(MeshDescription);

	return MoveTemp(MeshDescription);
}

bool FWireTranslatorImpl::LoadStaticMesh(const TSharedRef<IDatasmithMeshElement> MeshElement, FDatasmithMeshElementPayload& OutMeshPayload, const FDatasmithTessellationOptions& InTessellationOptions)
{
	CADLibrary::FMeshParameters MeshParameters;
	if (TOptional<FMeshDescription> Mesh = GetMeshDescription(MeshElement, MeshParameters))
	{
		OutMeshPayload.LodMeshes.Add(MoveTemp(Mesh.GetValue()));
		CADModelConverter->AddSurfaceDataForMesh(MeshElement->GetFile(), MeshParameters, InTessellationOptions, OutMeshPayload);
	}
	return OutMeshPayload.LodMeshes.Num() > 0;
}


#endif

//////////////////////////////////////////////////////////////////////////
// UDatasmithWireTranslator
//////////////////////////////////////////////////////////////////////////

FDatasmithWireTranslator::FDatasmithWireTranslator()
	: Translator(nullptr)
{
}

void FDatasmithWireTranslator::Initialize(FDatasmithTranslatorCapabilities& OutCapabilities)
{
#if WITH_EDITOR
	if (GIsEditor && !GEditor->PlayWorld && !GIsPlayInEditorWorld)
	{
#ifdef USE_OPENMODEL
		static void* DllHandle = FPlatformProcess::GetDllHandle(TEXT("libalias_api.dll"));
		if (DllHandle)
		{
			// Check installed version of Alias Tools because binaries before 2021.3 are not compatible with Alias 2022
			uint64 FileVersion = FPlatformMisc::GetFileVersion(TEXT("libalias_api.dll"));

#ifdef OPEN_MODEL_2020
			if (LibAlias2020_Version < FileVersion && FileVersion < LibAlias2021_Version)
			{
				TFunction<bool()> DisplayMessage = []() -> bool
				{
					UE_LOG(LogDatasmithWireTranslator, Warning, TEXT(WRONG_VERSION_TEXT)); return true;
				};
				static const bool bIsDispaly = DisplayMessage();
				OutCapabilities.bIsEnabled = false;
				return;
			}
#endif

			if (LibAliasVersionMin <= FileVersion && FileVersion < LibAliasVersionMax)
			{
				OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("wire"), *AliasVersionChar });
				OutCapabilities.bIsEnabled = true;
				return;
			}

			OutCapabilities.bIsEnabled = false;
			return;
		}
#endif
	}
#endif

	OutCapabilities.bIsEnabled = false;
}

bool FDatasmithWireTranslator::IsSourceSupported(const FDatasmithSceneSource& Source)
{
#ifdef USE_OPENMODEL
	return true;
#else
	return false;
#endif
}

bool FDatasmithWireTranslator::LoadScene(TSharedRef<IDatasmithScene> OutScene)
{
#ifdef USE_OPENMODEL
	const FString& Filename = GetSource().GetSourceFile();

	Translator = MakeShared<FWireTranslatorImpl>(Filename, OutScene);
	if (!Translator)
	{
		return false;
	}

	UE_LOG(LogDatasmithWireTranslator, Display, TEXT("CAD translation [%s]."), *Filename);
	UE_LOG(LogDatasmithWireTranslator, Display, TEXT(" - Parsing Library:      %s"), TEXT("Alias"));
	UE_LOG(LogDatasmithWireTranslator, Display, TEXT(" - Tessellation Library: %s"), CADLibrary::FImportParameters::bGDisableCADKernelTessellation ? TEXT("TechSoft") : TEXT("CADKernel"));

	FString OutputPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FDatasmithWireTranslatorModule::Get().GetTempDir(), TEXT("Cache"), GetSource().GetSceneName()));
	IFileManager::Get().MakeDirectory(*OutputPath, true);
	Translator->SetOutputPath(OutputPath);

	Translator->SetTessellationOptions(GetCommonTessellationOptions());

	return Translator->Read();

#else
	return false;
#endif
}

void FDatasmithWireTranslator::UnloadScene()
{
}

bool FDatasmithWireTranslator::LoadStaticMesh(const TSharedRef<IDatasmithMeshElement> MeshElement, FDatasmithMeshElementPayload& OutMeshPayload)
{
#ifdef USE_OPENMODEL
	if (Translator)
	{
		return Translator->LoadStaticMesh(MeshElement, OutMeshPayload, GetCommonTessellationOptions());
	}
#endif
	return false;
}

void FDatasmithWireTranslator::SetSceneImportOptions(const TArray<TObjectPtr<UDatasmithOptionsBase>>& Options)
{
#ifdef USE_OPENMODEL
	FParametricSurfaceTranslator::SetSceneImportOptions(Options);

	if (Translator)
	{
		Translator->SetTessellationOptions(GetCommonTessellationOptions());
	}
#endif
}

} // namespace

#undef LOCTEXT_NAMESPACE // "DatasmithWireTranslator"

#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif