// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using EpicGames.Core;
using EpicGames.UHT.Tokenizer;
using EpicGames.UHT.Types;

namespace EpicGames.UHT.Utils
{

	/// <summary>
	/// Describes whether issues will generate warnings or errors
	/// </summary>
	public enum UhtIssueBehavior
	{
		/// <summary>
		/// An error will be generated.
		/// </summary>
		Disallow,

		/// <summary>
		/// Ignore the issue.
		/// </summary>
		AllowSilently,

		/// <summary>
		/// Log a warning.
		/// </summary>
		AllowAndLog,
	};

	/// <summary>
	/// Interface for accessing configuration data.  Since UnrealBuildTool depends on EpicGames.UHT and all 
	/// of the configuration support exists in UBT, configuration data must be accessed through an interface.
	/// </summary>
	public interface IUhtConfig
	{
		/// <summary>
		/// Default version of generated code. Defaults to oldest possible, unless specified otherwise in config.
		/// </summary>
		public EGeneratedCodeVersion DefaultGeneratedCodeVersion { get; }

		/// <summary>
		/// Pointer warning for native pointers in the engine
		/// </summary>
		public UhtIssueBehavior EngineNativePointerMemberBehavior { get; }

		/// <summary>
		/// Pointer warning for object pointers in the engine
		/// </summary>
		public UhtIssueBehavior EngineObjectPtrMemberBehavior { get; }

		/// <summary>
		/// Pointer warning for native pointers in engine plugins
		/// </summary>
		public UhtIssueBehavior EnginePluginNativePointerMemberBehavior { get; }

		/// <summary>
		/// Pointer warning for object pointers in engine plugins
		/// </summary>
		public UhtIssueBehavior EnginePluginObjectPtrMemberBehavior { get; }

		/// <summary>
		/// Pointer warning for native pointers outside the engine
		/// </summary>
		public UhtIssueBehavior NonEngineNativePointerMemberBehavior { get; }

		/// <summary>
		/// Pointer warning for object pointers outside the engine
		/// </summary>
		public UhtIssueBehavior NonEngineObjectPtrMemberBehavior { get; }

		/// <summary>
		/// If true, deprecation warnings should be shown
		/// </summary>
		public bool ShowDeprecations { get; }

		/// <summary>
		/// If true, UObject properties are enabled in RigVM
		/// </summary>
		public bool AreRigVMUObjectPropertiesEnabled { get; }

		/// <summary>
		/// If true, UInterface properties are enabled in RigVM
		/// </summary>
		public bool AreRigVMUInterfaceProeprtiesEnabled { get; }

		/// <summary>
		/// Behavior for when generated headers aren't properly included in engine code.
		/// </summary>
		public UhtIssueBehavior EngineMissingGeneratedHeaderIncludeBehavior { get; }

		/// <summary>
		/// Behavior for when generated headers aren't properly included in non-engine code.
		/// </summary>
		public UhtIssueBehavior NonEngineMissingGeneratedHeaderIncludeBehavior { get; }

		/// <summary>
		/// Behavior for when enum underlying types aren't set for engine code.
		/// </summary>
		public UhtIssueBehavior EngineEnumUnderlyingTypeNotSet { get; }

		/// <summary>
		/// Behavior for when enum underlying types aren't set for non engine code.
		/// </summary>
		public UhtIssueBehavior NonEngineEnumUnderlyingTypeNotSet { get; }

		/// <summary>
		/// Collection of all known documentation policies
		/// </summary>
		public IReadOnlyDictionary<string, UhtDocumentationPolicy> DocumentationPolicies { get; }

		/// <summary>
		/// Default documentation policy to be used if none is specified
		/// </summary>
		public string DefaultDocumentationPolicy { get; }

		/// <summary>
		/// If the token references a remapped identifier, update the value in the token 
		/// </summary>
		/// <param name="token">Token to be remapped</param>
		public void RedirectTypeIdentifier(ref UhtToken token);

		/// <summary>
		/// Return the remapped key or the existing key
		/// </summary>
		/// <param name="key">Key to be remapped.</param>
		/// <param name="newKey">Resulting key name</param>
		/// <returns>True if the key has been remapped and has changed.</returns>
		public bool RedirectMetaDataKey(string key, out string newKey);

		/// <summary>
		/// Test to see if the given units are valid.
		/// </summary>
		/// <param name="units">Units to test</param>
		/// <returns>True if the units are valid, false if not</returns>
		public bool IsValidUnits(StringView units);

		/// <summary>
		/// Test to see if the structure name should be using a "T" prefix.
		/// </summary>
		/// <param name="name">Name of the structure to test without any prefix.</param>
		/// <returns>True if the structure should have a "T" prefix.</returns>
		public bool IsStructWithTPrefix(StringView name);

		/// <summary>
		/// Test to see if the given macro has a parameter count as part of the name.
		/// </summary>
		/// <param name="delegateMacro">Macro to test</param>
		/// <returns>-1 if the macro does not contain a parameter count.  The number of parameters minus one.</returns>
		public int FindDelegateParameterCount(StringView delegateMacro);

		/// <summary>
		/// Get the parameter count string associated with the given index
		/// </summary>
		/// <param name="index">Index from a prior call to FindDelegateParameterCount or -1.</param>
		/// <returns>Parameter count string or an empty string if Index is -1.</returns>
		public StringView GetDelegateParameterCountString(int index);

		/// <summary>
		/// Test to see if the exporter is enabled
		/// </summary>
		/// <param name="name">Name of the exporter</param>
		/// <returns>True if the exporter is enabled, false if not</returns>
		public bool IsExporterEnabled(string name);
	}
}
