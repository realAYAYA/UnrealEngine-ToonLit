// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithUE4ArchiCAD.h"
#include "IDirectLinkUI.h"
#include "IDatasmithExporterUIModule.h"

int32 _RunDatasmithUE(const TCHAR* /*Commandline*/)
{
	IDatasmithExporterUIModule* DsExporterUIModule = IDatasmithExporterUIModule::Get();
	if (DsExporterUIModule != nullptr)
	{
		IDirectLinkUI* DLUI = DsExporterUIModule->GetDirectLinkExporterUI();
		if (DLUI != nullptr)
		{
			DLUI->OpenDirectLinkStreamWindow();
		}
	}
	return 0;
}

