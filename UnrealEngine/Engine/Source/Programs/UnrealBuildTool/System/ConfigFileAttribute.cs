// Copyright Epic Games, Inc. All Rights Reserved.

using System;

namespace UnrealBuildTool
{
	/// <summary>
	/// Attribute indicating a value which should be populated from a UE .ini config file
	/// </summary>
	[AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
	public class ConfigFileAttribute : Attribute
	{
		/// <summary>
		/// Name of the config hierarchy to read from
		/// </summary>
		public ConfigHierarchyType ConfigType;

		/// <summary>
		/// Section containing the setting
		/// </summary>
		public string SectionName;

		/// <summary>
		/// Key name to search for
		/// </summary>
		public string? KeyName;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="ConfigType">Type of the config hierarchy to read from</param>
		/// <param name="SectionName">Section containing the setting</param>
		/// <param name="KeyName">Key name to search for. Optional; uses the name of the field if not set.</param>
		public ConfigFileAttribute(ConfigHierarchyType ConfigType, string SectionName, string? KeyName = null)
		{
			this.ConfigType = ConfigType;
			this.SectionName = SectionName;
			this.KeyName = KeyName;
		}
	}
}
