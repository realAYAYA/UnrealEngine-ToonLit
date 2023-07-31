// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHI.h"
#include "Modules/ModuleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MessageDialog.h"

static IDynamicRHIModule* LoadDynamicRHIModule()
{
	// command line overrides
	bool bForceD3D11 = FParse::Param(FCommandLine::Get(), TEXT("d3d11")) || FParse::Param(FCommandLine::Get(), TEXT("dx11"));
	bool bForceD3D12 = FParse::Param(FCommandLine::Get(), TEXT("d3d12")) || FParse::Param(FCommandLine::Get(), TEXT("dx12"));

	if (!(bForceD3D11 || bForceD3D12))
	{
		//Default graphics RHI is only used if no command line option is specified
		FConfigFile EngineSettings;
		FString PlatformNameString = FPlatformProperties::PlatformName();
		const TCHAR* PlatformName = *PlatformNameString;
		FConfigCacheIni::LoadLocalIniFile(EngineSettings, TEXT("Engine"), true, PlatformName);
		FString DefaultGraphicsRHI;
		if (EngineSettings.GetString(TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings"), TEXT("DefaultGraphicsRHI"), DefaultGraphicsRHI))
		{
			FString NAME_DX11(TEXT("DefaultGraphicsRHI_DX11"));
			FString NAME_DX12(TEXT("DefaultGraphicsRHI_DX12"));
			if (DefaultGraphicsRHI == NAME_DX11)
			{
				bForceD3D11 = true;
			}
			else if (DefaultGraphicsRHI == NAME_DX12)
			{
				bForceD3D12 = true;
			}
		}
	}
	else if (bForceD3D11 && bForceD3D12)
	{
		UE_LOG(LogRHI, Fatal, TEXT("-d3d12 and -d3d11 are mutually exclusive options, but more than one was specified on the command-line."));
	}

	// Load the dynamic RHI module.
	IDynamicRHIModule* DynamicRHIModule = NULL;

	// Default to D3D12
	if (!bForceD3D11)
	{
		FApp::SetGraphicsRHI(TEXT("DirectX 12"));
		DynamicRHIModule = FModuleManager::LoadModulePtr<IDynamicRHIModule>(TEXT("D3D12RHI"));
	}
	else
	{
		FApp::SetGraphicsRHI(TEXT("DirectX 11"));
		const TCHAR* D3D11RHIModuleName = TEXT("D3D11RHI");
		DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(D3D11RHIModuleName);
	}

	return DynamicRHIModule;
}

FDynamicRHI* PlatformCreateDynamicRHI()
{
	IDynamicRHIModule* DynamicRHIModule = NULL;

	DynamicRHIModule = LoadDynamicRHIModule();

	// Create the dynamic RHI.
	FDynamicRHI* DynamicRHI = NULL;
	if (!DynamicRHIModule->IsSupported())
	{
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("HoloLens", "FailedToCreateHoloLens_RHI", "HoloLensRHI failure?"));
		FPlatformMisc::RequestExit(1);
		DynamicRHIModule = NULL;
	}
	else
	{
		DynamicRHI = DynamicRHIModule->CreateRHI(ERHIFeatureLevel::ES3_1);
	}

	GMaxRHIFeatureLevel = ERHIFeatureLevel::ES3_1;
	GMaxRHIShaderPlatform = SP_D3D_ES3_1_HOLOLENS;

	return DynamicRHI;
}
