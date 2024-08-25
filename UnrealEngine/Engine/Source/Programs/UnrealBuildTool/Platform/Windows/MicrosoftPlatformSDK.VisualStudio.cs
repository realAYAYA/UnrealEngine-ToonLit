// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using EpicGames.Core;

namespace UnrealBuildTool
{
	partial class MicrosoftPlatformSDK : UEBuildPlatformSDK
	{
		/// <summary>
		/// The default set of components that should be suggested to be installed for Visual Studio
		/// This or the 2022 specific components should be updated if the preferred visual cpp version changes
		/// </summary>
		static readonly string[] s_visualStudioSuggestedComponents = SDK.GetStringArrayFromConfig("VisualStudioSuggestedComponents");

		/// <summary>
		/// Additional set of components that should be suggested to be installed for Visual Studio
		/// to support the Linux platform.
		/// </summary>
		static readonly string[] s_visualStudioSuggestedLinuxComponents = SDK.GetStringArrayFromConfig("VisualStudioSuggestedLinuxComponents");
		/// <summary>
		/// Additional set of components that should be suggested to be installed for Visual Studio 2022.
		/// </summary>
		static readonly string[] s_visualStudio2022SuggestedComponents = SDK.GetStringArrayFromConfig("VisualStudio2022SuggestedComponents");

		/// <summary>
		/// Returns the list of suggested of components that should be suggested to be installed for Visual Studio.
		/// Used to generate a .vsconfig file which will prompt Visual Studio to ask the user to install these components.
		/// </summary>
		public static IEnumerable<string> GetVisualStudioSuggestedComponents(VCProjectFileFormat format)
		{
			bool platformLinuxValid = (InstalledPlatformInfo.IsValidPlatform(UnrealTargetPlatform.Linux) && UEBuildPlatform.IsPlatformAvailable(UnrealTargetPlatform.Linux))
				|| (InstalledPlatformInfo.IsValidPlatform(UnrealTargetPlatform.LinuxArm64) && UEBuildPlatform.IsPlatformAvailable(UnrealTargetPlatform.LinuxArm64));

			SortedSet<string> components = new(s_visualStudioSuggestedComponents);

			switch (format)
			{
				case VCProjectFileFormat.VisualStudio2022:
					components.UnionWith(s_visualStudio2022SuggestedComponents);
					break;
				default:
					throw new BuildException("Unsupported Visual Studio version {0}", format);
			}

			if (platformLinuxValid)
			{
				components.UnionWith(s_visualStudioSuggestedLinuxComponents);
			}

			return components;
		}
	}
}
