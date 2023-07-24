// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using System;
using System.IO;
using System.Linq;
using UnrealBuildTool;

public class DX12 : ModuleRules
{
	protected bool IsX64Target { get => Target.Architecture.bIsX64; }
	protected virtual bool bUsesWindowsD3D12 { get => Target.Platform.IsInGroup(UnrealPlatformGroup.Windows); }
	protected virtual bool bUsesWindowsD3D12Libs { get => Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) && IsX64Target; }
	protected virtual bool bUsesAgilitySDK { get => Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) && IsX64Target; }

	public DX12(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (bUsesWindowsD3D12)
		{
			Log.TraceLog("Running DX12");

			string[] AllD3DLibs = new string[]
			{
				"dxgi.lib",
				"d3d12.lib",
				"dxguid.lib",
			};

			if (bUsesWindowsD3D12Libs)
			{
				string DirectXSDKDir = DirectX.GetLibDir(Target);
				PublicAdditionalLibraries.AddRange(AllD3DLibs.Select(LibName => Path.Combine(DirectXSDKDir, LibName)));
			}
			else
			{
				PublicSystemLibraries.AddRange(AllD3DLibs);
			}

			// D3D12Core runtime. Currently x64 only, but ARM64 can also be supported if necessary.
			if (bUsesAgilitySDK)
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
					DirectX.GetDllDir(Target) + "D3D12Core.dll");

				if (Target.Configuration != UnrealTargetConfiguration.Shipping &&
					Target.Configuration != UnrealTargetConfiguration.Test)
				{
					RuntimeDependencies.Add(
						"$(TargetOutputDir)/D3D12/d3d12SDKLayers.dll",
						DirectX.GetDllDir(Target) + "d3d12SDKLayers.dll");
				}
			}
			else
			{
				PublicDefinitions.Add("D3D12_CORE_ENABLED=0");
			}

			// Always delay-load D3D12
			PublicDelayLoadDLLs.Add("d3d12.dll");

			PublicSystemIncludePaths.Add(DirectX.GetIncludeDir(Target));

			PublicDefinitions.Add("D3D12_MAX_DEVICE_INTERFACE=10");
			PublicDefinitions.Add("D3D12_MAX_COMMANDLIST_INTERFACE=6");
			PublicDefinitions.Add("D3D12_SUPPORTS_INFO_QUEUE=1");
			PublicDefinitions.Add("D3D12_SUPPORTS_DXGI_DEBUG=1");
			PublicDefinitions.Add("DXGI_MAX_FACTORY_INTERFACE=6");
			PublicDefinitions.Add("DXGI_MAX_SWAPCHAIN_INTERFACE=4");

			// DX12 extensions, not part of SDK
			PublicSystemIncludePaths.Add(Path.Combine(Target.UEThirdPartySourceDirectory, "Windows", "D3DX12", "include"));
		}
	}
}

