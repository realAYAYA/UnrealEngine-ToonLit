// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	[Serializable]
	class IOSPostBuildSyncTarget
	{
		public UnrealTargetPlatform Platform;
		public UnrealArchitectures Architectures;
		public UnrealTargetConfiguration Configuration;
		public FileReference? ProjectFile;
		public string TargetName;
		public TargetType TargetType;
		public FileReference OutputPath;
		public List<string> UPLScripts;
		public VersionNumber SdkVersion;
		public bool bCreateStubIPA;
		public bool bSkipCrashlytics;
		public DirectoryReference ProjectDirectory;
		public DirectoryReference? ProjectIntermediateDirectory;
		public string? ImportProvision;
		public string? ImportCertificate;
		public string? ImportCertificatePassword;
		public Dictionary<string, DirectoryReference> FrameworkNameToSourceDir;
		public bool bForDistribution = false;
		public bool bBuildAsFramework = false;

		public IOSPostBuildSyncTarget(ReadOnlyTargetRules Target, FileReference OutputPath, DirectoryReference? ProjectIntermediateDirectory, List<string> UPLScripts, VersionNumber SdkVersion, Dictionary<string, DirectoryReference> FrameworkNameToSourceDir)
		{
			Platform = Target.Platform;
			Architectures = Target.Architectures;
			Configuration = Target.Configuration;
			ProjectFile = Target.ProjectFile;
			TargetName = Target.Name;
			TargetType = Target.Type;
			this.OutputPath = OutputPath;
			this.UPLScripts = UPLScripts;
			this.SdkVersion = SdkVersion;
			bCreateStubIPA = Target.IOSPlatform.bCreateStubIPA;
			bSkipCrashlytics = Target.IOSPlatform.bSkipCrashlytics;
			ProjectDirectory = DirectoryReference.FromFile(Target.ProjectFile) ?? Unreal.EngineDirectory;
			this.ProjectIntermediateDirectory = ProjectIntermediateDirectory;
			ImportProvision = Target.IOSPlatform.ImportProvision;
			ImportCertificate = Target.IOSPlatform.ImportCertificate;
			ImportCertificatePassword = Target.IOSPlatform.ImportCertificatePassword;
			this.FrameworkNameToSourceDir = FrameworkNameToSourceDir;
			bForDistribution = Target.IOSPlatform.bForDistribution;
			bBuildAsFramework = Target.bShouldCompileAsDLL;
		}
	}

	[ToolMode("IOSPostBuildSync", ToolModeOptions.XmlConfig | ToolModeOptions.BuildPlatforms)]
	class IOSPostBuildSyncMode : ToolMode
	{
		[CommandLine("-Input=", Required = true)]
		public FileReference? InputFile = null;

		[CommandLine("-XmlConfigCache=")]
		public FileReference? XmlConfigCache = null;

		// this isn't actually used, but is helpful to pass -legacyxcode along in CreatePostBuildSyncAction, and UBT won't
		// complain that nothing is using it, because where we _do_ use it is outside the normal cmdline parsing functionality
		[CommandLine("-LegacyXcode")]
		public bool bLegacyXcode;

		public override Task<int> ExecuteAsync(CommandLineArguments Arguments, ILogger Logger)
		{
			Arguments.ApplyTo(this);
			Arguments.CheckAllArgumentsUsed();

			IOSToolChainSettings.SelectXcode(false, Logger);

			// Run the PostBuildSync command
			IOSPostBuildSyncTarget Target = BinaryFormatterUtils.Load<IOSPostBuildSyncTarget>(InputFile!);
			IOSToolChain.PostBuildSync(Target, Logger);

			return Task.FromResult(0);
		}
	}
}
