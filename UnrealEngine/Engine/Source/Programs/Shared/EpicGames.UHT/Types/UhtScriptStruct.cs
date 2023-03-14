// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.Json.Serialization;
using EpicGames.Core;
using EpicGames.UHT.Parsers;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Types
{
	/// <summary>
	/// Flags to represent information about a RigVM parameter
	/// </summary>
	[Flags]
	public enum UhtRigVMParameterFlags : int
	{
		/// <summary>
		/// No RigVM flags
		/// </summary>
		None = 0,

		/// <summary>
		/// "Constant" metadata specified
		/// </summary>
		Constant = 0x00000001,

		/// <summary>
		/// "Input" metadata specified
		/// </summary>
		Input = 0x00000002,

		/// <summary>
		/// "Output" metadata specified
		/// </summary>
		Output = 0x00000004,

		/// <summary>
		/// "Singleton" metadata specified
		/// </summary>
		Singleton = 0x00000008,

		/// <summary>
		/// Set if the property is editor only
		/// </summary>
		EditorOnly = 0x00000010,

		/// <summary>
		/// Set if the property is an enum
		/// </summary>
		IsEnum = 0x00010000,

		/// <summary>
		/// Set if the property is an array
		/// </summary>
		IsArray = 0x00020000,
	}

	/// <summary>
	/// Helper methods for testing flags.  These methods perform better than the generic HasFlag which hits
	/// the GC and stalls.
	/// </summary>
	public static class UhtRigVMParameterFlagsExtensions
	{

		/// <summary>
		/// Test to see if any of the specified flags are set
		/// </summary>
		/// <param name="inFlags">Current flags</param>
		/// <param name="testFlags">Flags to test for</param>
		/// <returns>True if any of the flags are set</returns>
		public static bool HasAnyFlags(this UhtRigVMParameterFlags inFlags, UhtRigVMParameterFlags testFlags)
		{
			return (inFlags & testFlags) != 0;
		}

		/// <summary>
		/// Test to see if all of the specified flags are set
		/// </summary>
		/// <param name="inFlags">Current flags</param>
		/// <param name="testFlags">Flags to test for</param>
		/// <returns>True if all the flags are set</returns>
		public static bool HasAllFlags(this UhtRigVMParameterFlags inFlags, UhtRigVMParameterFlags testFlags)
		{
			return (inFlags & testFlags) == testFlags;
		}

		/// <summary>
		/// Test to see if a specific set of flags have a specific value.
		/// </summary>
		/// <param name="inFlags">Current flags</param>
		/// <param name="testFlags">Flags to test for</param>
		/// <param name="matchFlags">Expected value of the tested flags</param>
		/// <returns>True if the given flags have a specific value.</returns>
		public static bool HasExactFlags(this UhtRigVMParameterFlags inFlags, UhtRigVMParameterFlags testFlags, UhtRigVMParameterFlags matchFlags)
		{
			return (inFlags & testFlags) == matchFlags;
		}
	}

	/// <summary>
	/// The FRigVMParameter represents a single parameter of a method
	/// marked up with RIGVM_METHOD.
	/// Each parameter can be marked with Constant, Input or Output
	/// metadata - this struct simplifies access to that information.
	/// </summary>
	public class UhtRigVMParameter
	{
		/// <summary>
		/// Property associated with the RigVM parameter
		/// </summary>
		[JsonConverter(typeof(UhtTypeSourceNameJsonConverter<UhtProperty>))]
		public UhtProperty? Property { get; }

		/// <summary>
		/// Name of the property
		/// </summary>
		public string Name { get; } = String.Empty;

		/// <summary>
		/// Type of the property
		/// </summary>
		public string Type { get; } = String.Empty;

		/// <summary>
		/// Cast name
		/// </summary>
		[JsonIgnore]
		public string? CastName { get; }

		/// <summary>
		/// Cast type
		/// </summary>
		[JsonIgnore]
		public string? CastType { get; }

		/// <summary>
		/// Flags associated with the parameter
		/// </summary>
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public UhtRigVMParameterFlags ParameterFlags { get; set; } = UhtRigVMParameterFlags.None;

		/// <summary>
		/// True if the parameter is marked as "Constant"
		/// </summary>
		[JsonIgnore]
		public bool Constant => ParameterFlags.HasAnyFlags(UhtRigVMParameterFlags.Constant);

		/// <summary>
		/// True if the parameter is marked as "Input"
		/// </summary>
		[JsonIgnore]
		public bool Input => ParameterFlags.HasAnyFlags(UhtRigVMParameterFlags.Input);

		/// <summary>
		/// True if the parameter is marked as "Output"
		/// </summary>
		[JsonIgnore]
		public bool Output => ParameterFlags.HasAnyFlags(UhtRigVMParameterFlags.Output);

		/// <summary>
		/// True if the parameter is marked as "Singleton"
		/// </summary>
		[JsonIgnore]
		public bool Singleton => ParameterFlags.HasAnyFlags(UhtRigVMParameterFlags.Singleton);

		/// <summary>
		/// True if the parameter is editor only
		/// </summary>
		[JsonIgnore]
		public bool EditorOnly => ParameterFlags.HasAnyFlags(UhtRigVMParameterFlags.EditorOnly);

		/// <summary>
		/// True if the parameter is an enum
		/// </summary>
		[JsonIgnore]
		public bool IsEnum => ParameterFlags.HasAnyFlags(UhtRigVMParameterFlags.IsEnum);

		/// <summary>
		/// True if the parameter is an array
		/// </summary>
		[JsonIgnore]
		public bool IsArray => ParameterFlags.HasAnyFlags(UhtRigVMParameterFlags.IsArray);

		/// <summary>
		/// Create a new RigVM parameter from a property
		/// </summary>
		/// <param name="property">Source property</param>
		/// <param name="index">Parameter index.  Used to create a unique cast name.</param>
		public UhtRigVMParameter(UhtProperty property, int index)
		{
			Property = property;

			Name = property.EngineName;
			ParameterFlags |= property.MetaData.ContainsKey("Constant") ? UhtRigVMParameterFlags.Constant : UhtRigVMParameterFlags.None;
			ParameterFlags |= property.MetaData.ContainsKey("Input") ? UhtRigVMParameterFlags.Input : UhtRigVMParameterFlags.None;
			ParameterFlags |= property.MetaData.ContainsKey("Output") ? UhtRigVMParameterFlags.Output : UhtRigVMParameterFlags.None;
			ParameterFlags |= property.IsEditorOnlyProperty ? UhtRigVMParameterFlags.EditorOnly : UhtRigVMParameterFlags.None;
			ParameterFlags |= property.MetaData.ContainsKey("Singleton") ? UhtRigVMParameterFlags.Singleton : UhtRigVMParameterFlags.None;

			if (property.MetaData.ContainsKey("Visible"))
			{
				ParameterFlags |= UhtRigVMParameterFlags.Constant | UhtRigVMParameterFlags.Input;
				ParameterFlags &= ~UhtRigVMParameterFlags.Output;
			}

			if (EditorOnly)
			{
				property.LogError($"RigVM Struct '{Property.Outer?.SourceName}' - Member '{Property.SourceName}' is editor only - WITH_EDITORONLY_DATA not allowed on structs with RIGVM_METHOD.");
			}

			if (property.PropertyCaps.HasAnyFlags(UhtPropertyCaps.SupportsRigVM))
			{
				Type = property.GetRigVMType();
				if (property.PropertyCaps.HasAnyFlags(UhtPropertyCaps.IsRigVMEnum))
				{
					ParameterFlags |= UhtRigVMParameterFlags.IsEnum;
				}
				if (property.PropertyCaps.HasAnyFlags(UhtPropertyCaps.IsRigVMArray))
				{
					ParameterFlags |= UhtRigVMParameterFlags.IsArray;
					if (IsConst())
					{
						string extendedType = ExtendedType(false);
						CastName = $"{Name}_{index}_Array";
						CastType = $"TArrayView<const {extendedType[1..^1]}>";
					}
				}
			}
			else
			{
				property.LogError($"RigVM Struct '{Property.Outer?.SourceName}' - Member '{Property.SourceName}' type '{Property.GetUserFacingDecl()}' not supported by RigVM.");
			}
		}

		/// <summary>
		/// Create a new parameter
		/// </summary>
		/// <param name="name">Name of the parameter</param>
		/// <param name="type">Type of the parameter</param>
		public UhtRigVMParameter(string name, string type)
		{
			Name = name;
			Type = type;
		}

		/// <summary>
		/// Get the name of the parameter
		/// </summary>
		/// <param name="castName">If true, return the cast name</param>
		/// <returns>Parameter name</returns>
		public string NameOriginal(bool castName = false)
		{
			return castName && CastName != null ? CastName : Name;
		}

		/// <summary>
		/// Get the type of the parameter
		/// </summary>
		/// <param name="castType">If true, return the cast type</param>
		/// <returns>Parameter type</returns>
		public string TypeOriginal(bool castType = false)
		{
			return castType && CastType != null ? CastType : Type;
		}

		/// <summary>
		/// Get the full declaration (type and name)
		/// </summary>
		/// <param name="castType">If true, return the cast type</param>
		/// <param name="castName">If true, return the cast name</param>
		/// <returns>Parameter declaration</returns>
		public string Declaration(bool castType = false, bool castName = false)
		{
			return $"{TypeOriginal(castType)} {NameOriginal(castName)}";
		}

		/// <summary>
		/// Return the base type without any template arguments
		/// </summary>
		/// <param name="castType">If true, return the cast type</param>
		/// <returns>Base parameter type</returns>
		public string BaseType(bool castType = false)
		{
			string typeOriginal = TypeOriginal(castType);

			int lesserPos = typeOriginal.IndexOf('<', StringComparison.Ordinal);
			if (lesserPos >= 0)
			{
				return typeOriginal[..lesserPos];
			}
			else
			{
				return typeOriginal;
			}
		}

		/// <summary>
		/// Template arguments of the type or type if not a template type.
		/// </summary>
		/// <param name="castType">If true, return the cast type</param>
		/// <returns>Template arguments of the type</returns>
		public string ExtendedType(bool castType = false)
		{
			string typeOriginal = TypeOriginal(castType);

			int lesserPos = typeOriginal.IndexOf('<', StringComparison.Ordinal);
			if (lesserPos >= 0)
			{
				return typeOriginal[lesserPos..];
			}
			else
			{
				return typeOriginal;
			}
		}

		/// <summary>
		/// Return the type with a const reference
		/// </summary>
		/// <param name="castType">If true, return the cast type</param>
		/// <returns>Type with a const reference</returns>
		public string TypeConstRef(bool castType = false)
		{
			string typeNoRef = TypeNoRef(castType);
			if (typeNoRef.StartsWith("T", StringComparison.Ordinal) || typeNoRef.StartsWith("F", StringComparison.Ordinal))
			{
				return $"const {typeNoRef}&";
			}
			else
			{
				return $"const {typeNoRef}";
			}
		}

		/// <summary>
		/// Return the type with a reference
		/// </summary>
		/// <param name="castType">If true, return the cast type</param>
		/// <returns>Type with a reference</returns>
		public string TypeRef(bool castType = false)
		{
			string typeNoRef = TypeNoRef(castType);
			return $"{typeNoRef}&";
		}

		/// <summary>
		/// Return the type without reference
		/// </summary>
		/// <param name="castType">If true, return the cast type</param>
		/// <returns>Type without the reference</returns>
		public string TypeNoRef(bool castType = false)
		{
			string typeOriginal = TypeOriginal(castType);
			if (typeOriginal.EndsWith("&", StringComparison.Ordinal))
			{
				return typeOriginal[0..^1];
			}
			else
			{
				return typeOriginal;
			}
		}

		/// <summary>
		/// Return the type as a reference
		/// </summary>
		/// <param name="castType">If true, return the cast type</param>
		/// <returns>Type as a reference</returns>
		public string TypeVariableRef(bool castType = false)
		{
			return IsConst() ? TypeConstRef(castType) : TypeRef(castType);
		}

		/// <summary>
		/// Return a variable declaration for the parameter
		/// </summary>
		/// <param name="castType">If true, return the cast type</param>
		/// <param name="castName">If true, return the cast name</param>
		/// <returns>Parameter as a variable declaration</returns>
		public string Variable(bool castType = false, bool castName = false)
		{
			return $"{TypeVariableRef(castType)} {NameOriginal(castName)}";
		}

		/// <summary>
		/// True if the parameter is constant
		/// </summary>
		/// <returns>True if the parameter is constant</returns>
		public bool IsConst()
		{
			return Constant || (Input && !Output);
		}

		/// <summary>
		/// Return true if the parameter requires a cast
		/// </summary>
		/// <returns>True if the parameter requires a cast</returns>
		public bool RequiresCast()
		{
			return CastType != null && CastName != null;
		}
	}

	/// <summary>
	/// A single info dataset for a function marked with RIGVM_METHOD.
	/// This struct provides access to its name, the return type and all parameters.
	/// </summary>
	public class UhtRigVMMethodInfo
	{
		private static readonly string s_noPrefix = String.Empty;
		private const string ReturnPrefixInternal = "return ";

		/// <summary>
		/// Return type of the method
		/// </summary>
		public string ReturnType { get; set; } = String.Empty;

		/// <summary>
		/// Name of the method
		/// </summary>
		public string Name { get; set; } = String.Empty;

		/// <summary>
		/// Method parameters
		/// </summary>
		public List<UhtRigVMParameter> Parameters { get; } = new List<UhtRigVMParameter>();

		/// <summary>
		/// If the method has a return value, return "return".  Otherwise return nothing.
		/// </summary>
		/// <returns>Prefix required for the return value</returns>
		public string ReturnPrefix()
		{
			return (ReturnType.Length == 0 || ReturnType == "void") ? s_noPrefix : ReturnPrefixInternal;
		}
	}

	/// <summary>
	/// An info dataset providing access to all functions marked with RIGVM_METHOD
	/// for each struct.
	/// </summary>
	public class UhtRigVMStructInfo
	{

		/// <summary>
		/// True if the GetUpgradeInfoMethod was found. 
		/// </summary>
		public bool HasGetUpgradeInfoMethod { get; set; } = false;

		/// <summary>
		/// True if the GetNextAggregateNameMethod was found. 
		/// </summary>
		public bool HasGetNextAggregateNameMethod { get; set; } = false;

		/// <summary>
		/// Engine name of the owning script struct
		/// </summary>
		public string Name { get; set; } = String.Empty;

		/// <summary>
		/// List of the members
		/// </summary>
		public List<UhtRigVMParameter> Members { get; } = new List<UhtRigVMParameter>();

		/// <summary>
		/// List of the methods
		/// </summary>
		public List<UhtRigVMMethodInfo> Methods { get; } = new List<UhtRigVMMethodInfo>();
	};

	/// <summary>
	/// Series of flags not part of the engine's script struct flags that affect code generation or verification
	/// </summary>
	[Flags]
	public enum UhtScriptStructExportFlags : int
	{

		/// <summary>
		/// No export flags
		/// </summary>
		None = 0,

		/// <summary>
		/// "HasDefaults" specifier present
		/// </summary>
		HasDefaults = 1 << 0,

		/// <summary>
		/// "HasNoOpConstructor" specifier present
		/// </summary>
		HasNoOpConstructor = 1 << 1,

		/// <summary>
		/// "IsAlwaysAccessible" specifier present
		/// </summary>
		IsAlwaysAccessible = 1 << 2,

		/// <summary>
		/// "IsCoreType" specifier present
		/// </summary>
		IsCoreType = 1 << 3,
	}

	/// <summary>
	/// Helper methods for testing flags.  These methods perform better than the generic HasFlag which hits
	/// the GC and stalls.
	/// </summary>
	public static class UhtScriptStructExportFlagsExtensions
	{

		/// <summary>
		/// Test to see if any of the specified flags are set
		/// </summary>
		/// <param name="inFlags">Current flags</param>
		/// <param name="testFlags">Flags to test for</param>
		/// <returns>True if any of the flags are set</returns>
		public static bool HasAnyFlags(this UhtScriptStructExportFlags inFlags, UhtScriptStructExportFlags testFlags)
		{
			return (inFlags & testFlags) != 0;
		}

		/// <summary>
		/// Test to see if all of the specified flags are set
		/// </summary>
		/// <param name="inFlags">Current flags</param>
		/// <param name="testFlags">Flags to test for</param>
		/// <returns>True if all the flags are set</returns>
		public static bool HasAllFlags(this UhtScriptStructExportFlags inFlags, UhtScriptStructExportFlags testFlags)
		{
			return (inFlags & testFlags) == testFlags;
		}

		/// <summary>
		/// Test to see if a specific set of flags have a specific value.
		/// </summary>
		/// <param name="inFlags">Current flags</param>
		/// <param name="testFlags">Flags to test for</param>
		/// <param name="matchFlags">Expected value of the tested flags</param>
		/// <returns>True if the given flags have a specific value.</returns>
		public static bool HasExactFlags(this UhtScriptStructExportFlags inFlags, UhtScriptStructExportFlags testFlags, UhtScriptStructExportFlags matchFlags)
		{
			return (inFlags & testFlags) == matchFlags;
		}
	}

	/// <summary>
	/// Represents the USTRUCT object
	/// </summary>
	[UhtEngineClass(Name = "ScriptStruct")]
	public class UhtScriptStruct : UhtStruct
	{
		/// <summary>
		/// Script struct engine flags
		/// </summary>
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public EStructFlags ScriptStructFlags { get; set; } = EStructFlags.NoFlags;

		/// <summary>
		/// UHT only script struct falgs
		/// </summary>
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public UhtScriptStructExportFlags ScriptStructExportFlags { get; set; } = UhtScriptStructExportFlags.None;

		/// <summary>
		/// Line number where GENERATED_BODY/GENERATED_USTRUCT_BODY macro was found
		/// </summary>
		public int MacroDeclaredLineNumber { get; set; } = -1;

		/// <summary>
		/// RigVM structure info
		/// </summary>
		public UhtRigVMStructInfo? RigVMStructInfo { get; set; } = null;

		/// <inheritdoc/>
		[JsonIgnore]
		public override UhtEngineType EngineType => UhtEngineType.ScriptStruct;

		/// <summary>
		/// True if the struct has the "HasDefaults" specifier
		/// </summary>
		[JsonIgnore]
		public bool HasDefaults => ScriptStructExportFlags.HasAnyFlags(UhtScriptStructExportFlags.HasDefaults);

		/// <summary>
		/// True if the struct has the "IsAlwaysAccessible" specifier
		/// </summary>
		[JsonIgnore]
		public bool IsAlwaysAccessible => ScriptStructExportFlags.HasAnyFlags(UhtScriptStructExportFlags.IsAlwaysAccessible);

		/// <summary>
		/// True if the struct has the "IsCoreType" specifier
		/// </summary>
		[JsonIgnore]
		public bool IsCoreType => ScriptStructExportFlags.HasAnyFlags(UhtScriptStructExportFlags.IsCoreType);

		/// <summary>
		/// True if the struct has the "HasNoOpConstructor" specifier
		/// </summary>
		[JsonIgnore]
		public bool HasNoOpConstructor => ScriptStructExportFlags.HasAnyFlags(UhtScriptStructExportFlags.HasNoOpConstructor);

		/// <inheritdoc/>
		public override string EngineClassName => "ScriptStruct";

		/// <inheritdoc/>
		[JsonIgnore]
		public override string EngineNamePrefix => Session.Config!.IsStructWithTPrefix(EngineName) ? "T" : "F";

		///<inheritdoc/>
		[JsonIgnore]
		protected override UhtSpecifierValidatorTable? SpecifierValidatorTable => Session.GetSpecifierValidatorTable(UhtTableNames.ScriptStruct);

		/// <summary>
		/// Super struct
		/// </summary>
		[JsonConverter(typeof(UhtNullableTypeSourceNameJsonConverter<UhtScriptStruct>))]
		public UhtScriptStruct? SuperScriptStruct => (UhtScriptStruct?)Super;

		/// <summary>
		/// Construct a new script struct
		/// </summary>
		/// <param name="outer">Outer type</param>
		/// <param name="lineNumber">Line number of the definition</param>
		public UhtScriptStruct(UhtType outer, int lineNumber) : base(outer, lineNumber)
		{
		}

		#region Resolution support
		/// <inheritdoc/>
		protected override void ResolveSuper(UhtResolvePhase resolvePhase)
		{
			base.ResolveSuper(resolvePhase);

			switch (resolvePhase)
			{
				case UhtResolvePhase.Bases:
					BindAndResolveSuper(SuperIdentifier, UhtFindOptions.ScriptStruct);

					// if we have a base struct, propagate inherited struct flags now
					UhtScriptStruct? superScriptStruct = SuperScriptStruct;
					if (superScriptStruct != null)
					{
						ScriptStructFlags |= superScriptStruct.ScriptStructFlags & EStructFlags.Inherit;
					}
					break;
			}
		}

		/// <inheritdoc/>
		protected override bool ResolveSelf(UhtResolvePhase resolvePhase)
		{
			bool result = base.ResolveSelf(resolvePhase);

			switch (resolvePhase)
			{
				case UhtResolvePhase.Properties:
					UhtPropertyParser.ResolveChildren(this, UhtPropertyParseOptions.AddModuleRelativePath);
					break;
			}
			return result;
		}

		/// <inheritdoc/>
		protected override void ResolveChildren(UhtResolvePhase phase)
		{

			// Setup additional property as well as script struct def flags
			// for structs / properties being used for the RigVM.
			// The Input / Output / Constant metadata tokens can be used to mark
			// up an input / output pin of a RigVMNode. To allow authoring of those
			// nodes we'll mark up the property as accessible in Blueprint / Python
			// as well as make the struct a blueprint type.
			switch (phase)
			{
				case UhtResolvePhase.Properties:
					foreach (UhtType child in Children)
					{
						if (child is UhtProperty property)
						{
							EPropertyFlags originalFlags = property.PropertyFlags;
							if (property.MetaData.ContainsKey(UhtNames.Constant))
							{
								property.PropertyFlags |= EPropertyFlags.Edit | EPropertyFlags.EditConst | EPropertyFlags.BlueprintVisible;
							}
							if (property.MetaData.ContainsKey(UhtNames.Input) || property.MetaData.ContainsKey(UhtNames.Visible))
							{
								property.PropertyFlags |= EPropertyFlags.Edit | EPropertyFlags.BlueprintVisible;
							}
							if (property.MetaData.ContainsKey(UhtNames.Output))
							{
								if (!property.PropertyFlags.HasAnyFlags(EPropertyFlags.BlueprintVisible))
								{
									property.PropertyFlags |= EPropertyFlags.BlueprintVisible | EPropertyFlags.BlueprintReadOnly;
								}
							}

							if (originalFlags != property.PropertyFlags &&
								property.PropertyFlags.HasAnyFlags(EPropertyFlags.BlueprintVisible | EPropertyFlags.BlueprintReadOnly))
							{
								if (!MetaData.GetBooleanHierarchical(UhtNames.BlueprintType))
								{
									MetaData.Add(UhtNames.BlueprintType, true);
								}

								if (!property.MetaData.ContainsKey(UhtNames.Category))
								{
									property.MetaData.Add(UhtNames.Category, UhtNames.Pins);
								}
							}
						}
					}
					break;
			}

			base.ResolveChildren(phase);

			switch (phase)
			{
				case UhtResolvePhase.Final:
					if (ScanForInstancedReferenced(false))
					{
						ScriptStructFlags |= EStructFlags.HasInstancedReference;
					}
					CollectRigVMMembers();
					break;
			}
		}

		/// <inheritdoc/>
		protected override bool ScanForInstancedReferencedInternal(bool deepScan)
		{
			if (ScriptStructFlags.HasAnyFlags(EStructFlags.HasInstancedReference))
			{
				return true;
			}

			if (SuperScriptStruct != null && SuperScriptStruct.ScanForInstancedReferenced(deepScan))
			{
				return true;
			}

			return base.ScanForInstancedReferencedInternal(deepScan);
		}
		#endregion

		#region Validation support
		/// <inheritdoc/>
		protected override UhtValidationOptions Validate(UhtValidationOptions options)
		{
			options = base.Validate(options);

			if (ScriptStructFlags.HasAnyFlags(EStructFlags.Immutable))
			{
				if (HeaderFile != Session.UObject.HeaderFile)
				{
					this.LogError("Immutable is being phased out in favor of SerializeNative, and is only legal on the mirror structs declared in UObject");
				}
			}

			// Validate the engine name
			string expectedName = $"{EngineNamePrefix}{EngineName}";
			if (SourceName != expectedName)
			{
				this.LogError($"Struct '{SourceName}' has an invalid Unreal prefix, expecting '{expectedName}");
			}

			// Validate RigVM
			if (RigVMStructInfo != null && MetaData.ContainsKey("Deprecated") && !RigVMStructInfo.HasGetUpgradeInfoMethod)
			{
				this.LogError($"RigVMStruct '{SourceName}' is marked as deprecated but is missing 'GetUpgradeInfo method.");
				this.LogError("Please implement a method like below:");
				this.LogError("RIGVM_METHOD()");
				this.LogError("virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;");
			}

			ValidateProperties();

			return options |= UhtValidationOptions.Shadowing;
		}

		void ValidateProperties()
		{
			bool hasTestedStruct = false;
			foreach (UhtType child in Children)
			{
				if (child is UhtProperty property)
				{
					if (property.PropertyFlags.HasAnyFlags(EPropertyFlags.BlueprintVisible))
					{
						if (!property.PropertyFlags.HasAnyFlags(EPropertyFlags.BlueprintAssignable | EPropertyFlags.BlueprintCallable) && property is not UhtMulticastDelegateProperty)
						{
							if (!hasTestedStruct)
							{
								hasTestedStruct = true;
								if (!MetaData.GetBooleanHierarchical(UhtNames.BlueprintType))
								{
									property.LogError($"Cannot expose property to blueprints in a struct that is not a BlueprintType. Struct: {SourceName} Property: {property.SourceName}");
								}
							}

							if (property.IsStaticArray)
							{
								property.LogError($"Static array cannot be exposed to blueprint Class: {SourceName} Property: {property.SourceName}");
							}

							if (!property.PropertyCaps.HasAnyFlags(UhtPropertyCaps.IsMemberSupportedByBlueprint))
							{
								property.LogError($"Type '{property.GetUserFacingDecl()}' is not supported by blueprint. Struct: {SourceName} Property: {property.SourceName}");
							}
						}
					}
				}
			}
		}
		#endregion

		/// <inheritdoc/>
		public override void CollectReferences(IUhtReferenceCollector collector)
		{
			if (ScriptStructFlags.HasAnyFlags(EStructFlags.NoExport))
			{
				collector.AddSingleton(this);
			}
			collector.AddExportType(this);
			collector.AddDeclaration(this, true);
			collector.AddCrossModuleReference(this, true);
			if (SuperScriptStruct != null)
			{
				collector.AddCrossModuleReference(SuperScriptStruct, true);
			}
			collector.AddCrossModuleReference(Package, true);
			foreach (UhtType child in Children)
			{
				child.CollectReferences(collector);
			}
		}

		private void CollectRigVMMembers()
		{
			if (RigVMStructInfo != null)
			{
				for (UhtStruct? current = this; current != null; current = current.SuperStruct)
				{
					foreach (UhtProperty property in current.Properties)
					{
						RigVMStructInfo.Members.Add(new UhtRigVMParameter(property, RigVMStructInfo.Members.Count));
					}
				}

				if (RigVMStructInfo.Members.Count == 0)
				{
					this.LogError($"RigVM Struct '{SourceName}' - has zero members - invalid RIGVM_METHOD.");
				}
				else if (RigVMStructInfo.Members.Count > 64)
				{
					this.LogError($"RigVM Struct '{SourceName}' - has {RigVMStructInfo.Members.Count} members (64 is the limit).");
				}
			}
		}
	}
}