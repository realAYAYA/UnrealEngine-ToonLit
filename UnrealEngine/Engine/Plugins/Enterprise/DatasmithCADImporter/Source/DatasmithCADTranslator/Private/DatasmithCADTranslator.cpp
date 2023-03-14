// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithCADTranslator.h"

#include "CADFileReader.h"
#include "CADInterfacesModule.h"
#include "CADKernelSurfaceExtension.h"
#include "CADOptions.h"

#include "DatasmithCADTranslatorModule.h"
#include "DatasmithDispatcher.h"
#include "DatasmithMeshBuilder.h"
#include "DatasmithSceneGraphBuilder.h"
#include "DatasmithTranslator.h"
#include "DatasmithUtils.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "IDatasmithSceneElements.h"


DEFINE_LOG_CATEGORY(LogCADTranslator);

namespace DatasmithCADTranslatorImpl
{
static bool bGEnableNativeIFCTranslator = false;
FAutoConsoleVariableRef GCADTranslatorEnableNativeIFCTranslator(
	TEXT("ds.IFC.EnableNativeTranslator"),
	bGEnableNativeIFCTranslator,
	TEXT("\
Enable/disable UE native IFC translator. If native translator is disabled, TechSoft is used.\n\
Default is disable\n"),
	ECVF_Default);
}

void FDatasmithCADTranslator::Initialize(FDatasmithTranslatorCapabilities& OutCapabilities)
{
	TFunction<bool()> GetCADInterfaceAvailability = []() -> bool
	{
		if (ICADInterfacesModule::GetAvailability() == ECADInterfaceAvailability::Unavailable)
		{
			return false;
		}
		return true;
	};

	static bool bIsCADInterfaceAvailable = GetCADInterfaceAvailability();
	if (!bIsCADInterfaceAvailable)
	{
		OutCapabilities.bIsEnabled = false;
		return;
	}
	OutCapabilities.bIsEnabled = true;

#ifndef CAD_TRANSLATOR_DEBUG
	OutCapabilities.bParallelLoadStaticMeshSupported = true;
#endif
	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("CATPart"), TEXT("CATIA Part files") });
	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("CATProduct"), TEXT("CATIA Product files") });
	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("CATShape"), TEXT("CATIA Shape files") });
	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("cgr"), TEXT("CATIA Graphical Representation V5 files") });

	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("3dxml"), TEXT("CATIA 3D xml files") });
	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("3drep"), TEXT("CATIA 3D xml files") });

	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("model"), TEXT("CATIA V4 files") });
	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("session"), TEXT("CATIA V4 files") });
	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("exp"), TEXT("CATIA V4 files") });
	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("dlv"), TEXT("CATIA V4 files") });

	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("asm.*"), TEXT("Creo Assembly files") });
	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("creo.*"), TEXT("Creo Assembly files") });
	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("creo"), TEXT("Creo Assembly files") });
	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("neu.*"), TEXT("Creo Assembly files") });
	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("neu"), TEXT("Creo Assembly files") });
	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("prt.*"), TEXT("Creo Part files") });
	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("xas"), TEXT("Creo Assembly files") });
	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("xpr"), TEXT("Creo Part files") });

	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("iam"), TEXT("Inventor Assembly files") });
	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("ipt"), TEXT("Inventor Part files") });

	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("iges"), TEXT("IGES files") });
	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("igs"), TEXT("IGES files") });

	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("jt"), TEXT("JT Open files") });

	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("sat"), TEXT("3D ACIS model files") });
	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("sab"), TEXT("3D ACIS model files") });

	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("SLDASM"), TEXT("SolidWorks Product files") });
	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("SLDPRT"), TEXT("SolidWorks Part files") });

	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("step"), TEXT("Step files") });
	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("stp"), TEXT("Step files") });
	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("xml"), TEXT("AP242 Xml Step files, XPDM files") });

	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("x_t"), TEXT("Parasolid files (Text format)") });
	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("x_b"), TEXT("Parasolid files (Binary format)") });

	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("asm"), TEXT("Unigraphics, NX, SolidEdge Assembly files") });
	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("prt"), TEXT("Unigraphics, NX Part files") });

	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("par"), TEXT("SolidEdge Part files") });
	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("psm"), TEXT("SolidEdge Part files") });

	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("dwg"), TEXT("AutoCAD, Model files") });

	if (!DatasmithCADTranslatorImpl::bGEnableNativeIFCTranslator)
	{
		OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("ifc"), TEXT("IFC (Industry Foundation Classes)") });
	}

	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("hsf"), TEXT("HOOPS stream files") });
	OutCapabilities.SupportedFileFormats.Add(FFileFormatInfo{ TEXT("prc"), TEXT("HOOPS stream files") });
}

bool FDatasmithCADTranslator::IsSourceSupported(const FDatasmithSceneSource& Source)
{
	if (Source.GetSourceFileExtension() != TEXT("xml"))
	{
		return true;
	}

	return Datasmith::CheckXMLFileSchema(Source.GetSourceFile(), TEXT("XPDMXML"), TEXT("ns3:Uos"));
}

bool FDatasmithCADTranslator::LoadScene(TSharedRef<IDatasmithScene> DatasmithScene)
{
	using namespace CADLibrary;
	const FDatasmithTessellationOptions& TesselationOptions = GetCommonTessellationOptions();
	CADLibrary::FFileDescriptor FileDescriptor(*FPaths::ConvertRelativePathToFull(GetSource().GetSourceFile()));

	UE_LOG(LogCADTranslator, Display, TEXT("CAD translation [%s]."), *FileDescriptor.GetSourcePath());
	UE_LOG(LogCADTranslator, Display, TEXT(" - Parsing Library:      %s"), TEXT("TechSoft"));
	UE_LOG(LogCADTranslator, Display, TEXT(" - Tessellation Library: %s")
		, FImportParameters::bGDisableCADKernelTessellation ? TEXT("TechSoft") : TEXT("CADKernel"));
	UE_LOG(LogCADTranslator, Display, TEXT(" - Cache mode:           %s")
		, FImportParameters::bGEnableCADCache ? (FImportParameters::bGOverwriteCache ? TEXT("Override") : TEXT("Enabled")) : TEXT("Disabled"));
	UE_LOG(LogCADTranslator, Display, TEXT(" - Processing:           %s")
		, FImportParameters::bGEnableCADCache ? (GMaxImportThreads == 1 ? TEXT("Sequencial") : TEXT("Parallel")) : TEXT("Sequencial"));

	TFunction<double(double, double, const TCHAR*)> CheckParameterValue = [](double Value, double MinValue, const TCHAR* ParameterName) -> double
	{
		if (Value < MinValue)
		{
			UE_LOG(LogCADTranslator, Warning, TEXT("%s value (%f) of tessellation parameters is smaller than the minimal value %d. It's value is modified to respect the limit"), ParameterName, Value, MinValue);
			return MinValue;
		}
		return Value;
	};
	
	ImportParameters.SetTesselationParameters(CheckParameterValue(TesselationOptions.ChordTolerance, UE::DatasmithTessellation::MinTessellationChord, TEXT("Chord tolerance")),
		FMath::IsNearlyZero(TesselationOptions.MaxEdgeLength) ? 0. : CheckParameterValue(TesselationOptions.MaxEdgeLength, UE::DatasmithTessellation::MinTessellationEdgeLength, TEXT("Max Edge Length")),
		CheckParameterValue(TesselationOptions.NormalTolerance, UE::DatasmithTessellation::MinTessellationAngle, TEXT("Max Angle")),
		(EStitchingTechnique)TesselationOptions.StitchingTechnique);
	ImportParameters.SetModelCoordinateSystem(FDatasmithUtils::EModelCoordSystem::ZUp_RightHanded);

	UE_LOG(LogCADTranslator, Display, TEXT(" - Import parameters:"));
	UE_LOG(LogCADTranslator, Display, TEXT("     - ChordTolerance:     %f"), ImportParameters.GetChordTolerance());
	UE_LOG(LogCADTranslator, Display, TEXT("     - MaxEdgeLength:      %f"), ImportParameters.GetMaxEdgeLength());
	UE_LOG(LogCADTranslator, Display, TEXT("     - MaxNormalAngle:     %f"), ImportParameters.GetMaxNormalAngle());
	FString StitchingTechnique;
	switch(ImportParameters.GetStitchingTechnique())
	{
		case EStitchingTechnique::StitchingHeal:
			StitchingTechnique = TEXT("Heal");
			break;
		case EStitchingTechnique::StitchingSew:
			StitchingTechnique = TEXT("Sew");
			break;
		default:
			StitchingTechnique = TEXT("None");
			break;
	}
	UE_LOG(LogCADTranslator, Display, TEXT("     - StitchingTechnique: %s"), *StitchingTechnique);
	if (!FImportParameters::bGDisableCADKernelTessellation)
	{
		UE_LOG(LogCADTranslator, Display, TEXT("     - Stitching Options:"));
		UE_LOG(LogCADTranslator, Display, TEXT("         - ForceSew:        %s"), ImportParameters.bGStitchingForceSew ? TEXT("True") : TEXT("False"));
		UE_LOG(LogCADTranslator, Display, TEXT("         - RemoveThinFaces: %s"), ImportParameters.bGStitchingRemoveThinFaces ? TEXT("True") : TEXT("False"));
		UE_LOG(LogCADTranslator, Display, TEXT("         - ForceFactor:     %f"), ImportParameters.GStitchingForceFactor);
	}

	switch (FileDescriptor.GetFileFormat())
	{
	case CADLibrary::ECADFormat::N_X:
	{
		break;
	}

	case ECADFormat::SOLIDWORKS:
	{
		ImportParameters.SetModelCoordinateSystem(FDatasmithUtils::EModelCoordSystem::YUp_RightHanded);
		break;
	}

	case ECADFormat::INVENTOR:
	case ECADFormat::CREO:
	{
		ImportParameters.SetModelCoordinateSystem(FDatasmithUtils::EModelCoordSystem::YUp_RightHanded);
		break;
	}

	case ECADFormat::DWG:
	{
		break;
	}

	case ECADFormat::IFC:
	{
		ImportParameters.SetModelCoordinateSystem(FDatasmithUtils::EModelCoordSystem::ZUp_RightHanded_FBXLegacy);
		break;
	}

	default:
		break;
	}

	FString CachePath = FDatasmithCADTranslatorModule::Get().GetCacheDir();
	if (!CachePath.IsEmpty())
	{
		CachePath = FPaths::ConvertRelativePathToFull(CachePath);
	}

	// Use sequential translation (multi-processed or not)
	if (FImportParameters::bGEnableCADCache)
	{
		TMap<uint32, FString> CADFileToUEFileMap;
		{
			constexpr double RecommandedRamPerWorkers = 6.;
			constexpr double OneGigaByte = 1024. * 1024. * 1024.;
			const double AvailableRamGB = (double) (FPlatformMemory::GetStats().AvailablePhysical / OneGigaByte);

			const int32 MaxNumberOfWorkers = FPlatformMisc::NumberOfCores();
			const int32 RecommandedNumberOfWorkers = (int32) (AvailableRamGB / RecommandedRamPerWorkers + 0.5);

			// UE recommendation 
			int32 NumberOfWorkers = FMath::Min(MaxNumberOfWorkers, RecommandedNumberOfWorkers);

			// User choice but limited by the number of cores. More brings nothing
			if (GMaxImportThreads > 1)
			{
				NumberOfWorkers = FMath::Min(GMaxImportThreads, MaxNumberOfWorkers);
			}

			// If the file cannot have reference. One worker is enough.
			if (GMaxImportThreads != 1 && !FileDescriptor.CanReferenceOtherFiles())
			{
				NumberOfWorkers = 1;
			}

			DatasmithDispatcher::FDatasmithDispatcher Dispatcher(ImportParameters, CachePath, NumberOfWorkers, CADFileToUEFileMap, CADFileToUEGeomMap);
			Dispatcher.AddTask(FileDescriptor);

			Dispatcher.Process(GMaxImportThreads != 1);
		}

		FDatasmithSceneGraphBuilder SceneGraphBuilder(CADFileToUEFileMap, CachePath, DatasmithScene, GetSource(), ImportParameters);
		SceneGraphBuilder.Build();

		MeshBuilderPtr = MakeUnique<FDatasmithMeshBuilder>(CADFileToUEGeomMap, CachePath, ImportParameters);

		return true;
	}

	FCADFileReader FileReader(ImportParameters, FileDescriptor, *FPaths::EnginePluginsDir(), CachePath);
	if (FileReader.ProcessFile() != ECADParsingResult::ProcessOk)
	{
		return false;
	}

	FCADFileData& CADFileData = FileReader.GetCADFileData();
	FDatasmithSceneBaseGraphBuilder SceneGraphBuilder(&CADFileData.GetSceneGraphArchive(), CachePath, DatasmithScene, GetSource(), ImportParameters);
	SceneGraphBuilder.Build();

	MeshBuilderPtr = MakeUnique<FDatasmithMeshBuilder>(CADFileData.GetBodyMeshes(), ImportParameters);

	return true;
}

void FDatasmithCADTranslator::UnloadScene()
{
	MeshBuilderPtr.Reset();

	CADFileToUEGeomMap.Empty();
}

bool FDatasmithCADTranslator::LoadStaticMesh(const TSharedRef<IDatasmithMeshElement> MeshElement, FDatasmithMeshElementPayload& OutMeshPayload)
{
	if (!MeshBuilderPtr.IsValid())
	{
		return false;
	}

	CADLibrary::FMeshParameters MeshParameters;

	if (TOptional< FMeshDescription > Mesh = MeshBuilderPtr->GetMeshDescription(MeshElement, MeshParameters))
	{
		OutMeshPayload.LodMeshes.Add(MoveTemp(Mesh.GetValue()));

		if (CADLibrary::FImportParameters::bGDisableCADKernelTessellation)
		{
			ParametricSurfaceUtils::AddSurfaceData(MeshElement->GetFile(), ImportParameters, MeshParameters, GetCommonTessellationOptions(), OutMeshPayload);
		}
		else
		{
			CADKernelSurface::AddSurfaceDataForMesh(MeshElement->GetFile(), ImportParameters, MeshParameters, GetCommonTessellationOptions(), OutMeshPayload);
		}
	}

	return OutMeshPayload.LodMeshes.Num() > 0;
}

