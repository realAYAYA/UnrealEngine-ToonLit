// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	/// <summary>
	/// WinMD registration helper
	/// </summary>
	public class WinMDRegistrationInfo
	{
		/// <summary>
		/// WinMD type info
		/// </summary>
		public class ActivatableType
		{
			/// <summary>
			/// WinMD type info constructor
			/// </summary>
			/// <param name="InTypeName"></param>
			/// <param name="InThreadingModelName"></param>
			public ActivatableType(string InTypeName, string InThreadingModelName)
			{
				TypeName = InTypeName;
				ThreadingModelName = InThreadingModelName;
			}

			/// <summary>
			/// Type Name
			/// </summary>
			public string TypeName { get; private set; }

			/// <summary>
			/// Threading model
			/// </summary>
			public string ThreadingModelName { get; private set; }
		}

		private static Version FindLatestVersionDirectory(string InDirectory, Version? NoLaterThan)
		{
			Version LatestVersion = new Version(0, 0, 0, 0);
			if (Directory.Exists(InDirectory))
			{
				string[] VersionDirectories = Directory.GetDirectories(InDirectory);
				foreach (string Dir in VersionDirectories)
				{
					string VersionString = Path.GetFileName(Dir);
					Version? FoundVersion;
					if (Version.TryParse(VersionString, out FoundVersion) && FoundVersion > LatestVersion)
					{
						if (NoLaterThan == null || FoundVersion <= NoLaterThan)
						{
							LatestVersion = FoundVersion;
						}
					}
				}
			}
			return LatestVersion;
		}

		[SupportedOSPlatform("windows")]
		private List<string> ExpandWinMDReferences(string SdkVersion, string[] WinMDReferences)
		{
			// Allow bringing in Windows SDK contracts just by naming the contract
			// These are files that look like References/10.0.98765.0/AMadeUpWindowsApiContract/5.0.0.0/AMadeUpWindowsApiContract.winmd
			List<string> ExpandedWinMDReferences = new List<string>();

			VersionNumber? OutSdkVersion;
			DirectoryReference? OutSdkDir;
			if (!WindowsPlatform.TryGetWindowsSdkDir(SdkVersion, Logger, out OutSdkVersion, out OutSdkDir))
			{
				Logger.LogError("Failed to find WinSDK {SdkVersion}", SdkVersion);
				throw new Exception("Failed to find WinSDK");
			}

			// The first few releases of the Windows 10 SDK didn't put the SDK version in the reference path
			string ReferenceRoot = Path.Combine(OutSdkDir.FullName, "References");
			string VersionedReferenceRoot = Path.Combine(ReferenceRoot, OutSdkVersion.ToString());
			if (Directory.Exists(VersionedReferenceRoot))
			{
				ReferenceRoot = VersionedReferenceRoot;
			}

			foreach (string WinMDRef in WinMDReferences)
			{
				if (File.Exists(WinMDRef))
				{
					// Already a valid path
					ExpandedWinMDReferences.Add(WinMDRef);
				}
				else
				{
					string ContractFolder = Path.Combine(ReferenceRoot, WinMDRef);

					Version ContractVersion = FindLatestVersionDirectory(ContractFolder, null);
					string ExpandedWinMDRef = Path.Combine(ContractFolder, ContractVersion.ToString(), WinMDRef + ".winmd");
					if (File.Exists(ExpandedWinMDRef))
					{
						ExpandedWinMDReferences.Add(ExpandedWinMDRef);
					}
					else
					{
						Logger.LogWarning("Unable to resolve location for HoloLens WinMD api contract {Contract}, file {File}", WinMDRef, ExpandedWinMDRef);
					}
				}
			}

			return ExpandedWinMDReferences;
		}

		private readonly ILogger Logger;

		/// <summary>
		/// WinMD reference info
		/// </summary>
		/// <param name="InWindMDSourcePath"></param>
		/// <param name="InPackageRelativeDllPath"></param>
		/// <param name="SdkVersion"></param>
		/// <param name="InLogger"></param>
		[SupportedOSPlatform("windows")]
		public WinMDRegistrationInfo(FileReference InWindMDSourcePath, string InPackageRelativeDllPath, string SdkVersion, ILogger InLogger)
		{
			Logger = InLogger;

			PackageRelativeDllPath = InPackageRelativeDllPath;
			ResolveSearchPaths.Add(InWindMDSourcePath.Directory.FullName);
			List<string> WinMDAssemblies = ExpandWinMDReferences(SdkVersion, new string[] {
				"Windows.Foundation.FoundationContract",
				"Windows.Foundation.UniversalApiContract"
			});
			string[] runtimeAssemblies = Directory.GetFiles(RuntimeEnvironment.GetRuntimeDirectory(), "*.dll");
			// Create the list of assembly paths consisting of runtime assemblies and the input file.
			List<string> AssemblyPaths = new List<string>(runtimeAssemblies);
			AssemblyPaths.AddRange(WinMDAssemblies);
			AssemblyPaths.Add(InWindMDSourcePath.FullName);

			// Create MetadataLoadContext that can resolve assemblies using the created list.
			PathAssemblyResolver PathAssemblyResolver = new PathAssemblyResolver(AssemblyPaths);
			MetadataLoadContext Mlc = new MetadataLoadContext(PathAssemblyResolver);

			using (Mlc)
			{
				ActivatableTypesList = new List<ActivatableType>();
				Assembly DependsOn = Mlc.LoadFromAssemblyPath(InWindMDSourcePath.FullName);
				foreach (Type WinMDType in DependsOn.GetExportedTypes())
				{
					bool IsActivatable = false;
					string ThreadingModelName = "both";
					foreach (CustomAttributeData Attr in WinMDType.CustomAttributes)
					{
						if (Attr.AttributeType.FullName == "Windows.Foundation.Metadata.ActivatableAttribute" ||
						    Attr.AttributeType.FullName == "Windows.Foundation.Metadata.StaticAttribute")
						{
							IsActivatable = true;
						}
						else if (Attr.AttributeType.FullName == "Windows.Foundation.Metadata.ThreadingAttribute")
						{
							CustomAttributeTypedArgument Argument = Attr.ConstructorArguments[0];
							ThreadingModelName = Enum.GetName(Argument.ArgumentType, Argument.Value!)!.ToLowerInvariant();
						}
					}
					if (IsActivatable)
					{
						ActivatableTypesList.Add(new ActivatableType(WinMDType.FullName!, ThreadingModelName));
					}
				}
			}
		}

		/// <summary>
		/// Path to the WinRT library
		/// </summary>
		public string PackageRelativeDllPath { get; private set; }

		/// <summary>
		/// List of the types in the WinMD
		/// </summary>
		public IEnumerable<ActivatableType> ActivatableTypes
		{
			get
			{
				return ActivatableTypesList;
			}
		}

		private static List<string> ResolveSearchPaths = new List<string>();
		private List<ActivatableType> ActivatableTypesList;
	}
}
