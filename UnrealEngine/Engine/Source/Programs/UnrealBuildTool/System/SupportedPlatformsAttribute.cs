// Copyright Epic Games, Inc. All Rights Reserved.

using System;

namespace UnrealBuildTool
{
	/// <summary>
	/// Attribute which can be applied to a TargetRules-dervied class to indicate which platforms it supports
	/// </summary>
	[AttributeUsage(AttributeTargets.Class, AllowMultiple = true)]
	public class SupportedPlatformsAttribute : Attribute
	{
		/// <summary>
		/// Array of supported platforms
		/// </summary>
		public readonly UnrealTargetPlatform[] Platforms;

		/// <summary>
		/// Initialize the attribute with a list of platforms
		/// </summary>
		/// <param name="Platforms">Variable-length array of platform arguments</param>
		public SupportedPlatformsAttribute(params string[] Platforms)
		{
			try
			{
				this.Platforms = Array.ConvertAll(Platforms, x => UnrealTargetPlatform.Parse(x));
			}
			catch (BuildException Ex)
			{
				EpicGames.Core.ExceptionUtils.AddContext(Ex, "while parsing a SupportedPlatforms attribute");
				throw;
			}
		}

		/// <summary>
		/// Initialize the attribute with all the platforms in a given category
		/// </summary>
		/// <param name="Category">Category of platforms to add</param>
		public SupportedPlatformsAttribute(UnrealPlatformClass Category)
		{
			Platforms = Utils.GetPlatformsInClass(Category);
		}
	}
}
