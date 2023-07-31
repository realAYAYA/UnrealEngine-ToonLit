// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using EpicGames.Core;

public class DX12 : ModuleRules
{
	protected virtual bool bSupportsD3D12Core { get => Target.Platform.IsInGroup(UnrealPlatformGroup.Windows); }

	public DX12(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		Log.TraceLog("Running DX12");

		string DirectXSDKDir = Target.UEThirdPartySourceDirectory + "Windows/DirectX";

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			string LibDir = DirectXSDKDir + "/Lib/x64/";
			PublicAdditionalLibraries.AddRange(
				new string[] {
					LibDir + "dxgi.lib",
					LibDir + "d3d12.lib",
					LibDir + "dxguid.lib",
				});
		}

		// D3D12Core runtime. Currently x64 only, but ARM64 can also be supported if necessary.
		if (bSupportsD3D12Core && Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			PublicDefinitions.Add("D3D12_CORE_ENABLED=1");

			// Copy D3D12Core binaries to the target directory, so it can be found by D3D12.dll loader.
			// D3D redistributable search path is configured in LaunchWindows.cpp like so:			
			// 		extern "C" { _declspec(dllexport) extern const UINT D3D12SDKVersion = 4; }
			// 		extern "C" { _declspec(dllexport) extern const char* D3D12SDKPath = u8".\\D3D12\\"; }

			// NOTE: We intentionally put D3D12 redist binaries into a subdirectory.
			// System D3D12 loader will be able to pick them up using D3D12SDKPath export, if running on compatible Win10 version.
			// If we are running on incompatible/old system, we don't want those libraries to ever be loaded.
			// A specific D3D12Core.dll is only compatible with a matching d3d12SDKLayers.dll counterpart.
			// If a wrong d3d12SDKLayers.dll is present in PATH, it will be blindly loaded and the engine will crash.

			RuntimeDependencies.Add(
				"$(TargetOutputDir)/D3D12/D3D12Core.dll", 
				"$(EngineDir)/Binaries/ThirdParty/Windows/DirectX/x64/D3D12Core.dll");

			if (Target.Configuration != UnrealTargetConfiguration.Shipping &&
				Target.Configuration != UnrealTargetConfiguration.Test)
			{
				RuntimeDependencies.Add(
					"$(TargetOutputDir)/D3D12/d3d12SDKLayers.dll", 
					"$(EngineDir)/Binaries/ThirdParty/Windows/DirectX/x64/d3d12SDKLayers.dll");
			}
		}
		else if (!bSupportsD3D12Core)
		{
			PublicDefinitions.Add("D3D12_CORE_ENABLED=0");
		}

		// Always delay-load D3D12
		PublicDelayLoadDLLs.AddRange(
			new string[] {
				"d3d12.dll"
			});

		if (bSupportsD3D12Core)
		{
			PublicSystemIncludePaths.Add(DirectXSDKDir + "/include");

			PublicDefinitions.Add("D3D12_MAX_DEVICE_INTERFACE=10");
			PublicDefinitions.Add("D3D12_MAX_COMMANDLIST_INTERFACE=6");
			PublicDefinitions.Add("D3D12_SUPPORTS_INFO_QUEUE=1");
			PublicDefinitions.Add("D3D12_SUPPORTS_DXGI_DEBUG=1");
			PublicDefinitions.Add("DXGI_MAX_FACTORY_INTERFACE=6");
			PublicDefinitions.Add("DXGI_MAX_SWAPCHAIN_INTERFACE=4");
		}

		// DX12 extensions, not part of SDK
		string D3DX12Dir = Target.UEThirdPartySourceDirectory + "Windows/D3DX12";
		PublicSystemIncludePaths.Add(D3DX12Dir + "/include");
	}
}

