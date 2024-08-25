// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	/// <summary>
	/// Interface to allow exposing public methods from the toolchain to other assemblies
	/// </summary>
	public interface IAndroidToolChain
	{
		/// <summary>
		/// Returns the Android NDK Level
		/// </summary>
		/// <returns>The NDK Level</returns>
		int GetNdkApiLevelInt(int MinNDK);

		/// <summary>
		/// Returns the Current NDK Version
		/// </summary>
		/// <returns>The NDK Version</returns>
		UInt64 GetNdkVersionInt();
	}

	/// <summary>
	/// Interface to allow exposing public methods from the Android deployment context to other assemblies
	/// </summary>
	public interface IAndroidDeploy
	{
		/// <summary>
		/// 
		/// </summary>
		/// <returns></returns>
		bool GetPackageDataInsideApk();

		/// <summary>
		/// 
		/// </summary>
		/// <param name="Architectures"></param>
		/// <param name="inPluginExtraData"></param>
		void SetAndroidPluginData(UnrealArchitectures Architectures, List<string> inPluginExtraData);

		/// <summary>
		/// 
		/// </summary>
		/// <param name="ProjectFile"></param>
		/// <param name="ProjectName"></param>
		/// <param name="ProjectDirectory"></param>
		/// <param name="ExecutablePath"></param>
		/// <param name="EngineDirectory"></param>
		/// <param name="bForDistribution"></param>
		/// <param name="CookFlavor"></param>
		/// <param name="Configuration"></param>
		/// <param name="bIsDataDeploy"></param>
		/// <param name="bSkipGradleBuild"></param>
		/// <param name="bIsArchive"></param>
		/// <returns></returns>
		bool PrepForUATPackageOrDeploy(FileReference ProjectFile, string ProjectName, DirectoryReference ProjectDirectory, string ExecutablePath, string EngineDirectory, bool bForDistribution, string CookFlavor, UnrealTargetConfiguration Configuration, bool bIsDataDeploy, bool bSkipGradleBuild, bool bIsArchive);

		/// <summary>
		/// 
		/// </summary>
		/// <param name="ProjectName"></param>
		/// <param name="ProjectDirectoryFullName"></param>
		/// <param name="Type"></param>
		/// <param name="bIsEmbedded"></param>
		bool SavePackageInfo(string ProjectName, string ProjectDirectoryFullName, TargetType Type, bool bIsEmbedded);
	}

	/// <summary>
	/// Public Android functions exposed to UAT
	/// </summary>
	public static class AndroidExports
	{
		/// <summary>
		/// 
		/// </summary>
		/// <param name="ProjectFile"></param>
		/// <param name="InForcePackageData"></param>
		/// <returns></returns>
		public static IAndroidDeploy CreateDeploymentHandler(FileReference ProjectFile, bool InForcePackageData)
		{
			return new UEDeployAndroid(ProjectFile, InForcePackageData, Log.Logger);
		}

		/// <summary>
		/// 
		/// </summary>
		/// <returns></returns>
		public static bool ShouldMakeSeparateApks()
		{
			return UEDeployAndroid.ShouldMakeSeparateApks();
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="NDKArch"></param>
		/// <returns></returns>
		public static UnrealArch GetUnrealArch(string NDKArch)
		{
			return UEDeployAndroid.GetUnrealArch(NDKArch);
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="SourceFile"></param>
		/// <param name="TargetFile"></param>
		/// <param name="Logger">Logger for output</param>
		public static void StripSymbols(FileReference SourceFile, FileReference TargetFile, ILogger Logger)
		{
			AndroidToolChain ToolChain = new AndroidToolChain(null, Logger);
			ToolChain.StripSymbols(SourceFile, TargetFile, Logger);
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="ProjectFile"></param>
		/// <param name="bForceDontBundleLibrariesInAPK"></param>
		/// <param name="Configuration"></param>
		/// <param name="bIsArchive"></param>
		/// <param name="bFromMSBuild"></param>
		/// <param name="bIsFromUAT"></param>
		/// <param name="Logger"></param>
		public static bool GetDontBundleLibrariesInAPK(FileReference? ProjectFile, bool? bForceDontBundleLibrariesInAPK, UnrealTargetConfiguration Configuration, bool bIsArchive, bool bFromMSBuild, bool bIsFromUAT, ILogger? Logger)
		{
			return UEDeployAndroid.GetDontBundleLibrariesInAPK(ProjectFile, bForceDontBundleLibrariesInAPK, Configuration, bIsArchive, bFromMSBuild, bIsFromUAT, Logger);
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="Target"></param>
		/// <param name="Logger"></param>
		public static string GetAFSExecutable(UnrealTargetPlatform Target, ILogger Logger)
		{
			return UEDeployAndroid.GetAFSExecutable(Target, Logger);
		}
	}
}
