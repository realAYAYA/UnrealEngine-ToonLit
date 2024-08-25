// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.IO;
using System.Diagnostics;
using System.Collections.Generic;
using UnrealBuildTool;

public class Python3 : ModuleRules
{
	public Python3(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// if the target doesn't want python support, disable it in C++ via define and do nothing else
		// we could check this at a higher level and not even include this module, but the code is already setup to
		// disable Python support via this module
		if (!Target.bCompilePython)
		{
			PublicDefinitions.Add("WITH_PYTHON=0");
			return;
		}

		var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

		PythonSDKPaths PythonSDK = null;

		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.Linux)
		{
			// Check if an explicit version of Python was set by the user before using the auto-detection logic. This allow building the engine against
			// a user specified Python SDK. WARNING: Ensure to specify a 64-bit version of Python, especially on Windows where the 32-bit version is still available.
			var PythonRoot = System.Environment.GetEnvironmentVariable("UE_PYTHON_DIR");
			if (PythonRoot != null)
			{
				PythonSDK = DiscoverPythonSDK(PythonRoot);
				if (!PythonSDK.IsValid())
				{
					PythonSDK = null;
				}
			}
		}

		// Perform auto-detection to try and find the Python SDK
		if (PythonSDK == null)
		{
			// Get a list of installed Python SDKs from a set of known installation paths and use the first valid one in the returned list. By default, the first SDK in the returned
			// list is the one shipped with the engine. Note: It is preferred to set UE_PYTHON_DIR environment variable to use a custom version of Python.
			var PotentialSDKs = GetPotentialPythonSDKs(Target);
			foreach (var PotentialSDK in PotentialSDKs)
			{
				if (PotentialSDK.IsValid())
				{
					PythonSDK = PotentialSDK;
					break;
				}
			}
		}

		if (PythonSDK == null)
		{
			PublicDefinitions.Add("WITH_PYTHON=0");
			Console.WriteLine("Python SDK not found");
		}
		else
		{
			// If the Python install we're using is within the Engine directory, make the path relative so that it's portable
			string EngineRelativePythonRoot = PythonSDK.PythonRoot;
			var IsEnginePython = EngineRelativePythonRoot.StartsWith(EngineDir);
			if (IsEnginePython)
			{
				// Strip the Engine directory and then combine the path with the placeholder to ensure the path is delimited correctly
				EngineRelativePythonRoot = EngineRelativePythonRoot.Remove(0, EngineDir.Length);
				foreach(string FileName in Directory.EnumerateFiles(PythonSDK.PythonRoot, "*", SearchOption.AllDirectories))
				{
					if(!FileName.EndsWith(".pyc", System.StringComparison.OrdinalIgnoreCase))
					{
						RuntimeDependencies.Add(FileName);
					}
				}
				EngineRelativePythonRoot = Path.Combine("{ENGINE_DIR}", EngineRelativePythonRoot); // Can't use $(EngineDir) as the placeholder here as UBT is eating it
			}

			PublicDefinitions.Add("WITH_PYTHON=1");
			PublicDefinitions.Add(string.Format("UE_PYTHON_DIR=\"{0}\"", EngineRelativePythonRoot.Replace('\\', '/')));

			// Some versions of Python need this define set when building on MSVC
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				PublicDefinitions.Add("HAVE_ROUND=1");
			}

			PublicSystemIncludePaths.AddRange(PythonSDK.PythonIncludePaths);
			PublicAdditionalLibraries.AddRange(PythonSDK.PythonLibs);
			AppendPythonRuntimeDependencies(Target, IsEnginePython);
		}
	}

	/// <summary>
	/// Returns a list of existing PythonSDK by scanning a set of known paths for the specified target. By default, the
	/// function puts the Python SDK shipped with the engine first (as the preferred SDK). If a user wants to build against
	/// another Python SDK, the list below can be manually modified. The preferred way to override the Python SDK would be
	/// to set the UE_PYTHON_DIR environment variable though it might be move convenient for some users to auto-discover
	/// from a common install path. Ensure to install, link and use the 64-bit version of Python with Unreal Engine.
	/// </summary>
	/// <param name="Target"></param>
	/// <returns>
	/// The list of potential SDKs. The first valid one in the list is going to be used. This is typically the one
	/// shipped with the engine.
	/// </returns>
	private List<PythonSDKPaths> GetPotentialPythonSDKs(ReadOnlyTargetRules Target)
	{
		var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

		// The Python SDK shipped with the Engine.
		var PythonBinaryTPSDir = Path.Combine(EngineDir, "Binaries", "ThirdParty", "Python3");
		var PythonSourceTPSDir = Path.Combine(EngineDir, "Source", "ThirdParty", "Python3");

		var PotentialSDKs = new List<PythonSDKPaths>();

		// todo: This isn't correct for cross-compilation, we need to consider the host platform too
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			var PlatformDir = "Win64";

			PotentialSDKs.AddRange(
				new PythonSDKPaths[] {
					new PythonSDKPaths(Path.Combine(PythonBinaryTPSDir, PlatformDir), new List<string>() { Path.Combine(PythonSourceTPSDir, PlatformDir, "include") }, new List<string>() { Path.Combine(PythonSourceTPSDir, PlatformDir, "libs", "python311.lib") }),

					// If you uncomment/add a discovery path here, ensure to discover the 64-bit version of Python. As of Python 3.9.7, the 32-bit version still available on Windows (and will crash the engine if used). To avoid editing this file, use UE_PYTHON_DIR environment variable.
					//DiscoverPythonSDK("C:/Program Files/Python311"),
					//DiscoverPythonSDK("C:/Python311"),
				}
			);
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PotentialSDKs.AddRange(
				new PythonSDKPaths[] {
					new PythonSDKPaths(
						Path.Combine(PythonBinaryTPSDir, "Mac"),
						new List<string>() {
							Path.Combine(PythonSourceTPSDir, "Mac", "include")
						},
						new List<string>() {
							Path.Combine(PythonBinaryTPSDir, "Mac", "lib", "libpython3.11.dylib")
						}),
				}
			);
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			if (Target.Architecture == UnrealArch.X64)
			{
				var PlatformDir = Target.Platform.ToString();
		
				PotentialSDKs.AddRange(
					new PythonSDKPaths[] {
						new PythonSDKPaths(
							Path.Combine(PythonBinaryTPSDir, PlatformDir),
							new List<string>() {
								Path.Combine(PythonSourceTPSDir, PlatformDir, "include")
							},
							new List<string>() { Path.Combine(PythonSourceTPSDir, PlatformDir, "lib", "libpython3.11.a") }),
				});
				PublicSystemLibraries.Add("util");	// part of libc
			}
		}
		
		return PotentialSDKs;
	}
	
	private void AppendPythonRuntimeDependencies(ReadOnlyTargetRules Target, bool IsEnginePython)
	{
		if (Target.Platform == UnrealTargetPlatform.Linux && IsEnginePython)
		{
			RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/Python3/Linux/lib/libpython3.11.so.1.0");
		}

		// Copy python dll alongside the target in monolithic builds. We statically link a python stub that triggers the dll
		// load at global startup, before the paths are configured to find this dll in its native location. By copying it alongside
		// the executable we can guarantee it will be found and loaded
		if (Target.Platform == UnrealTargetPlatform.Win64 && Target.LinkType == TargetLinkType.Monolithic && IsEnginePython)
		{
			RuntimeDependencies.Add("$(TargetOutputDir)/python311.dll", "$(EngineDir)/Binaries/ThirdParty/Python3/Win64/python311.dll", StagedFileType.NonUFS);
		}
	}

	private PythonSDKPaths DiscoverPythonSDK(string InPythonRoot)
	{
		string PythonRoot = InPythonRoot;
		List<string> PythonIncludePaths = null;
		List<string> PythonLibs = null;

		// Work out the include path
		if (PythonRoot != null)
		{
			var PythonIncludePath = Path.Combine(PythonRoot, "include");
			if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				// On Mac the actual headers are inside a "pythonxy" directory, where x and y are the version number
				if (Directory.Exists(PythonIncludePath))
				{
					string[] MatchingIncludePaths = Directory.GetDirectories(PythonIncludePath, "python*");
					if (MatchingIncludePaths.Length > 0)
					{
						PythonIncludePath = Path.Combine(PythonIncludePath, Path.GetFileName(MatchingIncludePaths[0]));
					}
				}
			}
			if (Directory.Exists(PythonIncludePath))
			{
				PythonIncludePaths = new List<string> { PythonIncludePath };
			}
			else
			{
				PythonRoot = null;
			}
		}

		// Work out the lib path
		if (PythonRoot != null)
		{
			string LibFolder = null;
			string LibNamePattern = null;
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				LibFolder = "libs";
				LibNamePattern = "python311.lib";
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				LibFolder = "lib";
				LibNamePattern = "libpython3.11.dylib";
			}
			else if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				LibFolder = "lib";
				LibNamePattern = "libpython3.11.so";
			}

			if (LibFolder != null && LibNamePattern != null)
			{
				var PythonLibPath = Path.Combine(PythonRoot, LibFolder);

				if (Directory.Exists(PythonLibPath))
				{
					string[] MatchingLibFiles = Directory.GetFiles(PythonLibPath, LibNamePattern);
					if (MatchingLibFiles.Length > 0)
					{
						PythonLibs = new List<string>();
						foreach (var MatchingLibFile in MatchingLibFiles)
						{
							PythonLibs.Add(MatchingLibFile);
						}
					}
				}
			}

			if (PythonLibs == null)
			{
				PythonRoot = null;
			}
		}

		return new PythonSDKPaths(PythonRoot, PythonIncludePaths, PythonLibs);
	}

	private class PythonSDKPaths
	{
		public PythonSDKPaths(string InPythonRoot, List<string> InPythonIncludePaths, List<string> InPythonLibs)
		{
			PythonRoot = InPythonRoot;
			PythonIncludePaths = InPythonIncludePaths;
			PythonLibs = InPythonLibs;
		}

		public bool IsValid()
		{
			return PythonRoot != null && Directory.Exists(PythonRoot);
		}

		public string PythonRoot;
		public List<string> PythonIncludePaths;
		public List<string> PythonLibs;
	};
}
