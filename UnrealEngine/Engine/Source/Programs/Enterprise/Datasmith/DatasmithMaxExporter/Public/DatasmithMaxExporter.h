// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DatasmithMaxExporterDefines.h"

#include "Windows/AllowWindowsPlatformTypes.h"

MAX_INCLUDES_START
	#include "impexp.h"
	#include "max.h"
MAX_INCLUDES_END

class FDatasmithMaxSceneParser;
class FDatasmithSceneExporter;
class FDatasmithMaxProgressManager;
class IDatasmithScene;
class FDatasmithScene;
class RefEnumProc;

struct FDatasmithMaxExportOptions
{
	bool bShowPrompts = true;
	bool bShowWarnings = true;
	bool bExportGeometry = true;
	bool bExportLights = true;
	bool bExportMaterials = true;
	bool bExportActors = true;
	bool bExportDummies = true;
	bool bExportSelectionOnly = false;
	bool bExportAnimations = false;
};

class FDatasmithMaxExporter : public SceneExport
{
	friend INT_PTR CALLBACK ExportOptionsDlgProc(HWND Handle, UINT Message, WPARAM WParam, LPARAM LParam);

public:
	static HINSTANCE HInstanceMax;

	virtual int ExtCount() override;					// Number of extensions supported
	virtual const TCHAR* Ext(int n) override;			// Extension #n (i.e. "3DS")
	virtual const TCHAR* LongDesc() override;			// Long ASCII description (i.e. "Autodesk 3D Studio File")
	virtual const TCHAR* ShortDesc() override;			// Short ASCII description (i.e. "3D Studio")
	virtual const TCHAR* AuthorName() override;			// Author name
	virtual const TCHAR* CopyrightMessage() override;	// Copyright message
	virtual const TCHAR* OtherMessage1()override;		// Other message #1
	virtual const TCHAR* OtherMessage2() override;		// Other message #2
	virtual unsigned int Version() override;			// Version number * 100 (i.e. v3.01 = 301)
	virtual void ShowAbout(HWND Handle) override;		// Show DLL's "About..." box
	virtual int DoExport(const TCHAR* Filename, ExpInterface* ExpIf, Interface* IF, BOOL bSuppressPrompts, DWORD Options) override;	// Export file
	virtual BOOL SupportsOptions(int Ext, DWORD Options) override;

	static int ExportToFile(const TCHAR* Filename, const FDatasmithMaxExportOptions& ExporterOptions);

	FDatasmithMaxExportOptions ExporterOptions;

protected:
	static int ExportScene( FDatasmithSceneExporter* DatasmithSceneExporter, TSharedRef< IDatasmithScene > DatasmithScene, FDatasmithMaxSceneParser& InSceneParser, const TCHAR* Filename, const FDatasmithMaxExportOptions& ExporterOptions, TSharedPtr< FDatasmithMaxProgressManager >& ProgressManager );
	static void PrepareRender(RefEnumProc* Callback, const TCHAR* Message, TSharedPtr< FDatasmithMaxProgressManager >& ProgressManager );
};

// MaxScript interface 
//********************
#define DATASMITH_EXPORTER_INTERFACE Interface_ID(0x63591f05, 0xece213c)
#define GetDatasmithExporterInterface(cd) \
		(IDatasmithExport*)(cd)->GetInterface(DATASMITH_EXPORTER_INTERFACE)

enum DatasmithInterfaceFunctionIDs {
	DS_GET_INCLUDE_TARGET_ID,
	DS_SET_INCLUDE_TARGET_ID,
	DS_GET_ANIMATED_TRANSFORMS_ID,
	DS_SET_ANIMATED_TRANSFORMS_ID,
	DS_GET_VERSION_ID,
	DS_EXPORT_ID,
};

enum DatasithInterfaceEnumIDs {
	DS_EXPORT_INCLUDETARGET_ENUM_ID,
	DS_EXPORT_ANIMATEDTRANSFORM_ENUM_ID,
};

enum DataSmithImportOptionIncludeTarget {
	DS_INCLUDE_TARGET_VISIBLE_OBJECTS,
	DS_INCLUDE_TARGET_SELECTED_OBJECTS,
};

enum DatasmithImportOptionAnimatedTransforms {
	DS_ANIMATED_TRANSFORMS_CURRENT_FRAME,
	DS_ANIMATED_TRANSFORMS_ACTIVE_TIME_SEGMENT,
};

class IDatasmithExport : public FPStaticInterface
{
public:
	virtual INT GetInclude() = 0;
	virtual void SetInclude(INT Value) = 0;
	virtual INT GetAnimatedTransform() = 0;
	virtual void SetAnimatedTransform(INT Value) = 0;
	virtual INT GetVersion() = 0;
	virtual bool Export(const TCHAR* Filename, BOOL bSuppressWarnings) = 0;
};

class FDatasmithExportImpl : public IDatasmithExport
{
	// Function Map for Function Publish System 
	//***********************************
	DECLARE_DESCRIPTOR(FDatasmithExportImpl)
	BEGIN_FUNCTION_MAP
		PROP_FNS(DS_GET_INCLUDE_TARGET_ID, GetInclude, DS_SET_INCLUDE_TARGET_ID, SetInclude, TYPE_ENUM);
		PROP_FNS(DS_GET_ANIMATED_TRANSFORMS_ID, GetAnimatedTransform, DS_SET_ANIMATED_TRANSFORMS_ID, SetAnimatedTransform, TYPE_ENUM);
		RO_PROP_FN(DS_GET_VERSION_ID, GetVersion, TYPE_INT);
		FN_2(DS_EXPORT_ID, TYPE_BOOL, Export, TYPE_STRING, TYPE_BOOL);
	END_FUNCTION_MAP

	virtual INT GetInclude() override;
	virtual void SetInclude(INT Value) override;
	virtual INT GetAnimatedTransform() override;
	virtual void SetAnimatedTransform(INT Value) override;
	virtual INT GetVersion() override;
	virtual bool Export(const TCHAR* Filename, BOOL bSuppressWarnings) override;

	FDatasmithMaxExportOptions ExporterOptions;
};

#include "Windows/HideWindowsPlatformTypes.h"
