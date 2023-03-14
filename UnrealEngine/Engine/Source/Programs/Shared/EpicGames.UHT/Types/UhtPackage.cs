// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Text.Json.Serialization;
using EpicGames.Core;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Utils;
using UnrealBuildBase;

namespace EpicGames.UHT.Types
{

	/// <summary>
	/// Represents a UPackage in the engine
	/// </summary>
	[UhtEngineClass(Name = "Package")]
	public class UhtPackage : UhtObject
	{

		/// <summary>
		/// Unique index of the package
		/// </summary>
		[JsonIgnore]
		public int PackageTypeIndex { get; }

		/// <summary>
		/// Engine package flags
		/// </summary>
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public EPackageFlags PackageFlags { get; set; } = EPackageFlags.None;

		/// <summary>
		/// UHT module of the package (1 to 1 relationship)
		/// </summary>
		public UHTManifest.Module Module { get; set; }

		/// <inheritdoc/>
		[JsonIgnore]
		public override UhtEngineType EngineType => UhtEngineType.Package;

		/// <inheritdoc/>
		public override string EngineClassName => "Package";

		/// <inheritdoc/>
		[JsonIgnore]
		public override UhtPackage Package => this;

		/// <inheritdoc/>
		[JsonIgnore]
		public override UhtHeaderFile HeaderFile => throw new NotImplementedException();

		/// <summary>
		/// True if the package is part of the engine
		/// </summary>
		[JsonIgnore]
		public bool IsPartOfEngine
		{
			get
			{
				switch (Module.ModuleType)
				{
					case UHTModuleType.Program:
						return Module.BaseDirectory.Replace('\\', '/').StartsWith(Unreal.EngineDirectory.FullName.Replace('\\', '/'), StringComparison.Ordinal);
					case UHTModuleType.EngineRuntime:
					case UHTModuleType.EngineUncooked:
					case UHTModuleType.EngineDeveloper:
					case UHTModuleType.EngineEditor:
					case UHTModuleType.EngineThirdParty:
						return true;
					case UHTModuleType.GameRuntime:
					case UHTModuleType.GameUncooked:
					case UHTModuleType.GameDeveloper:
					case UHTModuleType.GameEditor:
					case UHTModuleType.GameThirdParty:
						return false;
					default:
						throw new UhtIceException("Invalid module type");
				}
			}
		}

		/// <summary>
		/// True if the package is a plugin
		/// </summary>
		[JsonIgnore]
		public bool IsPlugin => Module.BaseDirectory.Replace('\\', '/').Contains("/Plugins/", StringComparison.Ordinal);

		/// <summary>
		/// Short name of the package (without the /Script/)
		/// </summary>
		public string ShortName { get; set; }

		/// <summary>
		/// Construct a new instance of a package
		/// </summary>
		/// <param name="session">Running session</param>
		/// <param name="module">Source module of the package</param>
		/// <param name="packageFlags">Assorted package flags</param>
		public UhtPackage(UhtSession session, UHTManifest.Module module, EPackageFlags packageFlags) : base(session)
		{
			Module = module;
			PackageFlags = packageFlags;
			PackageTypeIndex = Session.GetNextPackageTypeIndex();

			int lastSlashIndex = module.Name.LastIndexOf('/');
			if (lastSlashIndex == -1)
			{
				SourceName = $"/Script/{module.Name}";
				ShortName = module.Name;
			}
			else
			{
				SourceName = module.Name;
				ShortName = SourceName[(lastSlashIndex + 1)..];
			}
		}
	}
}
