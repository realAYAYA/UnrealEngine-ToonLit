// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Text.RegularExpressions;
using EpicGames.Core;
using AutomationTool;
using UnrealBuildTool;

namespace AutomationUtils.Automation
{
	public class BundleUtils
	{
		public class BundleSettings
		{
			public string Name { get; set; }
			public List<string> Tags { get; set; }
			public List<string> Dependencies { get; set; }
			public List<string> FileRegex { get; set; }
			public List<string> Files { get; set; }
			public bool bFoundParent { get; set; }
			public bool bContainsShaderLibrary { get; set; }
			public int Order { get; set; }
			public bool	UseChunkDBs { get; set; }
			public bool UseDetailedInstallSizes { get; set; } // default is true
			public string ExecFileName { get; set; } // TODO: We never used this.  Clean this up.
		}

		public static void LoadBundleConfig(DirectoryReference ProjectDir, UnrealTargetPlatform Platform, out IReadOnlyDictionary<string, BundleSettings> Bundles)
		{
			LoadBundleConfig<BundleSettings>(ProjectDir, Platform, out Bundles, delegate (BundleSettings Settings, ConfigHierarchy BundleConfig, string Section) { });
		}

		public static void LoadBundleConfig<TPlatformBundleSettings>(DirectoryReference ProjectDir, UnrealTargetPlatform Platform, 
			out IReadOnlyDictionary<string, TPlatformBundleSettings> Bundles, 
			Action<TPlatformBundleSettings, ConfigHierarchy, string> GetPlatformSettings) 
			where TPlatformBundleSettings : BundleSettings, new()
		{
			var Results = new List<TPlatformBundleSettings>();

			ConfigHierarchy BundleConfig = ConfigCache.ReadHierarchy(ConfigHierarchyType.InstallBundle, ProjectDir, Platform);

			const string BundleDefinitionPrefix = "InstallBundleDefinition ";

			foreach (string SectionName in BundleConfig.SectionNames)
			{
				if (!SectionName.StartsWith(BundleDefinitionPrefix))
					continue;

				TPlatformBundleSettings Bundle = new TPlatformBundleSettings();
				Bundle.Name = SectionName.Substring(BundleDefinitionPrefix.Length);
				{
					int Order;
					if(BundleConfig.GetInt32(SectionName, "Order", out Order))
					{
						Bundle.Order = Order;
					}
					else
					{
						Bundle.Order = int.MaxValue;
					}
				}
				{
					List<string> Tags;
					if (BundleConfig.GetArray(SectionName, "Tags", out Tags))
					{
						Bundle.Tags = Tags;
					}
					else 
					{
						Bundle.Tags = new List<string>(); 
					}
				}
				{
					List<string> Dependencies;
					if (BundleConfig.GetArray(SectionName, "Dependencies", out Dependencies))
					{
						Bundle.Dependencies = Dependencies;
					}
					else
					{
						Bundle.Dependencies = new List<string>();
					}
				}
				{
					List<string> FileRegex;
					if (BundleConfig.GetArray(SectionName, "FileRegex", out FileRegex))
					{
						Bundle.FileRegex = FileRegex;
					}
					else
					{
						Bundle.FileRegex = new List<string>();
					}
				}
				{
					List<string> Files;
					if (BundleConfig.GetArray(SectionName, "Files", out Files))
					{
						Bundle.Files = Files;
					}
					else
					{
						Bundle.Files = new List<string>();
					}
				}
				{
					bool bContainsShaderLibrary;
					if (BundleConfig.GetBool(SectionName, "ContainsShaderLibrary", out bContainsShaderLibrary))
					{
						Bundle.bContainsShaderLibrary = bContainsShaderLibrary;
					}
					else 
					{
						Bundle.bContainsShaderLibrary = false;
					}
				}

				{
					bool bUseChunkDBs;
					if (!BundleConfig.GetBool(SectionName, "UseChunkDBs", out bUseChunkDBs))
					{
						Bundle.UseChunkDBs = false;
					}
					else
					{
						Bundle.UseChunkDBs = bUseChunkDBs;
					}
					{
						bool bUseDetailedInstallSizes;
						if (!BundleConfig.GetBool(SectionName, "UseDetailedInstallSizes", out bUseDetailedInstallSizes))
						{
							Bundle.UseDetailedInstallSizes = true; // default to true
						}
						else
						{
							Bundle.UseDetailedInstallSizes = bUseDetailedInstallSizes;
						}
					}

				}
				GetPlatformSettings(Bundle, BundleConfig, BundleDefinitionPrefix + Bundle.Name);

				Results.Add(Bundle);
			}

			// Use OrderBy and not Sort because OrderBy is stable
			Bundles = Results.OrderBy(b => b.Order).ToDictionary(b => b.Name, b => b);
		}

		public static TPlatformBundleSettings MatchBundleSettings<TPlatformBundleSettings>(
			string FileName, IReadOnlyDictionary<string, TPlatformBundleSettings> InstallBundles) where TPlatformBundleSettings : BundleSettings
		{
			// Try to find a matching chunk regex or exact filename
			foreach (var Bundle in InstallBundles.Values)
			{
				foreach (string RegexString in Bundle.FileRegex)
				{
					if (Regex.Match(FileName, RegexString, RegexOptions.IgnoreCase).Success)
					{
						return Bundle;
					}
				}
				foreach(string FileString in Bundle.Files)
				{
					if (FileString.Equals(FileName, StringComparison.OrdinalIgnoreCase))
					{
						return Bundle;
					}
				}
			}

			return null;
		}

		public static IEnumerable<TPlatformBundleSettings> GetBundleDependencies<TPlatformBundleSettings>(
			TPlatformBundleSettings Bundle, IReadOnlyDictionary<string, TPlatformBundleSettings> InstallBundles) where TPlatformBundleSettings : BundleSettings
		{
			var FoundDependencies = new Dictionary<string, TPlatformBundleSettings>();

			var CurrentDependencies = new List<TPlatformBundleSettings>();
			CurrentDependencies.Add(Bundle);

			while(CurrentDependencies.Count > 0)
			{
				var NextDependencies = new List<TPlatformBundleSettings>();
				foreach(var Dep in CurrentDependencies)
				{
					if (FoundDependencies.ContainsKey(Dep.Name))
						continue;
					
					FoundDependencies.Add(Dep.Name, Dep);					

					foreach (var NextDepName in Dep.Dependencies)
					{
						if (InstallBundles.TryGetValue(NextDepName, out TPlatformBundleSettings NextDep))
						{
							NextDependencies.Add(NextDep);
						}
					}
				}
				CurrentDependencies = NextDependencies;
			}

			return FoundDependencies.Values;
		}

		public static bool HasPlatformBundleSource(DirectoryReference ProjectDir, UnrealTargetPlatform Platform)
		{
			ConfigHierarchy BundleConfig = ConfigCache.ReadHierarchy(ConfigHierarchyType.InstallBundle, ProjectDir, Platform);
			return 
				BundleConfig.GetArray("InstallBundleManager.BundleSources", "DefaultBundleSources", out List<string> InstallBundleSources) && 
				InstallBundleSources.Contains("Platform");
		}
	}
}
