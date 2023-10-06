// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithMaxExporter.h"

#ifndef NEW_DIRECTLINK_PLUGIN


#include "Modules/ModuleManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"


#include "DatasmithCore.h"
#include "DatasmithExporterManager.h"
#include "DatasmithExportOptions.h"
#include "DatasmithSceneExporter.h"

#include "DatasmithMaxAttributes.h"
#include "DatasmithMaxClassIDs.h"
#include "DatasmithMaxExporterDefines.h"
#include "DatasmithMaxHelper.h"
#include "DatasmithMaxMeshExporter.h"
#include "DatasmithMaxSceneParser.h"
#include "DatasmithMaxSceneExporter.h"
#include "DatasmithMaxLogger.h"
#include "DatasmithMaxWriter.h"
#include "DatasmithMaxProgressManager.h"
#include "DatasmithMaxExporterUtils.h"

#include "Resources/Windows/resource.h"

#include "Windows/AllowWindowsPlatformTypes.h"
MAX_INCLUDES_START
	#include "Max.h"
	#include "bitmap.h"
	#include "gamma.h"
	#include "IPathConfigMgr.h"
	#include "maxheapdirect.h"
	#include "maxscript/maxwrapper/mxsobjects.h"
MAX_INCLUDES_END

THIRD_PARTY_INCLUDES_START
#include <locale.h>
THIRD_PARTY_INCLUDES_END

static FString OriginalLocale( _wsetlocale(LC_NUMERIC, nullptr) ); // Cache LC_NUMERIC locale before initialization of UnrealEditor
static FString NewLocale = _wsetlocale(LC_NUMERIC, TEXT("C"));

HINSTANCE FDatasmithMaxExporter::HInstanceMax;

class FDatasmithMaxExporterClassDesc : public ClassDesc
{
public:
	int IsPublic() { return 1; }
	void* Create(BOOL bIsLoading = FALSE) { return new FDatasmithMaxExporter; }
	const TCHAR* ClassName() { return TEXT("datasmithmaxexporter"); }
	SClass_ID SuperClassID() { return SCENE_EXPORT_CLASS_ID; }
	Class_ID ClassID() { return DATASMITHUNREALEXPORTER_CLASS_ID; }
	const TCHAR* Category() { return TEXT("Unreal Datasmith Exporter"); }
	bool UseOnlyInternalNameForMAXScriptExposure() { return true; }
	const TCHAR* InternalName() { return TEXT("DatasmithExport"); }

#if MAX_PRODUCT_YEAR_NUMBER > 2021
	virtual const MCHAR* NonLocalizedClassName() override { return _M("datasmithmaxexporter"); }
#endif

#ifdef UNICODE_
	virtual const MCHAR* GetRsrcString(int id) { return _M(""); }
#else
	virtual const MCHAR* GetRsrcString(INT_PTR id) { return _M(""); }
#endif
};

static FDatasmithMaxExporterClassDesc DatasmithMaxExporterClassDesc;

//------------------------------------------------------
// This is the interface to Jaguar:
//------------------------------------------------------

__declspec( dllexport ) int LibInitialize(void)
{
	bool bResult = FDatasmithExporterManager::Initialize();

	// Restore LC_NUMERIC locale after initialization of UnrealEditor
	_wsetlocale(LC_NUMERIC, *OriginalLocale);

	return bResult;
}

__declspec( dllexport ) int LibShutdown()
{
	FDatasmithExporterManager::Shutdown();

	// Set GIsRequestingExit flag so that static dtors don't crash
	if (!IsEngineExitRequested())
	{
		RequestEngineExit(TEXT("LibShutdown received"));
	}

	return TRUE;
}

__declspec(dllexport) const TCHAR* LibDescription()
{
	return TEXT("Unreal Datasmith Exporter");
}

// 4 libs import/project/actor/escalator
__declspec(dllexport) int LibNumberClasses()
{
	return 1;
}

// Return version so can detect obsolete DLLs
__declspec(dllexport) ULONG LibVersion()
{
	return VERSION_3DSMAX;
}

// Let the plug-in register itself for deferred loading
__declspec(dllexport) ULONG CanAutoDefer()
{
	return 0;
}

__declspec(dllexport) ClassDesc* LibClassDesc(int i)
{
	switch (i)
	{
	case 0:
		return &DatasmithMaxExporterClassDesc;
	default:
		return 0;
	}
}

int FDatasmithMaxExporter::ExtCount()
{
	return 1;
}

const TCHAR* FDatasmithMaxExporter::Ext(int n)
{ // Extensions supported for import/export modules
	switch (n)
	{
	case 0:
		return FDatasmithUtils::GetFileExtension();
	}
	return TEXT("");
}

const TCHAR* FDatasmithMaxExporter::LongDesc()
{ // Long ASCII description (i.e. "Targa 2.0 Image File")
	return FDatasmithUtils::GetLongAppName();
}

const TCHAR* FDatasmithMaxExporter::ShortDesc()
{ // Short ASCII description (i.e. "Targa")
	return FDatasmithUtils::GetLongAppName();
}

const TCHAR* FDatasmithMaxExporter::AuthorName()
{ // ASCII Author name
	return TEXT("Epic Games");
}

const TCHAR* FDatasmithMaxExporter::CopyrightMessage()
{ // ASCII Copyright message
	return TEXT("Epic Games");
}

const TCHAR* FDatasmithMaxExporter::OtherMessage1()
{ // Other message #1
	return TEXT("");
}

const TCHAR* FDatasmithMaxExporter::OtherMessage2()
{ // Other message #2
	return TEXT("");
}

unsigned int FDatasmithMaxExporter::Version()
{ // Version number * 100 (i.e. v3.01 = 301)
	return FDatasmithUtils::GetEnterpriseVersionAsInt();
}

void FDatasmithMaxExporter::ShowAbout(HWND hWnd)
{ // Optional
}

/** public functions **/
BOOL WINAPI DllMain(HINSTANCE hinstDLL, ULONG FdwReason, LPVOID LpvReserved)
{
	if (FdwReason == DLL_PROCESS_ATTACH)
	{
		FDatasmithMaxExporter::HInstanceMax = hinstDLL;
	}
	return (TRUE);
}


namespace DatasmithMaxExporterUtils
{
	void RemapUVChannelsForCompositeTexture(const TSharedPtr<IDatasmithCompositeTexture>& CompositeElement, const FDatasmithMaxMeshExporter::FUVChannelsMap& UVChannels)
	{
		// Remap the coordinate index of the texture sampler with the given UV channels mapping
		for (int32 i = 0; i < CompositeElement->GetParamSurfacesCount(); ++i)
		{
			if (CompositeElement->GetUseTexture(i))
			{
				FDatasmithTextureSampler& TextureSampler = CompositeElement->GetParamTextureSampler(i);
				const int32* UVChannel = UVChannels.Find(TextureSampler.CoordinateIndex);
				if (UVChannel != nullptr)
				{
					TextureSampler.CoordinateIndex = *UVChannel;
				}
			}
		}
	}

	void RemapUVChannels(const TSharedPtr<IDatasmithMaterialElement>& MaterialElement, const FDatasmithMaxMeshExporter::FUVChannelsMap& UVChannels)
	{
		// Remap UV channels of all composite textures of each shaders in the material
		for (int32 i = 0; i < MaterialElement->GetShadersCount(); ++i)
		{
			const TSharedPtr<IDatasmithShaderElement>& ShaderElement = MaterialElement->GetShader(i);
			RemapUVChannelsForCompositeTexture(ShaderElement->GetDiffuseComp(), UVChannels);
			RemapUVChannelsForCompositeTexture(ShaderElement->GetRefleComp(), UVChannels);
			RemapUVChannelsForCompositeTexture(ShaderElement->GetRoughnessComp(), UVChannels);
			RemapUVChannelsForCompositeTexture(ShaderElement->GetNormalComp(), UVChannels);
			RemapUVChannelsForCompositeTexture(ShaderElement->GetBumpComp(), UVChannels);
			RemapUVChannelsForCompositeTexture(ShaderElement->GetTransComp(), UVChannels);
			RemapUVChannelsForCompositeTexture(ShaderElement->GetMaskComp(), UVChannels);
			RemapUVChannelsForCompositeTexture(ShaderElement->GetMetalComp(), UVChannels);
			RemapUVChannelsForCompositeTexture(ShaderElement->GetEmitComp(), UVChannels);
			RemapUVChannelsForCompositeTexture(ShaderElement->GetWeightComp(), UVChannels);
		}
	}

	void RemapUVChannels(const TSharedPtr<IDatasmithUEPbrMaterialElement>& MaterialElement, const FDatasmithMaxMeshExporter::FUVChannelsMap& UVChannels)
	{
		// Remap UV channels of all composite textures of each shaders in the material
		for (int32 i = 0; i < MaterialElement->GetExpressionsCount(); ++i)
		{
			IDatasmithMaterialExpression* MaterialExpression = MaterialElement->GetExpression(i);

			if ( MaterialExpression && MaterialExpression->IsSubType( EDatasmithMaterialExpressionType::TextureCoordinate ) )
			{
				IDatasmithMaterialExpressionTextureCoordinate& TextureCoordinate = static_cast< IDatasmithMaterialExpressionTextureCoordinate& >( *MaterialExpression );

				const int32* UVChannel = UVChannels.Find( TextureCoordinate.GetCoordinateIndex() );
				if ( UVChannel != nullptr )
				{
					TextureCoordinate.SetCoordinateIndex( *UVChannel );
				}
			}
		}
	}
}

int FDatasmithMaxExporter::ExportScene(FDatasmithSceneExporter* DatasmithSceneExporter, TSharedRef< IDatasmithScene > DatasmithScene, FDatasmithMaxSceneParser& InSceneParser, const TCHAR* Filename, const FDatasmithMaxExportOptions& ExporterOptions, TSharedPtr < FDatasmithMaxProgressManager >& ProgressManager)
{
	float UnitMultiplier = FMath::Abs(GetSystemUnitScale(UNITS_CENTIMETERS));

	MSTR Renderer;
	FString Host;
	Host = TEXT("Autodesk 3dsmax ") + FString::FromInt(MAX_VERSION_MAJOR) + TEXT(".") + FString::FromInt(MAX_VERSION_MINOR) + TEXT(".") + FString::FromInt(MAX_VERSION_POINT);
	GetCOREInterface()->GetCurrentRenderer()->GetClassName(Renderer);

	DatasmithSceneExporter->SetName( *FPaths::GetBaseFilename( Filename ) );
	DatasmithSceneExporter->SetOutputPath( *FPaths::GetPath( Filename ) );
	DatasmithSceneExporter->SetLogger( DatasmithMaxLogger::Get().AsShared() );

	DatasmithScene->SetVendor( TEXT("Autodesk") );
	DatasmithScene->SetProductName( TEXT("3dsmax") );
	DatasmithScene->SetHost( *( Host + Renderer ) );
	FString Version = FString::FromInt(MAX_VERSION_MAJOR) + TEXT(".") + FString::FromInt(MAX_VERSION_MINOR) + TEXT(".") + FString::FromInt(MAX_VERSION_POINT);
	DatasmithScene->SetProductVersion( *Version );

	// Use the same name for the unique level sequence as the scene name
	TSharedRef< IDatasmithLevelSequenceElement > LevelSequence = FDatasmithSceneFactory::CreateLevelSequence( *FPaths::GetBaseFilename( Filename ) );
	LevelSequence->SetFrameRate( GetFrameRate() );
	DatasmithScene->AddLevelSequence( LevelSequence );

	int GeometryCounter = 0;
	FDatasmithMaxMeshExporter DatasmithMeshExporter;

	int PercentTakenForMeshes = 75;
	if (FDatasmithExportOptions::ResizeTexturesMode == EDSResizeTextureMode::NoResize)
	{
		PercentTakenForMeshes = 95;
	}

	ProgressManager->SetMainMessage(TEXT("Exporting to Datasmith"));
	ProgressManager->SetProgressStart(0);
	ProgressManager->SetProgressEnd(PercentTakenForMeshes);

	FDatasmithUniqueNameProvider UniqueNameProvider;
	TMap < INode *, TSharedPtr< IDatasmithActorElement >> ElementMap;
	int LastDisplayedProgress = -1;
	for (int i = 0; i < InSceneParser.GetRendereableNodesCount(); i++)
	{
		if (InSceneParser.GetEntityType(i) == EMaxEntityType::Geometry && GetCOREInterface()->GetCancel() == false)
		{
			const float ProgressRatio = (float(i)) / InSceneParser.GetRendereableNodesCount();
			int ProgressPct = PercentTakenForMeshes * ProgressRatio;
			if (ProgressPct != LastDisplayedProgress)
			{
				LastDisplayedProgress = ProgressPct;
				FString ProgressText = InSceneParser.GetNode(i)->GetName();
				ProgressManager->ProgressEvent(ProgressRatio, *ProgressText);
			}

			// Export to file
			FString NodeID = FString::FromInt(InSceneParser.GetNode(i)->GetHandle());

			{
				const int32 MeshIndex = InSceneParser.GetRailCloneMeshIndex(i);
				if (MeshIndex != -1)
				{
					NodeID.Append(TEXT("_") + FString::FromInt(MeshIndex));
				}
			}

			TSet<uint16> SupportedChannels;
			bool bForceSinglemat = true;
			bool bBakePivot = false;
			Mtl* StaticMeshMtl = nullptr;
			FString MeshLabel;

			if (ExporterOptions.bExportGeometry &&
				(InSceneParser.GetInstanceMode(i) == EMaxExporterInstanceMode::NotInstanced || InSceneParser.GetInstanceMode(i) == EMaxExporterInstanceMode::InstanceMaster || InSceneParser.GetInstanceMode(i) == EMaxExporterInstanceMode::UnrealHISM))
			{
				StaticMeshMtl = InSceneParser.GetCustomMeshNode(i) ? InSceneParser.GetCustomMeshNode(i)->GetMtl() : InSceneParser.GetNode(i)->GetMtl();

				if (StaticMeshMtl != nullptr && FDatasmithMaxMatHelper::GetMaterialClass(StaticMeshMtl) == EDSMaterialType::XRefMat)
				{
					// HACK: 3dsmax only initializes xref materials if the material editor is open
					// so we need to do it by ourselves to be on the safe side
					Interval Interv;
					StaticMeshMtl->Update(GetCOREInterface()->GetTime(), Interv);
					StaticMeshMtl->RenderBegin(GetCOREInterface()->GetTime());
					Mtl* RenderedMaterial = FDatasmithMaxMatHelper::GetRenderedXRefMaterial(StaticMeshMtl);
					if (RenderedMaterial != nullptr)
					{
						RenderedMaterial->Update(GetCOREInterface()->GetTime(), Interv);
						RenderedMaterial->RenderBegin(GetCOREInterface()->GetTime());
					}
					else
					{
						MSTR MatName = StaticMeshMtl->GetName();
						FString Error = FString::Printf(TEXT("XRef file not found for XRefMat \"%s\""), MatName.data());
						DatasmithMaxLogger::Get().AddMissingAssetError(*Error);
					}
					StaticMeshMtl = RenderedMaterial;
				}

				if (StaticMeshMtl != nullptr)
				{
					if (FDatasmithMaxMatHelper::GetMaterialClass(StaticMeshMtl) == EDSMaterialType::MultiMat)
					{
						bForceSinglemat = false;
					}
					else
					{
						for (int InstanceIndex = 0; InstanceIndex < InSceneParser.GetInstances(i).Count(); InstanceIndex++)
						{
							Mtl* InstanceMaterial = InSceneParser.GetInstances(i)[InstanceIndex]->GetMtl();
							if (InstanceMaterial != nullptr)
							{
								if (FDatasmithMaxMatHelper::GetMaterialClass(StaticMeshMtl) == EDSMaterialType::MultiMat)
								{
									bForceSinglemat = false;
								}
							}
						}
					}
				}

				FString Error;
				TSharedPtr< IDatasmithMeshElement > MeshElement;
				FMaxExportMeshArgs MeshExportArgs;
				MeshExportArgs.bForceSingleMat= bForceSinglemat;
				MeshExportArgs.Node = InSceneParser.GetCustomMeshNode(i) ? InSceneParser.GetCustomMeshNode(i) : InSceneParser.GetNode(i);
				MeshExportArgs.DatasmithAttributes = InSceneParser.GetDatasmithStaticMeshAttributes(i) ? &InSceneParser.GetDatasmithStaticMeshAttributes(i).GetValue() : nullptr;
				MeshExportArgs.ExportPath = DatasmithSceneExporter->GetAssetsOutputPath();
				MeshExportArgs.UnitMultiplier = UnitMultiplier;
				MeshExportArgs.ExportName = *NodeID;


				//Regular mesh
				if (!InSceneParser.GetMaxMesh(i))
				{
					FTransform Pivot = FDatasmithMaxSceneExporter::GetPivotTransform( InSceneParser.GetNode(i), UnitMultiplier );
					bBakePivot = !Pivot.Equals( FTransform::Identity );

					for (int j = 0; j < InSceneParser.GetInstances(i).Count(); j++)
					{
						INode* InstanceNode = InSceneParser.GetInstances(i)[j];
						if ( !Pivot.Equals( FDatasmithMaxSceneExporter::GetPivotTransform( InstanceNode, UnitMultiplier ) ) )
						{
							bBakePivot = false;

							DatasmithMaxLogger::Get().AddGeneralError(*FString::Printf(TEXT("Multiple pivot locations found on instances of object %s. This is supported in Unreal, but it's recommended to use the Reset Xform Utility on those objects prior to export.")
								, static_cast<const TCHAR*>(InSceneParser.GetNode(i)->GetName())));
							break;
						}
					}

					if ( !bBakePivot )
					{
						Pivot.SetIdentity();
					}

					MeshExportArgs.bBakePivot = bBakePivot;
					MeshExportArgs.Pivot = Pivot;
				}
				else //special mesh access for Railclone
				{
					MeshExportArgs.MaxMesh = InSceneParser.GetMaxMesh(i);
				}

				StaticMeshMtl = MeshExportArgs.Node ? MeshExportArgs.Node->GetMtl() : nullptr;
				MeshElement = DatasmithMeshExporter.ExportMesh(MeshExportArgs, SupportedChannels, Error);


				bool bReadyToExport = MeshElement.IsValid();
				InSceneParser.SetReadyToExport(i, bReadyToExport);

				if (bReadyToExport)
				{
					if (InSceneParser.GetCustomMeshNode(i))
					{
						MeshLabel = InSceneParser.GetRailCloneMeshIndex(i) != -1 ? InSceneParser.GetCustomMeshNode(i)->GetName() + (TEXT("_") + FString::FromInt(InSceneParser.GetRailCloneMeshIndex(i))) : InSceneParser.GetNode(i)->GetName();
					}
					else
					{
						MeshLabel = InSceneParser.GetRailCloneMeshIndex(i) != -1 ? InSceneParser.GetNode(i)->GetName() + (TEXT("_") + FString::FromInt(InSceneParser.GetRailCloneMeshIndex(i))) : InSceneParser.GetNode(i)->GetName();
					}
					MeshElement->SetLabel(*MeshLabel);
					DatasmithScene->AddMesh( MeshElement );
				}
				else
				{
					DatasmithMaxLogger::Get().AddInvalidObj(InSceneParser.GetNode(i));
					DatasmithMaxLogger::Get().AddGeneralError(*Error);
				}
			}
			else
			{
				if (ExporterOptions.bExportActors) // I need what channel is supported to apply materials
				{
					InSceneParser.SetReadyToExport(i, DatasmithMeshExporter.CalcSupportedChannelsOnly(InSceneParser.GetNode(i), SupportedChannels, bForceSinglemat));
				}
			}

			if ( ExporterOptions.bExportActors && InSceneParser.GetReadyToExport(i) )
			{
				const TSharedPtr< IDatasmithMeshElement >& MeshElement = DatasmithScene->GetMesh( GeometryCounter );

				FString MeshName;
				if ( MeshElement.IsValid() )
				{
					MeshName = MeshElement->GetName();
				}

				EStaticMeshExportMode StaticMeshExportMode = EStaticMeshExportMode::Default;
				const TOptional<FDatasmithMaxStaticMeshAttributes>& DatasmithMaxStaticMeshAttributes = InSceneParser.GetDatasmithStaticMeshAttributes(i);
				if (DatasmithMaxStaticMeshAttributes)
				{
					StaticMeshExportMode = DatasmithMaxStaticMeshAttributes->GetExportMode();
				}

				if ( InSceneParser.GetInstanceMode(i) != EMaxExporterInstanceMode::UnrealHISM )
				{

					for (int j = 0; j < InSceneParser.GetInstances(i).Count(); j++)
					{
						if (InSceneParser.GetInstances(i)[j]->IsHidden(0, true) == false && ( InSceneParser.bOnlySelection == false || InSceneParser.GetInstances(i)[j]->Selected() != 0 ))
						{
							FDatasmithMaxSceneExporter::ExportMeshActor(DatasmithScene, SupportedChannels, InSceneParser.GetInstances(i)[j], *MeshName, UnitMultiplier, bBakePivot, StaticMeshMtl, StaticMeshExportMode);

							const TSharedPtr< IDatasmithActorElement > ActorElement = DatasmithScene->GetActor(DatasmithScene->GetActorsCount() - 1);
							if ( ExporterOptions.bExportAnimations )
							{
								FDatasmithMaxSceneExporter::ExportAnimation(LevelSequence, InSceneParser.GetInstances(i)[j], ActorElement->GetName(), UnitMultiplier);
							}

							ElementMap.Add(InSceneParser.GetInstances(i)[j], ActorElement);
						}
					}
					GeometryCounter++;
				}
				else // EMaxExporterInstanceMode::UnrealHISM
				{
					INode* Node = InSceneParser.GetNode(i);
					TSharedPtr< IDatasmithActorElement >* PointerToActorElement = ElementMap.Find(Node);
					TSharedPtr< IDatasmithActorElement > ActorElement;
					if (PointerToActorElement)
					{
						ActorElement = *PointerToActorElement;
					}
					else
					{
						FDatasmithMaxSceneExporter::ExportActor(DatasmithScene, Node, *MeshName, UnitMultiplier);
						ActorElement = DatasmithScene->GetActor(DatasmithScene->GetActorsCount() - 1);
						ElementMap.Add(InSceneParser.GetNode(i), ActorElement);
					}

					TSharedPtr< IDatasmithActorElement > InversedHISMActor;
					ActorElement->AddChild( FDatasmithMaxSceneExporter::ExportHierarchicalInstanceStaticMeshActor( DatasmithScene, InSceneParser.GetNode(i), InSceneParser.GetCustomMeshNode(i), *MeshLabel, SupportedChannels,
						StaticMeshMtl, InSceneParser.GetInstancesTransform(i), *MeshName, UnitMultiplier, StaticMeshExportMode, InversedHISMActor) );

					if( InversedHISMActor.IsValid() )
					{
						ActorElement->AddChild(InversedHISMActor);
					}

					GeometryCounter++;
				}
			}
		}
	}

	if ( ExporterOptions.bExportMaterials && GetCOREInterface()->GetCancel() == false)
	{
		for (int i = 0; i < InSceneParser.GetTexturesCount(); i++)
		{
			FDatasmithMaxMatExport::GetXMLTexture(DatasmithScene, InSceneParser.GetTexture(i), DatasmithSceneExporter->GetAssetsOutputPath());
		}

		for (int i = 0; i < InSceneParser.GetMaterialsCount(); i++)
		{
			if ( Mtl* MaxMaterial = InSceneParser.GetMaterial(i) )
			{
				FDatasmithMaxMatExport::ExportUniqueMaterial( DatasmithScene, MaxMaterial, DatasmithSceneExporter->GetAssetsOutputPath() );
			}
		}

		//Exporting Blend materials may add more than one material to the scene, so we loop those separately from the SceneParser
		for (int32 MaterialIndex = 0; MaterialIndex < DatasmithScene->GetMaterialsCount(); ++MaterialIndex)
		{
			if ( TSharedPtr< IDatasmithBaseMaterialElement > MaterialElement = DatasmithScene->GetMaterial(MaterialIndex) )
			{
				// Remap the UV channels of the textures used in this material to the UV channels that are actually exported in a mesh that uses this material.
				// In 3dsmax, the UV channel indices need not be be consecutive, while the UV channels exported to Unreal are consecutive.
				FDatasmithMaxMeshExporter::FUVChannelsMap UVChannels;
				bool bFoundUVChannelsMap = false;
				const TCHAR* MaterialName = MaterialElement->GetName();

				for (int32 MeshIndex = 0; !bFoundUVChannelsMap && MeshIndex < DatasmithScene->GetMeshesCount(); ++MeshIndex)
				{
					const TSharedPtr< IDatasmithMeshElement > MeshElement = DatasmithScene->GetMesh(MeshIndex);
					for (int32 MaterialSlotIndex = 0; MaterialSlotIndex < MeshElement->GetMaterialSlotCount(); ++MaterialSlotIndex)
					{
						//If the material is used by the mesh
						if (FCString::Strcmp(MaterialName, MeshElement->GetMaterialSlotAt(MaterialSlotIndex)->GetName()) == 0)
						{
							UVChannels = DatasmithMeshExporter.GetUVChannelsMapForMesh(MeshElement->GetName());
							if (UVChannels.Num() > 0)
							{
								bFoundUVChannelsMap = true;
								// Stop as soon as we have found the UVChannels map
								break;
							}
						}
					}
				}

				if (bFoundUVChannelsMap)
				{
					if ( MaterialElement->IsA( EDatasmithElementType::Material ) )
					{
						DatasmithMaxExporterUtils::RemapUVChannels( StaticCastSharedPtr< IDatasmithMaterialElement >( MaterialElement ), UVChannels );
					}
					else if ( MaterialElement->IsA( EDatasmithElementType::UEPbrMaterial ) )
					{
						DatasmithMaxExporterUtils::RemapUVChannels( StaticCastSharedPtr< IDatasmithUEPbrMaterialElement >( MaterialElement ), UVChannels );
					}
				}
			}
		}
	}

	bool bExportCameras = true;
	if (bExportCameras && GetCOREInterface()->GetCancel() == false)
	{
		for (int i = 0; i < InSceneParser.GetRendereableNodesCount(); i++)
		{
			if (InSceneParser.GetEntityType(i) == EMaxEntityType::Camera)
			{
				INodeTab Instances = InSceneParser.GetInstances(i);
				for (int j = 0; j < Instances.Count(); j++)
				{
					if (Instances[j]->IsHidden(0, true) == false && ( InSceneParser.bOnlySelection == false || Instances[j]->Selected() != 0 ))
					{
						FString UniqueName = FString::FromInt(Instances[j]->GetHandle());

						if (FDatasmithMaxSceneExporter::ExportCameraActor(DatasmithScene, InSceneParser.GetNode(i), Instances, j, *UniqueName, UnitMultiplier))
						{
							const TSharedPtr< IDatasmithActorElement > ActorElement = DatasmithScene->GetActor(DatasmithScene->GetActorsCount() - 1);
							if ( ExporterOptions.bExportAnimations )
							{
								FDatasmithMaxSceneExporter::ExportAnimation(LevelSequence, Instances[j], ActorElement->GetName(), UnitMultiplier);
							}

							ElementMap.Add(Instances[j], ActorElement);
						}
					}
				}
			}
		}

		FDatasmithMaxSceneExporter::ExportToneOperator(DatasmithScene);
	}

	if ( ExporterOptions.bExportDummies && GetCOREInterface()->GetCancel() == false)
	{
		for (int i = 0; i < InSceneParser.GetRendereableNodesCount(); i++)
		{
			if (InSceneParser.GetEntityType(i) == EMaxEntityType::Dummy)
			{
				INodeTab Instances = InSceneParser.GetInstances(i);
				for (int j = 0; j < Instances.Count(); j++)
				{
					if (Instances[j]->IsHidden(0, true) == false && (InSceneParser.bOnlySelection == false || Instances[j]->Selected() != 0))
					{
						FString UniqueName = FString::FromInt(Instances[j]->GetHandle());

						if (FDatasmithMaxSceneExporter::ExportActor(DatasmithScene, Instances[j], *UniqueName, UnitMultiplier))
						{
							TSharedPtr< IDatasmithActorElement > ActorElement = DatasmithScene->GetActor(DatasmithScene->GetActorsCount() - 1);
							if ( ExporterOptions.bExportAnimations )
							{
								FDatasmithMaxSceneExporter::ExportAnimation(LevelSequence, Instances[j], ActorElement->GetName(), UnitMultiplier);
							}

							ElementMap.Add(Instances[j], ActorElement);
						}
					}
				}
			}
		}
	}

	if ( ExporterOptions.bExportLights && GetCOREInterface()->GetCancel() == false)
	{
		for (int i = 0; i < InSceneParser.GetRendereableNodesCount(); i++)
		{
			if (InSceneParser.GetEntityType(i) == EMaxEntityType::Light)
			{
				INodeTab Instances = InSceneParser.GetInstances(i);
				for (int j = 0; j < Instances.Count(); j++)
				{
					if (Instances[j]->IsHidden(0, true) == false && ( InSceneParser.bOnlySelection == false || Instances[j]->Selected()!=0 ))
					{
						FString UniqueName = FString::FromInt(Instances[j]->GetHandle());

						if (FDatasmithMaxSceneExporter::WriteXMLLightActor(DatasmithScene, InSceneParser.GetNode(i), Instances[j], *UniqueName, UnitMultiplier))
						{
							const TSharedPtr< IDatasmithActorElement > ActorElement = DatasmithScene->GetActor(DatasmithScene->GetActorsCount() - 1);
							if ( ExporterOptions.bExportAnimations )
							{
								const TSharedPtr< IDatasmithLightActorElement> LightElement = StaticCastSharedPtr< IDatasmithLightActorElement >(ActorElement);
								const FMaxLightCoordinateConversionParams LightParams(InSceneParser.GetNode(i),
									LightElement->IsA(EDatasmithElementType::AreaLight) ? StaticCastSharedPtr<IDatasmithAreaLightElement>(LightElement)->GetLightShape() : EDatasmithLightShape::None);

								FDatasmithMaxSceneExporter::ExportAnimation(LevelSequence, Instances[j], ActorElement->GetName(), UnitMultiplier, LightParams);
							}

							ElementMap.Add(Instances[j], ActorElement);
						}
					}
				}
			}
		}
		FDatasmithMaxSceneExporter::WriteEnvironment(DatasmithScene, InSceneParser.bOnlySelection);
	}

	// We don't know how the 3ds max lookup_MaxClass is implemented so we use this map to skip it when we can
	TMap<TPair<uint32, TPair<uint32, uint32>>, MAXClass*> KnownMaxDesc;
	// Same for the lookup_MAXSuperClass.
	TMap<uint32, MAXSuperClass*> KnownMaxSuperClass;

	for (auto& Element : ElementMap)
	{
		INode* Node = Element.Key;
		TSharedPtr < IDatasmithActorElement > ActorElement = Element.Value;
		INode* ParentNode = Node->GetParentNode();

		DatasmithMaxExporterUtils::ExportMaxTagsForDatasmithActor( ActorElement, Node, ParentNode, KnownMaxDesc, KnownMaxSuperClass );

		if (ParentNode != NULL && ElementMap.Find(ParentNode) != NULL && Node != NULL)
		{
			// Actor must be removed first before being added to the parent, otherwise it would be removed from the parent right away
			DatasmithScene->RemoveActor(Element.Value, EDatasmithActorRemovalRule::RemoveChildren);
			(*ElementMap.Find(ParentNode))->AddChild(Element.Value);
		}
	}

	ProgressManager->SetProgressStart((float)PercentTakenForMeshes);
	ProgressManager->SetProgressEnd(100.f);
	DatasmithSceneExporter->SetProgressManager( ProgressManager );

	if (GetCOREInterface()->GetCancel() == false)
	{
		DatasmithSceneExporter->Export( DatasmithScene );
	}

	if (DatasmithMaxLogger::Get().HasWarnings() &&  ExporterOptions.bShowWarnings)
	{
		CreateDialog(HInstanceMax, MAKEINTRESOURCE(IDD_ERROR_MSGS), GetCOREInterface()->GetMAXHWnd(), MsgListDlgProc);
	}
	InSceneParser.Status = 1;

	return 1;
}

INT_PTR CALLBACK ExportOptionsDlgProc(HWND Handle, UINT Message, WPARAM WParam, LPARAM LParam)
{
	static FDatasmithMaxExporter* Exporter;

	FString IniFilePath = FString( GetCOREInterface()->GetDir(APP_PLUGCFG_DIR) ) + TEXT("/DatasmithMaxExporter.ini");

	switch (Message)
	{
	case WM_INITDIALOG:
	{
		Exporter = (FDatasmithMaxExporter*)LParam;

		// Set the default values for the export options
		Exporter->ExporterOptions.bExportGeometry = true;
		Exporter->ExporterOptions.bExportMaterials = true;
		Exporter->ExporterOptions.bExportLights = true;
		Exporter->ExporterOptions.bExportActors = true;
		Exporter->ExporterOptions.bExportDummies = true;
		Exporter->ExporterOptions.bExportAnimations = false;

		int IncludeOption = Exporter->ExporterOptions.bExportSelectionOnly ? 1 : 0;
		int AnimatedOption = Exporter->ExporterOptions.bExportAnimations ? 1 : 0;

		CenterWindow(Handle, GetParent(Handle));
		SetFocus(Handle);

		SetWindowText(GetDlgItem(Handle, IDC_LBVersion), *FDatasmithUtils::GetEnterpriseVersionAsString());

		// Include
		SendMessageW(GetDlgItem(Handle, IDC_CBInclude), CB_RESETCONTENT, 0, 0);
		SendMessageW(GetDlgItem(Handle, IDC_CBInclude), CB_ADDSTRING, 0, (LPARAM) TEXT("Visible Objects"));
		SendMessageW(GetDlgItem(Handle, IDC_CBInclude), CB_ADDSTRING, 0, (LPARAM) TEXT("Selection"));
		SendMessageW(GetDlgItem(Handle, IDC_CBInclude), CB_SETCURSEL, IncludeOption, 0);

		// Animated transforms
		SendMessageW(GetDlgItem(Handle, IDC_CBAnimated), CB_RESETCONTENT, 0, 0);
		SendMessageW(GetDlgItem(Handle, IDC_CBAnimated), CB_ADDSTRING, 0, (LPARAM) TEXT("Current Frame Only"));
		SendMessageW(GetDlgItem(Handle, IDC_CBAnimated), CB_ADDSTRING, 0, (LPARAM) TEXT("Active Time Segment"));
		SendMessageW(GetDlgItem(Handle, IDC_CBAnimated), CB_SETCURSEL, AnimatedOption, 0);

		return FALSE;
	}
	case WM_DESTROY:
		return FALSE;
	case WM_COMMAND:

		switch (LOWORD(WParam))
		{
		case IDOK:
		{
			int IncludeOption = SendMessageW(GetDlgItem(Handle, IDC_CBInclude), CB_GETCURSEL, 0, 0);
			Exporter->ExporterOptions.bExportSelectionOnly = (IncludeOption != 0);

			int AnimatedOption = SendMessageW(GetDlgItem(Handle, IDC_CBAnimated), CB_GETCURSEL, 0, 0);
			Exporter->ExporterOptions.bExportAnimations = (AnimatedOption != 0);

			EndDialog(Handle, 1);
			return TRUE;
		}
		case IDCANCEL:
			EndDialog(Handle, 0);
			return TRUE;
		}
	case WM_SYSCOMMAND:
		if ((WParam & 0xfff0) == SC_CONTEXTHELP)
		{
			return FALSE;
		}
	}
	return FALSE;
}

void FDatasmithMaxExporter::PrepareRender(RefEnumProc* Callback, const TCHAR* Message, TSharedPtr< FDatasmithMaxProgressManager >& ProgressManager)
{
	// This function uses EnumRefHierarchy to execute the given Callback only once on each ReferenceMaker in the scene hierarchy.
	// Since EnumRefHierarchy itself traverses the hierarchy, we only need to call it on the children of the root node
	// to execute the callback on the complete ReferenceMaker hierarchy. Note this doesn't work when calling on the root node directly.
	// This is needed to ensure that RenderBegin/RenderEnd is called on the ReferenceMaker to give the correct render mesh, not the viewport mesh.
	ProgressManager->SetProgressStart(0);
	ProgressManager->SetProgressEnd(100);
	ProgressManager->SetMainMessage(Message);

	Callback->BeginEnumeration();

	INode* Node = GetCOREInterface()->GetRootNode();
	ObjectState ObjState = Node->EvalWorldState(0);

	int NumChildren = Node->NumberOfChildren();
	int LastDisplayedProgress = -1;
	for (int ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		// Show progress bar without the Cancel button
		int Progress = 100 * ChildIndex / NumChildren;
		if (Progress != LastDisplayedProgress)
		{
			FString Msg = FString::Printf(TEXT("%d%%"), Progress);
			ProgressManager->ProgressEvent(Progress * 0.01f, *Msg);
			LastDisplayedProgress = Progress;
		}

		ReferenceMaker* ChildReference = (ReferenceMaker*)Node->GetChildNode(ChildIndex);
		ChildReference->EnumRefHierarchy(*Callback);
	}

	Callback->EndEnumeration();
}

class FBeginRefEnumProc : public RefEnumProc
{
public:
	FBeginRefEnumProc(TimeValue StartTime)
	{
		Time = StartTime;
	}

	virtual int proc(ReferenceMaker* RefMaker) override
	{
		RefMaker->RenderBegin(Time);
		return REF_ENUM_CONTINUE;
	}

private:
	TimeValue Time;
};

class FEndRefEnumProc : public RefEnumProc
{
public:
	FEndRefEnumProc(TimeValue EndTime)
	{
		Time = EndTime;
	}

	virtual int proc(ReferenceMaker* RefMaker) override
	{
		RefMaker->RenderEnd(Time);
		return REF_ENUM_CONTINUE;
	}

private:
	TimeValue Time;
};

struct FLocaleSwitcher
{
	FLocaleSwitcher(FString Locale)
	{
		OriginalLocale = _wsetlocale(LC_NUMERIC, nullptr);
		_wsetlocale(LC_NUMERIC, TEXT("C"));
	}

	~FLocaleSwitcher()
	{
		_wsetlocale(LC_NUMERIC, *OriginalLocale);
	}

	FString OriginalLocale;
};

#define WM_TRIGGER_CALLBACK WM_USER + 4764
int FDatasmithMaxExporter::DoExport(const TCHAR* Filename, ExpInterface* ExpIf, Interface* IF, BOOL bSuppressPrompts, DWORD Options)
{
	FLocaleSwitcher SwitchLocale( TEXT("C") );

	DatasmithMaxLogger::Get().Purge();

	// Set a global prompt display switch
	ExporterOptions.bShowPrompts = ExporterOptions.bShowWarnings = bSuppressPrompts ? false : true;
	ExporterOptions.bExportSelectionOnly = (Options & SCENE_EXPORT_SELECTED) ? true : false;

	if (ExporterOptions.bShowPrompts)
	{
		int Result = (int)DialogBoxParam(HInstanceMax, MAKEINTRESOURCE(IDD_EXPORTOPTIONS), IF->GetMAXHWnd(), ExportOptionsDlgProc, (LPARAM)this);
		if (Result <= 0)
		{
			return 1;
		}
	}
	else
	{
		// Set default parameters here
		ExporterOptions.bExportGeometry = true;
		ExporterOptions.bExportLights = true;
		ExporterOptions.bExportMaterials = true;
		ExporterOptions.bExportActors = true;
		ExporterOptions.bExportDummies = true;
		ExporterOptions.bExportAnimations = false;
		FDatasmithExportOptions::ResizeTexturesMode = EDSResizeTextureMode::NoResize;
		FDatasmithExportOptions::MaxTextureSize = 2048;
		FDatasmithExportOptions::PathTexturesMode = EDSResizedTexturesPath::ExportFolder;
	}

	int ImportStatus = ExportToFile(Filename, ExporterOptions);

	if (ImportStatus == 0)
	{
		return 1; // Dialog canceled
	}
	else if (ImportStatus < 0)
	{
		return 0;
	}
	else
	{
		return ImportStatus;
	}
}

INT FDatasmithMaxExporter::ExportToFile(const TCHAR* Filename, const FDatasmithMaxExportOptions& ExporterOptions)
{
	TSharedPtr<FDatasmithMaxProgressManager> ProgressManager(MakeShared<FDatasmithMaxProgressManager>());
	FDatasmithMaxSceneParser Parser;

	GetCOREInterface()->EnableUndo(false);
	GetCOREInterface()->DisableSceneRedraw();
	SuspendAll UberSuspend(TRUE, TRUE, TRUE, TRUE, TRUE, TRUE);

	FBeginRefEnumProc RenderBegin(0);
	PrepareRender(&RenderBegin, TEXT("Preparing export"), ProgressManager);

	{
		// Include the scene parsing time in the export duration stat
		TSharedRef< IDatasmithScene > DatasmithScene = FDatasmithSceneFactory::CreateScene(*FPaths::GetBaseFilename(Filename));
		TUniquePtr< FDatasmithSceneExporter > DatasmithSceneExporter = MakeUnique< FDatasmithSceneExporter >();
		DatasmithSceneExporter->PreExport();

		Parser.ParseCurrentScene(ExporterOptions.bExportSelectionOnly, ProgressManager);

		ExportScene(DatasmithSceneExporter.Get(), DatasmithScene, Parser, Filename, ExporterOptions, ProgressManager);
	}

	FEndRefEnumProc RenderEnd(0);
	PrepareRender(&RenderEnd, TEXT("Cleaning up export"), ProgressManager);

	Parser.RestoreMaterialNames();

	UberSuspend.Resume();
	GetCOREInterface()->EnableSceneRedraw();
	GetCOREInterface()->EnableUndo(true);

	return Parser.Status;
}

BOOL FDatasmithMaxExporter::SupportsOptions(int Ext, DWORD Options)
{
	check(Ext == 0); // We only support one extension
	return (Options == SCENE_EXPORT_SELECTED) ? TRUE : FALSE;
}


// Declare the static interface for using this plugin in MaxScript.
static  FDatasmithExportImpl DatasmithExport_staticInterface(
	DATASMITH_EXPORTER_INTERFACE, _T("DatasmithExport"), 0, &DatasmithMaxExporterClassDesc, 0,

	//The export method
	DS_EXPORT_ID, _T("Export"), 0, TYPE_BOOL, 0, 2,
	_T("FileName"), 0, TYPE_STRING,
	_T("SuppressWarnings"), 0, TYPE_BOOL,

	properties,
	DS_GET_INCLUDE_TARGET_ID, DS_SET_INCLUDE_TARGET_ID, _T("IncludeTarget"), 0, TYPE_ENUM, DS_EXPORT_INCLUDETARGET_ENUM_ID, //Include property
	DS_GET_ANIMATED_TRANSFORMS_ID, DS_SET_ANIMATED_TRANSFORMS_ID, _T("AnimatedTransforms"), 0, TYPE_ENUM, DS_EXPORT_ANIMATEDTRANSFORM_ENUM_ID, //AnimatedTransform property
	DS_GET_VERSION_ID, FP_NO_FUNCTION, _T("Version"), 0, TYPE_INT, //GetVersion Read-only, so we put FP_NO_FUNCTION in the setter functionID

	enums,
	DS_EXPORT_INCLUDETARGET_ENUM_ID, 2,
	_T("VisibleObjects"), DS_INCLUDE_TARGET_VISIBLE_OBJECTS,
	_T("SelectedObjects"), DS_INCLUDE_TARGET_SELECTED_OBJECTS,
	DS_EXPORT_ANIMATEDTRANSFORM_ENUM_ID, 2,
	_T("CurrentFrame"), DS_ANIMATED_TRANSFORMS_CURRENT_FRAME,
	_T("ActiveTimeSegment"), DS_ANIMATED_TRANSFORMS_ACTIVE_TIME_SEGMENT,

	p_end
);

INT FDatasmithExportImpl::GetInclude()
{
	return ExporterOptions.bExportSelectionOnly;
}
void FDatasmithExportImpl::SetInclude(INT Value)
{
	switch (Value)
	{
	case DS_INCLUDE_TARGET_VISIBLE_OBJECTS:
		ExporterOptions.bExportSelectionOnly = false;
		break;
	case DS_INCLUDE_TARGET_SELECTED_OBJECTS:
		ExporterOptions.bExportSelectionOnly = true;
		break;
	default:
		break;
	}
}

INT FDatasmithExportImpl::GetAnimatedTransform()
{
	return ExporterOptions.bExportAnimations;
}

void FDatasmithExportImpl::SetAnimatedTransform(INT Value)
{
	switch (Value)
	{
	case DS_ANIMATED_TRANSFORMS_CURRENT_FRAME:
		ExporterOptions.bExportAnimations = false;
		break;
	case DS_ANIMATED_TRANSFORMS_ACTIVE_TIME_SEGMENT:
		ExporterOptions.bExportAnimations = true;
		break;
	default:
		break;
	}
}

INT FDatasmithExportImpl::GetVersion()
{
	return FDatasmithUtils::GetEnterpriseVersionAsInt();
}

bool FDatasmithExportImpl::Export(const TCHAR* Filename, BOOL bSuppressWarnings)
{
	FLocaleSwitcher SwitchLocale(TEXT("C"));
	DatasmithMaxLogger::Get().Purge();

	ExporterOptions.bShowWarnings = !bSuppressWarnings;

	// Set default parameters here
	ExporterOptions.bShowPrompts = false;
	ExporterOptions.bExportGeometry = true;
	ExporterOptions.bExportLights = true;
	ExporterOptions.bExportMaterials = true;
	ExporterOptions.bExportActors = true;
	ExporterOptions.bExportDummies = true;
	FDatasmithExportOptions::ResizeTexturesMode = EDSResizeTextureMode::NoResize;
	FDatasmithExportOptions::MaxTextureSize = 2048;
	FDatasmithExportOptions::PathTexturesMode = EDSResizedTexturesPath::ExportFolder;

	int ExportStatus = FDatasmithMaxExporter::ExportToFile(Filename, ExporterOptions);

	return !!ExportStatus;
}

#include "Windows/HideWindowsPlatformTypes.h"

#endif // NEW_DIRECTLINK_PLUGIN
