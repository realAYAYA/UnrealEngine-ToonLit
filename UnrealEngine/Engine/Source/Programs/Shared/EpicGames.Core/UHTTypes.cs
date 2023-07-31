// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Text.Json.Serialization;

#pragma warning disable CS1591 // Missing documentation

namespace EpicGames.Core
{
	/// <summary>
	/// Defines the version of the code generation to be used.
	/// 
	/// This MUST be kept in sync with EGeneratedBodyVersion defined in 
	/// Engine\Source\Programs\UnrealHeaderTool\Private\GeneratedCodeVersion.h.
	/// </summary>
	public enum EGeneratedCodeVersion
	{
		/// <summary>
		/// Version not set or the default is to be used.
		/// </summary>
		None,

		/// <summary>
		/// 
		/// </summary>
		V1,

		/// <summary>
		/// 
		/// </summary>
		V2,

		/// <summary>
		/// 
		/// </summary>
		VLatest = V2
	};

	/// <summary>
	/// This MUST be kept in sync with EPackageOverrideType defined in 
	/// Engine\Source\Programs\UnrealHeaderTool\Public\IScriptGeneratorPluginInterface.h
	/// </summary>
	public enum EPackageOverrideType
	{
		None,
		EditorOnly,
		EngineDeveloper,
		GameDeveloper,
		EngineUncookedOnly,
		GameUncookedOnly,
	}

	/// <summary>
	/// Type of module. This should be sorted by the order in which we expect modules to be built.
	/// 
	/// This MUST be kept in sync with EBuildModuleType defined in 
	/// Engine\Source\Programs\UnrealHeaderTool\Public\IScriptGeneratorPluginInterface.h
	/// </summary>
	public enum UHTModuleType
	{
		Program,
		EngineRuntime,
		EngineUncooked,
		EngineDeveloper,
		EngineEditor,
		EngineThirdParty,
		GameRuntime,
		GameUncooked,
		GameDeveloper,
		GameEditor,
		GameThirdParty,
	}

	public class UHTManifest
	{
		public class Module
		{
			public string Name { get; set; } = "";
			[JsonConverter(typeof(JsonStringEnumConverter))]
			public UHTModuleType ModuleType { get; set; } = UHTModuleType.Program;
			[JsonConverter(typeof(JsonStringEnumConverter))]
			public EPackageOverrideType OverrideModuleType { get; set; } = EPackageOverrideType.None;
			public string BaseDirectory { get; set; } = string.Empty;
			public string IncludeBase { get; set; } = string.Empty; // The include path which all UHT-generated includes should be relative to
			public string OutputDirectory { get; set; } = string.Empty;
			public List<string> ClassesHeaders { get; set; } = new List<string>();
			public List<string> PublicHeaders { get; set; } = new List<string>();
			public List<string> InternalHeaders { get; set; } = new List<string>();
			public List<string> PrivateHeaders { get; set; } = new List<string>();
			public List<string> PublicDefines { get; set; } = new List<string>();
			public string? GeneratedCPPFilenameBase { get; set; } = null;
			public bool SaveExportedHeaders { get; set; } = false;
			[JsonConverter(typeof(JsonStringEnumConverter))]
			[JsonPropertyName("UHTGeneratedCodeVersion")]
			public EGeneratedCodeVersion GeneratedCodeVersion { get; set; } = EGeneratedCodeVersion.None;

			public override string ToString()
			{
				return Name;
			}

			public bool TryGetDefine(string Name, out string? Value)
			{
				Value = null;
				int Length = Name.Length;
				foreach (string Define in PublicDefines)
				{
					if (!Define.StartsWith(Name))
					{
						continue;
					}
					if (Define.Length > Length)
					{
						if (Define[Length] != '=')
						{
							continue;
						}
						Value = Define.Substring(Length + 1, Define.Length - Length - 1);
					}
					return true;
				}
				return false;
			}

			public bool TryGetDefine(string Name, out int Value)
			{
				string? String;
				if (TryGetDefine(Name, out String))
				{
					return int.TryParse(String, out Value);
				}
				Value = 0;
				return false;
			}
		}

		/// <summary>
		/// True if the current target is a game target
		/// </summary>
		public bool IsGameTarget { get; set; } = false;

		/// <summary>
		/// The engine path on the local machine
		/// </summary>
		public string RootLocalPath { get; set; } = string.Empty;

		/// <summary>
		/// Name of the target currently being compiled
		/// </summary>
		public string TargetName { get; set; } = string.Empty;

		/// <summary>
		/// File to contain additional dependencies that the generated code depends on
		/// </summary>
		public string ExternalDependenciesFile { get; set; } = string.Empty;

		/// <summary>
		/// List of modules
		/// </summary>
		public List<Module> Modules { get; set; } = new List<Module>();

		/// <summary>
		/// List of active UHT plugins.  Only used by the C# version of UHT.  This
		/// list contains plugins from only modules listed above.
		/// </summary>
		public List<string> UhtPlugins { get; set; } = new List<string>();
	}
}
