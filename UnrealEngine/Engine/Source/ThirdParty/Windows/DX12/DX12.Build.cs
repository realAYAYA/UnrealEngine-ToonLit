// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using System.Linq;
using UnrealBuildTool;

public class DX12 : ModuleRules
{
	public DX12(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			PublicDependencyModuleNames.Add("DirectX");	

			string[] AllD3DLibs = new string[]
			{
				"dxgi.lib",
				"d3d12.lib",
				"dxguid.lib",
			};

			string DirectXSDKDir = Target.WindowsPlatform.DirectXLibDir;
			PublicAdditionalLibraries.AddRange(AllD3DLibs.Select(LibName => Path.Combine(DirectXSDKDir, LibName)));

			// D3D12Core runtime
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
				Path.Combine(Target.WindowsPlatform.DirectXDllDir, "D3D12Core.dll"));

			RuntimeDependencies.Add(
				"$(TargetOutputDir)/D3D12/d3d12SDKLayers.dll",
				Path.Combine(Target.WindowsPlatform.DirectXDllDir, "d3d12SDKLayers.dll"));

			// Always delay-load D3D12
			PublicDelayLoadDLLs.Add("d3d12.dll");

			PublicDefinitions.Add("D3D12_MAX_DEVICE_INTERFACE=12");
			PublicDefinitions.Add("D3D12_MAX_COMMANDLIST_INTERFACE=9");
			PublicDefinitions.Add("D3D12_MAX_FEATURE_OPTIONS=20");
			PublicDefinitions.Add("D3D12_SUPPORTS_INFO_QUEUE=1");
			PublicDefinitions.Add("D3D12_SUPPORTS_DXGI_DEBUG=1");
			PublicDefinitions.Add("DXGI_MAX_FACTORY_INTERFACE=7");
			PublicDefinitions.Add("DXGI_MAX_SWAPCHAIN_INTERFACE=4");

			// DX12 extensions, not part of SDK
			PublicSystemIncludePaths.Add(Path.Combine(Target.UEThirdPartySourceDirectory, "Windows", "D3DX12", "include"));
		}
	}
}

