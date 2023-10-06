// Copyright Epic Games, Inc. All Rights Reserved.

#include "Export.h"
#include "ResourcesIDs.h"
#include "Utils/Error.h"
#include "Utils/AutoChangeDatabase.h"
#include "Exporter.h"

BEGIN_NAMESPACE_UE_AC

enum : GSType
{
	kDatasmithFileRefCon = 'Tuds',
	kStrFileType = 'TEXT',
	kStrFileCreator = '    '
};

const utf8_t* StrFileExtension = "udatasmith";

static GSErrCode __ACENV_CALL SaveToDatasmithFile(const API_IOParams* IOParams, Modeler::SightPtr sight)
{
	GSErrCode GSErr = TryFunctionCatchAndAlert("FExport::SaveDatasmithFile", [&IOParams, &sight]() -> GSErrCode {
		return FExport::SaveDatasmithFile(*IOParams, *sight.GetPtr());
	});
	ACAPI_KeepInMemory(true);
	return GSErr;
}

GSErrCode FExport::Register()
{
	return ACAPI_Register_FileType(kDatasmithFileRefCon, kStrFileType, kStrFileCreator, StrFileExtension, kIconDSFile,
								   LocalizeResId(kStrListFileTypes), 0, SaveAs3DSupported);
}

GSErrCode FExport::Initialize()
{
	GSErrCode GSErr = ACAPI_Install_FileTypeHandler3D(kDatasmithFileRefCon, SaveToDatasmithFile);
	if (GSErr != NoError)
	{
		UE_AC_DebugF("FExport::Initialize - ACAPI_Install_FileTypeHandler3D error=%s\n", GetErrorName(GSErr));
	}
	return GSErr;
}

GSErrCode FExport::SaveDatasmithFile(const API_IOParams& IOParams, const Modeler::Sight& InSight)
{
	try
	{
		FAutoChangeDatabase db(APIWind_FloorPlanID);

		ModelerAPI::Model		 model;
		Modeler::ConstModel3DPtr model3D(InSight.GetMainModelPtr());
#if AC_VERSION < 26
		AttributeReader			 AttrReader; // deprecated constructor, temporary!
		UE_AC_TestGSError(EXPGetModel(model3D, &model, &AttrReader));
#else
		GS::Owner<Modeler::IAttributeReader> AttrReader(ACAPI_Attribute_GetCurrentAttributeSetReader());
		UE_AC_TestGSError(EXPGetModel(model3D, &model, AttrReader.Get()));
#endif

		FExporter exporter;
		exporter.DoExport(model, IOParams);
	}
	catch (...)
	{
		IO::fileSystem.Delete(*IOParams.fileLoc); // Delete tmp file
		throw;
	}
	return GS::NoError;
}

END_NAMESPACE_UE_AC
