// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Security;
using System.Xml.Linq;
using EpicGames.Core;
using UnrealBuildBase;
using System.Runtime.Versioning;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	/// <summary>
	/// Base class for platform-specific project generators
	/// </summary>
	[SupportedOSPlatform("windows")]
	class HoloLensProjectGenerator : PlatformProjectGenerator
	{
		const string PlatformString = "HoloLens";

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Arguments">Command line arguments passed to the project generator</param>
		/// <param name="Logger"></param>
		public HoloLensProjectGenerator(CommandLineArguments Arguments, ILogger Logger)
			: base(Arguments, Logger)
		{
		}

		/// <summary>
		/// Enumerate all the platforms that this generator supports
		/// </summary>
		public override IEnumerable<UnrealTargetPlatform> GetPlatforms()
		{
			yield return UnrealTargetPlatform.HoloLens;
		}

		///
		///	VisualStudio project generation functions
		///	
		/// <summary>
		/// Whether this build platform has native support for VisualStudio
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration"> The UnrealTargetConfiguration being built</param>
		/// <param name="ProjectFileFormat"></param>
		/// <returns>bool    true if native VisualStudio support (or custom VSI) is available</returns>
		public override bool HasVisualStudioSupport(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, VCProjectFileFormat ProjectFileFormat)
		{
			return true;
		}

		/// <summary>
		/// Get whether this platform deploys
		/// </summary>
		/// <returns>bool  true if the 'Deploy' option should be enabled</returns>
		public override bool GetVisualStudioDeploymentEnabled(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return true;
		}

		public override void GenerateGameProperties(UnrealTargetConfiguration Configuration, StringBuilder VCProjectFileContent, TargetType TargetType, DirectoryReference RootDirectory, FileReference TargetFilePath)
		{
			string MinVersion = string.Empty;
			string MaxTestedVersion = string.Empty;
			ConfigHierarchy EngineIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, RootDirectory, UnrealTargetPlatform.HoloLens);
			if (EngineIni != null)
			{
				EngineIni.GetString("/Script/HoloLensPlatformEditor.HoloLensTargetSettings", "MinimumPlatformVersion", out MinVersion);
				EngineIni.GetString("/Script/HoloLensPlatformEditor.HoloLensTargetSettings", "MaximumPlatformVersionTested", out MaxTestedVersion);
			}
			if (!string.IsNullOrEmpty(MinVersion))
			{
				VCProjectFileContent.Append("		<WindowsTargetPlatformMinVersion>" + MinVersion + "</WindowsTargetPlatformMinVersion>" + ProjectFileGenerator.NewLine);
			}
			if (!string.IsNullOrEmpty(MaxTestedVersion))
			{
				VCProjectFileContent.Append("		<WindowsTargetPlatformVersion>" + MaxTestedVersion + "</WindowsTargetPlatformVersion>" + ProjectFileGenerator.NewLine);
			}

			WindowsCompiler Compiler = WindowsPlatform.GetDefaultCompiler(TargetFilePath, WindowsArchitecture.x64, Logger);
			DirectoryReference? PlatformWinMDLocation = HoloLensPlatform.GetCppCXMetadataLocation(Compiler, "Latest", WindowsArchitecture.x64, Logger);
			if (PlatformWinMDLocation == null || !FileReference.Exists(FileReference.Combine(PlatformWinMDLocation, "platform.winmd")))
			{
				Logger.LogWarning("Unable to find platform.winmd for {ToolChain} toolchain; '{Version}' is an invalid version", WindowsPlatform.GetCompilerName(Compiler), "Latest");
			}
			string FoundationWinMDPath = HoloLensPlatform.GetLatestMetadataPathForApiContract("Windows.Foundation.FoundationContract", Compiler, Logger);
			string UniversalWinMDPath = HoloLensPlatform.GetLatestMetadataPathForApiContract("Windows.Foundation.UniversalApiContract", Compiler, Logger);
			VCProjectFileContent.Append("		<AdditionalOptions>/ZW /ZW:nostdlib</AdditionalOptions>" + ProjectFileGenerator.NewLine);
			VCProjectFileContent.Append("		<NMakePreprocessorDefinitions>$(NMakePreprocessorDefinitions);PLATFORM_HOLOLENS=1;HOLOLENS=1;</NMakePreprocessorDefinitions>" + ProjectFileGenerator.NewLine);
			if (PlatformWinMDLocation != null)
			{
				VCProjectFileContent.Append("		<NMakeAssemblySearchPath>$(NMakeAssemblySearchPath);" + PlatformWinMDLocation + "</NMakeAssemblySearchPath>" + ProjectFileGenerator.NewLine);
			}
			VCProjectFileContent.Append("		<NMakeForcedUsingAssemblies>$(NMakeForcedUsingAssemblies);" + FoundationWinMDPath + ";" + UniversalWinMDPath + ";platform.winmd</NMakeForcedUsingAssemblies>" + ProjectFileGenerator.NewLine);
		}

		public override void GetVisualStudioPreDefaultString(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, StringBuilder VCProjectFileContent)
		{
			// VS2017 expects WindowsTargetPlatformVersion to be set in conjunction with these other properties, otherwise the projects
			// will fail to load when the solution is in a HoloLens configuration.
			// Default to latest supported version.  Game projects can override this later.
			// Because this property is only required for VS2017 we can safely say that's the compiler version (whether that's actually true
			// or not)
			//string SDKFolder = "";
			string SDKVersion = "";

			DirectoryReference? folder;
			VersionNumber? version;
			if(WindowsPlatform.TryGetWindowsSdkDir("Latest", Logger, out version, out folder))
			{
				//SDKFolder = folder.FullName;
				SDKVersion = version.ToString();
			}

			VCProjectFileContent.AppendLine("		<AppContainerApplication>true</AppContainerApplication>");
			VCProjectFileContent.AppendLine("		<ApplicationType>Windows Store</ApplicationType>");
			VCProjectFileContent.AppendLine("		<ApplicationTypeRevision>10.0</ApplicationTypeRevision>");
			VCProjectFileContent.AppendLine("		<WindowsAppContainer>true</WindowsAppContainer>");
			VCProjectFileContent.AppendLine("		<AppxPackage>true</AppxPackage>");
			VCProjectFileContent.AppendLine("		<WindowsTargetPlatformVersion>{0}</WindowsTargetPlatformVersion>", SDKVersion.ToString()); 
		}

		public override string GetVisualStudioLayoutDirSection(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, string InConditionString, TargetType TargetType, FileReference TargetRulesPath, FileReference ProjectFilePath, FileReference NMakeOutputPath, VCProjectFileFormat InProjectFileFormat)
		{
			string LayoutDirString = "";

			if (IsValidHoloLensTarget(InPlatform, TargetType, TargetRulesPath))
			{
				LayoutDirString += "	<PropertyGroup " + InConditionString + ">" + ProjectFileGenerator.NewLine;
				LayoutDirString += "		<RemoveExtraDeployFiles>false</RemoveExtraDeployFiles>" + ProjectFileGenerator.NewLine;
				LayoutDirString += "		<LayoutDir>" + SecurityElement.Escape(DirectoryReference.Combine(NMakeOutputPath.Directory, "AppX").FullName) + "</LayoutDir>" + ProjectFileGenerator.NewLine;
				LayoutDirString += "		<AppxPackageRecipe>" + SecurityElement.Escape(FileReference.Combine(NMakeOutputPath.Directory, ProjectFilePath.GetFileNameWithoutExtension() + ".build.appxrecipe").FullName) + "</AppxPackageRecipe>" + ProjectFileGenerator.NewLine;
				LayoutDirString += "	</PropertyGroup>" + ProjectFileGenerator.NewLine;

				// another hijack - this is a convenient point to make sure that HoloLens-appropriate debuggers are available
				// in the project property pages.
				LayoutDirString += "    <ItemGroup " + InConditionString + ">" + ProjectFileGenerator.NewLine;
				LayoutDirString += "		<PropertyPageSchema Include=\"$(VCTargetsPath)$(LangID)\\AppHostDebugger_Local.xml\" />" + ProjectFileGenerator.NewLine;
				LayoutDirString += "		<PropertyPageSchema Include=\"$(VCTargetsPath)$(LangID)\\AppHostDebugger_Simulator.xml\" />" + ProjectFileGenerator.NewLine;
				LayoutDirString += "		<PropertyPageSchema Include=\"$(VCTargetsPath)$(LangID)\\AppHostDebugger_Remote.xml\" />" + ProjectFileGenerator.NewLine;
				LayoutDirString += "    </ItemGroup>" + ProjectFileGenerator.NewLine;
			}

			return LayoutDirString;
		}

		///
		///	VisualStudio project generation functions
		///	
		/// <summary>
		/// Return the VisualStudio platform name for this build platform
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration"> The UnrealTargetConfiguration being built</param>
		/// <returns>string    The name of the platform that VisualStudio recognizes</returns>
		public override string GetVisualStudioPlatformName(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			if (InPlatform == UnrealTargetPlatform.HoloLens)
			{
				return "arm64";
			}
			return InPlatform.ToString();
		}

		private bool IsValidHoloLensTarget(UnrealTargetPlatform InPlatform, TargetType InTargetType, FileReference InTargetFilePath)
		{
			if ((InPlatform == UnrealTargetPlatform.HoloLens) &&
				(InTargetType == TargetRules.TargetType.Client || InTargetType == TargetRules.TargetType.Game ) &&
				InTargetType != TargetRules.TargetType.Editor &&
				InTargetType != TargetRules.TargetType.Server
				)
			{
				// We do not want to include any Templates targets
				// Not a huge fan of doing it via path name comparisons... but it works
				string TempTargetFilePath = InTargetFilePath.FullName.Replace("\\", "/");
				if (TempTargetFilePath.Contains("/Templates/"))
				{
					string AbsoluteEnginePath = Unreal.EngineDirectory.FullName;
					AbsoluteEnginePath = AbsoluteEnginePath.Replace("\\", "/");
					if (AbsoluteEnginePath.EndsWith("/") == false)
					{
						AbsoluteEnginePath += "/";
					}
					string CheckPath = AbsoluteEnginePath.Replace("/Engine/", "/Templates/");
					if (TempTargetFilePath.StartsWith(CheckPath))
					{
						return false;
					}
				}
				return true;
			}

			return false;
		}

		public override bool RequiresVSUserFileGeneration()
		{
			return true;
		}

		public override string GetVisualStudioUserFileStrings(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration,
			string InConditionString, TargetRules InTargetRules, FileReference TargetRulesPath, FileReference ProjectFilePath)
		{
			string UserFileEntry = "";
			if (IsValidHoloLensTarget(InPlatform, InTargetRules.Type, TargetRulesPath))
			{
				UserFileEntry += "<PropertyGroup " + InConditionString + ">\n";
				UserFileEntry += "	<DebuggerFlavor>AppHostLocalDebugger</DebuggerFlavor>\n";
				UserFileEntry += "</PropertyGroup>\n";
			}
			return UserFileEntry;
		}
	}
}

