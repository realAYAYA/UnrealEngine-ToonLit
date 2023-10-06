// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;

namespace UnrealBuildTool
{
	/// <summary>
	/// Attribute which can be applied to a TargetRules-dervied class to indicate which platforms it supports
	/// </summary>
	[AttributeUsage(AttributeTargets.Class, AllowMultiple = true)]
	public class SupportedPlatformGroupsAttribute : SupportedPlatformsAttribute
	{
		/// <summary>
		/// Initialize the attribute with a list of platform groups
		/// </summary>
		/// <param name="PlatformGroups">Variable-length array of platform group arguments</param>
		public SupportedPlatformGroupsAttribute(params string[] PlatformGroups) : base(GetPlatformsForGroups(PlatformGroups))
		{
		}

		private static string[] GetPlatformsForGroups(params string[] PlatformGroups)
		{
			HashSet<UnrealTargetPlatform> SupportedPlatforms = new();
			try
			{
				foreach (string Name in PlatformGroups)
				{
					if (UnrealPlatformGroup.TryParse(Name, out UnrealPlatformGroup Group))
					{
						SupportedPlatforms.UnionWith(UnrealTargetPlatform.GetValidPlatforms().Where(x => x.IsInGroup(Group)));
						continue;
					}
					throw new BuildException(String.Format("The platform group name {0} is not a valid platform group name. Valid names are ({1})", Name,
						String.Join(",", UnrealPlatformGroup.GetValidGroupNames())));
				}
			}
			catch (BuildException Ex)
			{
				EpicGames.Core.ExceptionUtils.AddContext(Ex, $"while parsing a SupportedPlatformGroups attribute '{String.Join(',', PlatformGroups)}'");
				throw;
			}

			return SupportedPlatforms.Select(x => x.ToString()).ToArray();
		}
	}
}
