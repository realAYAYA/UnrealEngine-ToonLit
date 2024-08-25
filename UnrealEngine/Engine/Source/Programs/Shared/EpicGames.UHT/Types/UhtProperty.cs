// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Text.Json.Serialization;
using EpicGames.Core;
using EpicGames.UHT.Parsers;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Tokenizer;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Types
{

	/// <summary>
	/// Collection of UHT only flags associated with properties
	/// </summary>
	[Flags]
	public enum UhtPropertyExportFlags
	{
		/// <summary>
		/// Property should be exported as public
		/// </summary>
		Public = 0x00000001,

		/// <summary>
		/// Property should be exported as private
		/// </summary>
		Private = 0x00000002,

		/// <summary>
		/// Property should be exported as protected
		/// </summary>
		Protected = 0x00000004,

		/// <summary>
		/// The BlueprintPure flag was set in software and should not be considered an error
		/// </summary>
		ImpliedBlueprintPure = 0x00000008,

		/// <summary>
		/// If true, the property has a getter function
		/// </summary>
		GetterSpecified = 0x00000010,

		/// <summary>
		/// If true, the property has a setter function
		/// </summary>
		SetterSpecified = 0x00000020,

		/// <summary>
		/// If true, the getter has been disabled in the specifiers
		/// </summary>
		GetterSpecifiedNone = 0x00000040,

		/// <summary>
		/// If true, the setter has been disabled in the specifiers
		/// </summary>
		SetterSpecifiedNone = 0x00000080,

		/// <summary>
		/// If true, the getter function was found
		/// </summary>
		GetterFound = 0x00000100,

		/// <summary>
		/// If true, the property has a setter function
		/// </summary>
		SetterFound = 0x00000200,

		/// <summary>
		/// Property is marked as a field notify
		/// </summary>
		FieldNotify = 0x00000400,

		/// <summary>
		/// If true, the property should have a generated getter function
		/// </summary>
		GetterSpecifiedAuto = 0x00001000,
		
		/// <summary>
		/// If true, the property should have a generated setter function
		/// </summary>
		SetterSpecifiedAuto = 0x00002000,
	};

	/// <summary>
	/// Helper methods for testing flags.  These methods perform better than the generic HasFlag which hits
	/// the GC and stalls.
	/// </summary>
	public static class UhtPropertyExportFlagsExtensions
	{

		/// <summary>
		/// Test to see if any of the specified flags are set
		/// </summary>
		/// <param name="inFlags">Current flags</param>
		/// <param name="testFlags">Flags to test for</param>
		/// <returns>True if any of the flags are set</returns>
		public static bool HasAnyFlags(this UhtPropertyExportFlags inFlags, UhtPropertyExportFlags testFlags)
		{
			return (inFlags & testFlags) != 0;
		}

		/// <summary>
		/// Test to see if all of the specified flags are set
		/// </summary>
		/// <param name="inFlags">Current flags</param>
		/// <param name="testFlags">Flags to test for</param>
		/// <returns>True if all the flags are set</returns>
		public static bool HasAllFlags(this UhtPropertyExportFlags inFlags, UhtPropertyExportFlags testFlags)
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
		public static bool HasExactFlags(this UhtPropertyExportFlags inFlags, UhtPropertyExportFlags testFlags, UhtPropertyExportFlags matchFlags)
		{
			return (inFlags & testFlags) == matchFlags;
		}
	}

	/// <summary>
	/// The context of the property.
	/// </summary>
	public enum UhtPropertyCategory
	{
		/// <summary>
		/// Function parameter for a function that isn't marked as NET
		/// </summary>
		RegularParameter,

		/// <summary>
		/// Function parameter for a function that is marked as NET
		/// </summary>
		ReplicatedParameter,

		/// <summary>
		/// Function return value
		/// </summary>
		Return,

		/// <summary>
		/// Class or a script structure member property
		/// </summary>
		Member,
	};

	/// <summary>
	/// Helper methods for the property category
	/// </summary>
	public static class UhtPropertyCategoryExtensions
	{

		/// <summary>
		/// Return the hint text for the property category
		/// </summary>
		/// <param name="propertyCategory">Property category</param>
		/// <returns>The user facing hint text</returns>
		/// <exception cref="UhtIceException">Unexpected category</exception>
		public static string GetHintText(this UhtPropertyCategory propertyCategory)
		{
			switch (propertyCategory)
			{
				case UhtPropertyCategory.ReplicatedParameter:
				case UhtPropertyCategory.RegularParameter:
					return "Function parameter";

				case UhtPropertyCategory.Return:
					return "Function return type";

				case UhtPropertyCategory.Member:
					return "Member variable declaration";

				default:
					throw new UhtIceException("Unknown variable category");
			}
		}
	}

	/// <summary>
	/// Allocator used for container
	/// </summary>
	public enum UhtPropertyAllocator
	{
		/// <summary>
		/// Default allocator
		/// </summary>
		Default,

		/// <summary>
		/// Memory image allocator
		/// </summary>
		MemoryImage
	};

	/// <summary>
	/// Type of pointer
	/// </summary>
	public enum UhtPointerType
	{

		/// <summary>
		/// No pointer specified
		/// </summary>
		None,

		/// <summary>
		/// Native pointer specified
		/// </summary>
		Native,
	}

	/// <summary>
	/// Type of reference
	/// </summary>
	public enum UhtPropertyRefQualifier
	{

		/// <summary>
		/// Property is not a reference
		/// </summary>
		None,

		/// <summary>
		/// Property is a const reference
		/// </summary>
		ConstRef,

		/// <summary>
		/// Property is a non-const reference
		/// </summary>
		NonConstRef,
	};

	/// <summary>
	/// Options that customize the properties.
	/// </summary>
	[Flags]
	public enum UhtPropertyOptions
	{

		/// <summary>
		/// No property options
		/// </summary>
		None = 0,

		/// <summary>
		/// Don't automatically mark properties as CPF_Const
		/// </summary>
		NoAutoConst = 1 << 0,
	}

	/// <summary>
	/// Helper methods for testing flags.  These methods perform better than the generic HasFlag which hits
	/// the GC and stalls.
	/// </summary>
	public static class UhtPropertyOptionsExtensions
	{

		/// <summary>
		/// Test to see if any of the specified flags are set
		/// </summary>
		/// <param name="inFlags">Current flags</param>
		/// <param name="testFlags">Flags to test for</param>
		/// <returns>True if any of the flags are set</returns>
		public static bool HasAnyFlags(this UhtPropertyOptions inFlags, UhtPropertyOptions testFlags)
		{
			return (inFlags & testFlags) != 0;
		}

		/// <summary>
		/// Test to see if all of the specified flags are set
		/// </summary>
		/// <param name="inFlags">Current flags</param>
		/// <param name="testFlags">Flags to test for</param>
		/// <returns>True if all the flags are set</returns>
		public static bool HasAllFlags(this UhtPropertyOptions inFlags, UhtPropertyOptions testFlags)
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
		public static bool HasExactFlags(this UhtPropertyOptions inFlags, UhtPropertyOptions testFlags, UhtPropertyOptions matchFlags)
		{
			return (inFlags & testFlags) == matchFlags;
		}
	}

	/// <summary>
	/// Property capabilities.  Use the caps system instead of patterns such as "Property is UhtObjectProperty"
	/// </summary>
	[Flags]
	public enum UhtPropertyCaps
	{
		/// <summary>
		/// No property caps
		/// </summary>
		None = 0,

		/// <summary>
		/// If true, the property will be passed by reference when generating the full type string
		/// </summary>
		PassCppArgsByRef = 1 << 0,

		/// <summary>
		/// If true, the an argument will need to be added to the constructor
		/// </summary>
		RequiresNullConstructorArg = 1 << 1,

		/// <summary>
		/// If true, the property type can be TArray or TMap value
		/// </summary>
		CanBeContainerValue = 1 << 2,

		/// <summary>
		/// If true, the property type can be a TSet or TMap key
		/// </summary>
		CanBeContainerKey = 1 << 3,

		/// <summary>
		/// True if the property can be instanced
		/// </summary>
		CanBeInstanced = 1 << 4,

		/// <summary>
		/// True if the property can be exposed on spawn
		/// </summary>
		CanExposeOnSpawn = 1 << 5,

		/// <summary>
		/// True if the property can have a config setting
		/// </summary>
		CanHaveConfig = 1 << 6,

		/// <summary>
		/// True if the property allows the BlueprintAssignable flag.
		/// </summary>
		CanBeBlueprintAssignable = 1 << 7,

		/// <summary>
		/// True if the property allows the BlueprintCallable flag.
		/// </summary>
		CanBeBlueprintCallable = 1 << 8,

		/// <summary>
		/// True if the property allows the BlueprintAuthorityOnly flag.
		/// </summary>
		CanBeBlueprintAuthorityOnly = 1 << 9,

		/// <summary>
		/// True to see if the function parameter property is supported by blueprint
		/// </summary>
		IsParameterSupportedByBlueprint = 1 << 10,

		/// <summary>
		/// True to see if the member property is supported by blueprint
		/// </summary>
		IsMemberSupportedByBlueprint = 1 << 11,

		/// <summary>
		/// True if the property supports RigVM
		/// </summary>
		SupportsRigVM = 1 << 12,

		/// <summary>
		/// True if the property should codegen as an enumeration
		/// </summary>
		IsRigVMEnum = 1 << 13,

		/// <summary>
		/// True if the property should codegen as an array
		/// </summary>
		IsRigVMArray = 1 << 14,
		
		/// <summary>
		/// True if the property should codegen as a byte enumeration
		/// </summary>
		IsRigVMEnumAsByte = 1 << 15,
	}

	/// <summary>
	/// Helper methods for testing flags.  These methods perform better than the generic HasFlag which hits
	/// the GC and stalls.
	/// </summary>
	public static class UhtPropertyCapsExtensions
	{

		/// <summary>
		/// Test to see if any of the specified flags are set
		/// </summary>
		/// <param name="inFlags">Current flags</param>
		/// <param name="testFlags">Flags to test for</param>
		/// <returns>True if any of the flags are set</returns>
		public static bool HasAnyFlags(this UhtPropertyCaps inFlags, UhtPropertyCaps testFlags)
		{
			return (inFlags & testFlags) != 0;
		}

		/// <summary>
		/// Test to see if all of the specified flags are set
		/// </summary>
		/// <param name="inFlags">Current flags</param>
		/// <param name="testFlags">Flags to test for</param>
		/// <returns>True if all the flags are set</returns>
		public static bool HasAllFlags(this UhtPropertyCaps inFlags, UhtPropertyCaps testFlags)
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
		public static bool HasExactFlags(this UhtPropertyCaps inFlags, UhtPropertyCaps testFlags, UhtPropertyCaps matchFlags)
		{
			return (inFlags & testFlags) == matchFlags;
		}
	}

	/// <summary>
	/// Text can be formatted in different context with slightly different results
	/// </summary>
	public enum UhtPropertyTextType
	{
		/// <summary>
		/// Generic type
		/// </summary>
		Generic,

		/// <summary>
		/// Generic function argument or return value
		/// </summary>
		GenericFunctionArgOrRetVal,

		/// <summary>
		/// Generic function argument or return value implementation (specific to booleans and will always return "bool")
		/// </summary>
		GenericFunctionArgOrRetValImpl,

		/// <summary>
		/// Class function argument or return value
		/// </summary>
		ClassFunctionArgOrRetVal,

		/// <summary>
		/// Event function argument or return value
		/// </summary>
		EventFunctionArgOrRetVal,

		/// <summary>
		/// Interface function argument or return value
		/// </summary>
		InterfaceFunctionArgOrRetVal,

		/// <summary>
		/// Sparse property declaration
		/// </summary>
		Sparse,

		/// <summary>
		/// Sparse property short name
		/// </summary>
		SparseShort,

		/// <summary>
		/// Class or structure member
		/// </summary>
		ExportMember,

		/// <summary>
		/// Members of the event parameters structure used by code generation to invoke events
		/// </summary>
		EventParameterMember,

		/// <summary>
		/// Members of the event parameters structure used by code generation to invoke functions
		/// </summary>
		EventParameterFunctionMember,

		/// <summary>
		/// Instance of the property is being constructed
		/// </summary>
		Construction,

		/// <summary>
		/// Used to get the type argument for a function thunk.  This is used for P_GET_ARRAY_*
		/// </summary>
		FunctionThunkParameterArrayType,

		/// <summary>
		/// If the P_GET macro requires an argument, this is used to fetch that argument
		/// </summary>
		FunctionThunkParameterArgType,

		/// <summary>
		/// Used to get the return type for function thunks
		/// </summary>
		FunctionThunkRetVal,

		/// <summary>
		/// Basic RigVM type
		/// </summary>
		RigVMType,

		/// <summary>
		/// Type expected in a getter/setter argument list
		/// </summary>
		GetterSetterArg,
		
		/// <summary>
		/// Type expected from a getter
		/// </summary>
		GetterRetVal,
		
		/// <summary>
		/// Type expected as a setter argument
		/// </summary>
		SetterParameterArgType,
	}

	/// <summary>
	/// Extension methods for the property text type
	/// </summary>
	public static class UhtPropertyTextTypeExtensions
	{

		/// <summary>
		/// Test to see if the text type is for a function
		/// </summary>
		/// <param name="textType">Type of text</param>
		/// <returns>True if the text type is a function</returns>
		public static bool IsParameter(this UhtPropertyTextType textType)
		{
			return
				textType == UhtPropertyTextType.GenericFunctionArgOrRetVal ||
				textType == UhtPropertyTextType.GenericFunctionArgOrRetValImpl ||
				textType == UhtPropertyTextType.ClassFunctionArgOrRetVal ||
				textType == UhtPropertyTextType.EventFunctionArgOrRetVal ||
				textType == UhtPropertyTextType.InterfaceFunctionArgOrRetVal ||
				textType == UhtPropertyTextType.SetterParameterArgType;
		}
		
		/// <summary>
		/// Test to see if the text type is for a getter retVal or setter argType
		/// </summary>
		/// <param name="textType">Type of text</param>
		/// <returns>True if the text type is a retVal or argType</returns>
		public static bool IsGetOrSet(this UhtPropertyTextType textType)
		{
			return
				textType == UhtPropertyTextType.GetterSetterArg ||
				textType == UhtPropertyTextType.GetterRetVal ||
				textType == UhtPropertyTextType.SetterParameterArgType;
		}
	}

	//ETSTODO - This can be removed since FunctionThunkParameterArgType is only used in conjunction with UhtPGetArgumentType
	/// <summary>
	/// Specifies how the PGet argument type is to be formatted
	/// </summary>
	public enum UhtPGetArgumentType
	{
		/// <summary>
		/// </summary>
		None,

		/// <summary>
		/// </summary>
		EngineClass,

		/// <summary>
		/// </summary>
		TypeText,
	}

	/// <summary>
	/// Property member context provides extra context when formatting the member declaration and definitions.
	/// </summary>
	public interface IUhtPropertyMemberContext
	{
		/// <summary>
		/// The outer/owning structure
		/// </summary>
		public UhtStruct OuterStruct { get; }

		/// <summary>
		/// The outer/owning structure source name.  In some cases, this can differ from the OuterStruct.SourceName
		/// </summary>
		public string OuterStructSourceName { get; }

		/// <summary>
		/// Name of the statics block for static definitions for the outer object
		/// </summary>
		public string StaticsName { get; }

		/// <summary>
		/// Prefix to apply to declaration names
		/// </summary>
		public string NamePrefix { get; }

		/// <summary>
		/// Suffix to add to the declaration name
		/// </summary>
		public string MetaDataSuffix { get; }

		/// <summary>
		/// Return the hash code for a given object
		/// </summary>
		/// <param name="obj">Object in question</param>
		/// <returns>Hash code</returns>
		public uint GetTypeHash(UhtObject obj);

		/// <summary>
		/// Return the singleton name for the given object
		/// </summary>
		/// <param name="obj">Object in question</param>
		/// <param name="registered">If true, the singleton that returns the registered object is returned.</param>
		/// <returns>Singleton function name</returns>
		public string GetSingletonName(UhtObject? obj, bool registered);
	}

	/// <summary>
	/// Property settings is a transient object used during the parsing of properties
	/// </summary>
	public class UhtPropertySettings
	{

		/// <summary>
		/// Source name of the property
		/// </summary>
		public string SourceName { get; set; } = String.Empty;

		/// <summary>
		/// Engine name of the property
		/// </summary>
		public string EngineName { get; set; } = String.Empty;

		/// <summary>
		/// Property's meta data
		/// </summary>
		public UhtMetaData MetaData { get; set; } = UhtMetaData.Empty;

		/// <summary>
		/// Property outer object
		/// </summary>
		public UhtType Outer { get; set; }

		/// <summary>
		/// Line number of the property declaration
		/// </summary>
		public int LineNumber { get; set; }

		/// <summary>
		/// Property category
		/// </summary>
		public UhtPropertyCategory PropertyCategory { get; set; }

		/// <summary>
		/// Engine property flags
		/// </summary>
		public EPropertyFlags PropertyFlags { get; set; }

		/// <summary>
		/// Property flags not allowed by the context of the property parsing
		/// </summary>
		public EPropertyFlags DisallowPropertyFlags { get; set; }

		/// <summary>
		/// UHT specified property flags
		/// </summary>
		public UhtPropertyExportFlags PropertyExportFlags { get; set; }

		/// <summary>
		/// #define scope of where the property exists
		/// </summary>
		public UhtDefineScope DefineScope { get; set; }

		/// <summary>
		/// Allocator used for containers
		/// </summary>
		public UhtPropertyAllocator Allocator { get; set; }

		/// <summary>
		/// Options for property parsing
		/// </summary>
		public UhtPropertyOptions Options { get; set; }

		/// <summary>
		/// Property pointer type
		/// </summary>
		public UhtPointerType PointerType { get; set; }

		/// <summary>
		/// Replication notify name
		/// </summary>
		public string? RepNotifyName { get; set; }

		/// <summary>
		/// If set, the array size of the property
		/// </summary>
		public string? ArrayDimensions { get; set; }

		/// <summary>
		/// Getter method
		/// </summary>
		public string? Setter { get; set; }

		/// <summary>
		/// Setter method
		/// </summary>
		public string? Getter { get; set; }

		/// <summary>
		/// Default value of the property
		/// </summary>
		[SuppressMessage("Usage", "CA2227:Collection properties should be read only", Justification = "")]
		public List<UhtToken>? DefaultValueTokens { get; set; }

		/// <summary>
		/// If true, the property is a bit field
		/// </summary>
		public bool IsBitfield { get; set; }

		/// <summary>
		/// Construct a new, uninitialized version of the property settings
		/// </summary>
#pragma warning disable CS8618 // Non-nullable field must contain a non-null value when exiting constructor. Consider declaring as nullable.
		public UhtPropertySettings()
#pragma warning restore CS8618 // Non-nullable field must contain a non-null value when exiting constructor. Consider declaring as nullable.
		{
		}

		/// <summary>
		/// Construct property settings based on the property settings for a parent container
		/// </summary>
		/// <param name="parentPropertySettings">Parent container property</param>
		/// <param name="sourceName">Name of the property</param>
		/// <param name="messageSite">Message site used to construct meta data object</param>
		public void Reset(UhtPropertySettings parentPropertySettings, string sourceName, IUhtMessageSite messageSite)
		{
			SourceName = sourceName;
			EngineName = sourceName;
			MetaData = new UhtMetaData(messageSite, parentPropertySettings.Outer.Session.Config);
			Outer = parentPropertySettings.Outer;
			LineNumber = parentPropertySettings.LineNumber;
			PropertyCategory = parentPropertySettings.PropertyCategory;
			PropertyFlags = parentPropertySettings.PropertyFlags;
			DisallowPropertyFlags = parentPropertySettings.DisallowPropertyFlags;
			PropertyExportFlags = UhtPropertyExportFlags.Public;
			DefineScope = parentPropertySettings.DefineScope;
			RepNotifyName = null;
			Allocator = UhtPropertyAllocator.Default;
			Options = parentPropertySettings.Options;
			PointerType = UhtPointerType.None;
			ArrayDimensions = null;
			DefaultValueTokens = null;
			Setter = null;
			Getter = null;
			IsBitfield = false;
		}

		/// <summary>
		/// Reset the property settings.  Used on a cached property settings object
		/// </summary>
		/// <param name="outer">Outer/owning type</param>
		/// <param name="lineNumber">Line number of property</param>
		/// <param name="propertyCategory">Category of property</param>
		/// <param name="disallowPropertyFlags">Property flags that are not allowed</param>
		public void Reset(UhtType outer, int lineNumber, UhtPropertyCategory propertyCategory, EPropertyFlags disallowPropertyFlags)
		{
			SourceName = String.Empty;
			EngineName = String.Empty;
			MetaData = new UhtMetaData(outer, outer.Session.Config);
			Outer = outer;
			LineNumber = lineNumber;
			PropertyCategory = propertyCategory;
			PropertyFlags = EPropertyFlags.None;
			DisallowPropertyFlags = disallowPropertyFlags;
			DefineScope = UhtDefineScope.None;
			PropertyExportFlags = UhtPropertyExportFlags.Public;
			RepNotifyName = null;
			Allocator = UhtPropertyAllocator.Default;
			PointerType = UhtPointerType.None;
			ArrayDimensions = null;
			DefaultValueTokens = null;
			Setter = null;
			Getter = null;
			IsBitfield = false;
		}

		/// <summary>
		/// Reset property settings based on the given property.  Used to prepare a cached property settings for parsing.
		/// </summary>
		/// <param name="property">Source property</param>
		/// <param name="options">Property options</param>
		/// <exception cref="UhtIceException">Thrown if the input property doesn't have an outer</exception>
		public void Reset(UhtProperty property, UhtPropertyOptions options)
		{
			if (property.Outer == null)
			{
				throw new UhtIceException("Property must have an outer specified");
			}
			SourceName = property.SourceName;
			EngineName = property.EngineName;
			MetaData = property.MetaData;
			Outer = property.Outer;
			LineNumber = property.LineNumber;
			PropertyCategory = property.PropertyCategory;
			PropertyFlags = property.PropertyFlags;
			DisallowPropertyFlags = property.DisallowPropertyFlags;
			PropertyExportFlags = property.PropertyExportFlags;
			DefineScope = property.DefineScope;
			Allocator = property.Allocator;
			Options = options;
			PointerType = property.PointerType;
			RepNotifyName = property.RepNotifyName;
			ArrayDimensions = property.ArrayDimensions;
			DefaultValueTokens = property.DefaultValueTokens;
			Setter = property.Setter;
			Getter = property.Getter;
			IsBitfield = property.IsBitfield;
		}
	}

	/// <summary>
	/// Represents FProperty fields
	/// </summary>
	[UhtEngineClass(Name = "Field", IsProperty = true)] // This is here just so it is defined
	[UhtEngineClass(Name = "Property", IsProperty = true)]
	public abstract class UhtProperty : UhtType
	{
		#region Constants
		/// <summary>
		/// Collection of recognized casts when parsing array dimensions
		/// </summary>
		private static readonly string[] s_casts = new string[]
		{
			"(uint32)",
			"(int32)",
			"(uint16)",
			"(int16)",
			"(uint8)",
			"(int8)",
			"(int)",
			"(unsigned)",
			"(signed)",
			"(unsigned int)",
			"(signed int)",
			"static_cast<uint32>",
			"static_cast<int32>",
			"static_cast<uint16>",
			"static_cast<int16>",
			"static_cast<uint8>",
			"static_cast<int8>",
			"static_cast<int>",
			"static_cast<unsigned>",
			"static_cast<signed>",
			"static_cast<unsigned int>",
			"static_cast<signed int>",
		};

		/// <summary>
		/// Collection of invalid names for parameters 
		/// </summary>
		private static readonly HashSet<string> s_invalidParamNames = new(StringComparer.OrdinalIgnoreCase) { "self" };

		/// <summary>
		/// Standard object flags for properties
		/// </summary>
		protected const string ObjectFlags = "RF_Public|RF_Transient|RF_MarkAsNative";

		/// <summary>
		/// Prefix used when declaring P_GET_ parameters
		/// </summary>
		protected const string FunctionParameterThunkPrefix = "Z_Param_";
		#endregion

		#region Protected property configuration used to simplify implementation details
		/// <summary>
		/// Simple native CPP type text.  Will not include any template arguments
		/// </summary>
		protected abstract string CppTypeText { get; }

		/// <summary>
		/// P_GET_ macro name
		/// </summary>
		protected abstract string PGetMacroText { get; }

		/// <summary>
		/// If true, then references must be passed without a pointer
		/// </summary>
		protected virtual bool PGetPassAsNoPtr => false;

		/// <summary>
		/// Type of the PGet argument if one is required
		/// </summary>
		protected virtual UhtPGetArgumentType PGetTypeArgument => UhtPGetArgumentType.None;
		#endregion

		/// <summary>
		/// Property category
		/// </summary>
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public UhtPropertyCategory PropertyCategory { get; set; } = UhtPropertyCategory.Member;

		/// <summary>
		/// Engine property flags
		/// </summary>
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public EPropertyFlags PropertyFlags { get; set; }

		/// <summary>
		/// Capabilities of the property.  Use caps system instead of testing for specific property types.
		/// </summary>
		[JsonIgnore]
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public UhtPropertyCaps PropertyCaps { get; set; }

		/// <summary>
		/// Engine flags that are disallowed on this property
		/// </summary>
		[JsonIgnore]
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public EPropertyFlags DisallowPropertyFlags { get; set; } = EPropertyFlags.None;

		/// <summary>
		/// UHT specified property flags
		/// </summary>
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public UhtPropertyExportFlags PropertyExportFlags { get; set; } = UhtPropertyExportFlags.Public;

		/// <summary>
		/// Reference type of the property
		/// </summary>
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public UhtPropertyRefQualifier RefQualifier { get; set; } = UhtPropertyRefQualifier.None;

		/// <summary>
		/// Pointer type of the property
		/// </summary>
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public UhtPointerType PointerType { get; set; } = UhtPointerType.None;

		/// <summary>
		/// Allocator to be used with containers
		/// </summary>
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public UhtPropertyAllocator Allocator { get; set; } = UhtPropertyAllocator.Default;

		/// <summary>
		/// Replication notify name
		/// </summary>
		public string? RepNotifyName { get; set; } = null;

		/// <summary>
		/// Fixed array size
		/// </summary>
		public string? ArrayDimensions { get; set; } = null;

		/// <summary>
		/// Property setter
		/// </summary>
		public string? Setter { get; set; } = null;

		/// <summary>
		/// Property getter
		/// </summary>
		public string? Getter { get; set; } = null;

		/// <summary>
		/// Default value of property
		/// </summary>
		[JsonIgnore]
		[SuppressMessage("Usage", "CA2227:Collection properties should be read only", Justification = "<Pending>")]
		public List<UhtToken>? DefaultValueTokens { get; set; } = null;

		/// <summary>
		/// If true, this property is a bit field
		/// </summary>
		public bool IsBitfield { get; set; } = false;

		///<inheritdoc/>
		[JsonIgnore]
		public override UhtEngineType EngineType => UhtEngineType.Property;

		///<inheritdoc/>
		[JsonIgnore]
		public override bool Deprecated => PropertyFlags.HasAnyFlags(EPropertyFlags.Deprecated);

		///<inheritdoc/>
		[JsonIgnore]
		protected override UhtSpecifierValidatorTable? SpecifierValidatorTable
		{
			get
			{
				switch (PropertyCategory)
				{
					case UhtPropertyCategory.Member:
						return Session.GetSpecifierValidatorTable(UhtTableNames.PropertyMember);
					case UhtPropertyCategory.RegularParameter:
					case UhtPropertyCategory.ReplicatedParameter:
					case UhtPropertyCategory.Return:
						return Session.GetSpecifierValidatorTable(UhtTableNames.PropertyArgument);
					default:
						throw new UhtIceException("Unknown property category type");
				}
			}
		}

		/// <summary>
		/// If true, the property is a fixed/static array
		/// </summary>
		[JsonIgnore]
		public bool IsStaticArray => !String.IsNullOrEmpty(ArrayDimensions);

		/// <summary>
		/// If true, the property is editor only
		/// </summary>
		[JsonIgnore]
		public bool IsEditorOnlyProperty => PropertyFlags.HasAnyFlags(EPropertyFlags.EditorOnly);

		/// <summary>
		/// Construct a new property
		/// </summary>
		/// <param name="outer">Outer type of the property</param>
		/// <param name="lineNumber">Line number where property was declared</param>
		protected UhtProperty(UhtType outer, int lineNumber) : base(outer, lineNumber)
		{
			PropertyFlags = EPropertyFlags.None;
			PropertyCaps = UhtPropertyCaps.CanBeContainerValue | UhtPropertyCaps.CanBeContainerKey | UhtPropertyCaps.CanHaveConfig;
		}

		/// <summary>
		/// Construct a new property
		/// </summary>
		/// <param name="propertySettings">Property settings from parsing</param>
		protected UhtProperty(UhtPropertySettings propertySettings) : base(propertySettings.Outer, propertySettings.LineNumber, propertySettings.MetaData)
		{
			SourceName = propertySettings.SourceName;
			// Engine name defaults to source name.  If it doesn't match what is coming in, then set it.
			if (propertySettings.EngineName.Length > 0 && propertySettings.SourceName != propertySettings.EngineName)
			{
				EngineName = propertySettings.EngineName;
			}
			PropertyCategory = propertySettings.PropertyCategory;
			PropertyFlags = propertySettings.PropertyFlags;
			DisallowPropertyFlags = propertySettings.DisallowPropertyFlags;
			PropertyExportFlags = propertySettings.PropertyExportFlags;
			DefineScope = propertySettings.DefineScope;
			Allocator = propertySettings.Allocator;
			PointerType = propertySettings.PointerType;
			RepNotifyName = propertySettings.RepNotifyName;
			ArrayDimensions = propertySettings.ArrayDimensions;
			DefaultValueTokens = propertySettings.DefaultValueTokens;
			Getter = propertySettings.Getter;
			Setter = propertySettings.Setter;
			IsBitfield = propertySettings.IsBitfield;
			PropertyCaps = UhtPropertyCaps.CanBeContainerValue | UhtPropertyCaps.CanBeContainerKey | UhtPropertyCaps.CanHaveConfig;
		}

		#region Text and code generation support
		/// <summary>
		/// Internal version of AppendText.  Don't append any text to the builder to get the default behavior
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="textType">Text type of where the property is being referenced</param>
		/// <param name="isTemplateArgument">If true, this property is a template arguments</param>
		/// <returns>Destination builder</returns>
		public virtual StringBuilder AppendText(StringBuilder builder, UhtPropertyTextType textType, bool isTemplateArgument = false)
		{
			// By default, we assume it will be just the simple text
			return builder.Append(CppTypeText);
		}

		/// <summary>
		/// Append the full declaration including such things as property name and const<amp/> requirements
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="textType">Text type of where the property is being referenced</param>
		/// <param name="skipParameterName">If true, do not include the property name</param>
		/// <returns>Destination builder</returns>
		public virtual StringBuilder AppendFullDecl(StringBuilder builder, UhtPropertyTextType textType, bool skipParameterName = false)
		{
			UhtPropertyCaps caps = PropertyCaps;

			bool isParameter = textType.IsParameter();
			bool isGetOrSet = textType.IsGetOrSet();
			bool isInterfaceProp = this is UhtInterfaceProperty;

			// When do we need a leading const:
			// 1) If this is a object or object ptr property and the referenced class is const
			// 2) If this is not an out parameter or reference parameter then,
			//		if this is a parameter
			//		AND - if this is a const param OR (if this is an interface property and not an out param)
			// 3) If this is a parameter without array dimensions, must be passed by reference, but not an out parameter or const param
			bool passCppArgsByRef = caps.HasAnyFlags(UhtPropertyCaps.PassCppArgsByRef);
			bool isConstParam = isParameter && (PropertyFlags.HasAnyFlags(EPropertyFlags.ConstParm) || (isInterfaceProp && !PropertyFlags.HasAnyFlags(EPropertyFlags.OutParm)));
			bool isConstArgsByRef = isParameter && ArrayDimensions == null && passCppArgsByRef && !PropertyFlags.HasAnyFlags(EPropertyFlags.ConstParm | EPropertyFlags.OutParm);
			bool isOnConstClass = false;
			if (this is UhtObjectProperty objectProperty)
			{
				isOnConstClass = objectProperty.Class.ClassFlags.HasAnyFlags(EClassFlags.Const);
			}
			bool shouldHaveRef = PropertyFlags.HasAnyFlags(EPropertyFlags.OutParm | EPropertyFlags.ReferenceParm);

			bool constAtTheBeginning = isGetOrSet | isOnConstClass || isConstArgsByRef || (isConstParam && !shouldHaveRef);
			if (constAtTheBeginning)
			{
				builder.Append("const ");
			}

			AppendText(builder, textType);

			bool fromConstClass = false;
			if (textType == UhtPropertyTextType.ExportMember && Outer != null)
			{
				if (Outer is UhtClass outerClass)
				{
					fromConstClass = outerClass.ClassFlags.HasAnyFlags(EClassFlags.Const);
				}
			}
			bool constAtTheEnd = fromConstClass || (isConstParam && shouldHaveRef);
			if (constAtTheEnd)
			{
				builder.Append(" const");
			}

			shouldHaveRef = textType == UhtPropertyTextType.SetterParameterArgType || (textType == UhtPropertyTextType.GetterRetVal && passCppArgsByRef); 
			if (shouldHaveRef || isParameter && ArrayDimensions == null && (passCppArgsByRef || PropertyFlags.HasAnyFlags(EPropertyFlags.OutParm | EPropertyFlags.ReferenceParm)))
			{
				builder.Append('&');
			}

			builder.Append(' ');
			if (!skipParameterName)
			{
				builder.Append(SourceName);
			}

			if (ArrayDimensions != null)
			{
				builder.Append('[').Append(ArrayDimensions).Append(']');
			}
			return builder;
		}

		/// <summary>
		/// Append the required code to declare the properties meta data
		/// </summary>
		/// <param name="builder">Output builder</param>
		/// <param name="context">Current context</param>
		/// <param name="name">Name of the property.  This is needed in some cases where the name in the declarations doesn't match the property name.</param>
		/// <param name="nameSuffix">Suffix to the property name</param>
		/// <param name="tabs">Number of tabs prefix the line with</param>
		/// <returns>Output builder</returns>
		public virtual StringBuilder AppendMetaDataDecl(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, int tabs)
		{
			return builder.AppendMetaDataDecl(this, context.NamePrefix, name, nameSuffix, context.MetaDataSuffix, tabs);
		}

		/// <summary>
		/// Append the required code to declare the property as a member
		/// </summary>
		/// <param name="builder">Output builder</param>
		/// <param name="context">Current context</param>
		/// <param name="name">Name of the property.  This is needed in some cases where the name in the declarations doesn't match the property name.</param>
		/// <param name="nameSuffix">Suffix to the property name</param>
		/// <param name="tabs">Number of tabs prefix the line with</param>
		/// <returns>Output builder</returns>
		public abstract StringBuilder AppendMemberDecl(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, int tabs);

		/// <summary>
		/// Append the required code to declare the property as a member
		/// </summary>
		/// <param name="builder">Output builder</param>
		/// <param name="context">Current context</param>
		/// <param name="name">Name of the property.  This is needed in some cases where the name in the declarations doesn't match the property name.</param>
		/// <param name="nameSuffix">Suffix to the property name</param>
		/// <param name="tabs">Number of tabs prefix the line with</param>
		/// <param name="paramsStructName">Structure name</param>
		/// <returns>Output builder</returns>
		public static StringBuilder AppendMemberDecl(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, int tabs, string paramsStructName)
		{
			builder.AppendTabs(tabs).Append("static const UECodeGen_Private::").Append(paramsStructName).Append(' ').AppendNameDecl(context, name, nameSuffix).Append(";\r\n");
			return builder;
		}

		/// <summary>
		/// Append the required code to define the property as a member
		/// </summary>
		/// <param name="builder">Output builder</param>
		/// <param name="context">Context of the call</param>
		/// <param name="name">Name of the property.  This is needed in some cases where the name in the declarations doesn't match the property name.</param>
		/// <param name="nameSuffix">Suffix to the property name</param>
		/// <param name="offset">Offset of the property</param>
		/// <param name="tabs">Number of tabs prefix the line with</param>
		/// <returns>Output builder</returns>
		public abstract StringBuilder AppendMemberDef(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, string? offset, int tabs);

		/// <summary>
		/// Append the required start of code to define the property as a member
		/// </summary>
		/// <param name="builder">Output builder</param>
		/// <param name="context">Context of the call</param>
		/// <param name="name">Name of the property.  This is needed in some cases where the name in the declarations doesn't match the property name.</param>
		/// <param name="nameSuffix">Suffix to the property name</param>
		/// <param name="offset">Offset of the property</param>
		/// <param name="tabs">Number of tabs prefix the line with</param>
		/// <param name="paramsStructName">Structure name</param>
		/// <param name="paramsGenFlags">Structure flags</param>
		/// <param name="appendOffset">If true, add the offset parameter</param>
		/// <returns>Output builder</returns>
		public StringBuilder AppendMemberDefStart(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, string? offset, int tabs,
			string paramsStructName, string paramsGenFlags, bool appendOffset = true)
		{
			builder
				.AppendTabs(tabs)
				.Append("const UECodeGen_Private::").Append(paramsStructName).Append(' ')
				.AppendNameDef(context, name, nameSuffix).Append(" = { ")
				.AppendUTF8LiteralString(EngineName).Append(", ")
				.AppendNotifyFunc(this).Append(", ")
				.AppendFlags(PropertyFlags).Append(", ")
				.Append(paramsGenFlags).Append(", ")
				.Append(ObjectFlags).Append(", ");

			if (PropertyExportFlags.HasAnyFlags(UhtPropertyExportFlags.SetterFound))
			{
				builder.Append('&').Append(Outer!.SourceName).Append("::").AppendPropertySetterWrapperName(this).Append(", ");
			}
			else
			{
				builder.Append("nullptr, ");
			}

			if (PropertyExportFlags.HasAnyFlags(UhtPropertyExportFlags.GetterFound))
			{
				builder.Append('&').Append(Outer!.SourceName).Append("::").AppendPropertyGetterWrapperName(this).Append(", ");
			}
			else
			{
				builder.Append("nullptr, ");
			}

			builder.AppendArrayDim(this, context).Append(", ");

			if (appendOffset)
			{
				if (!String.IsNullOrEmpty(offset))
				{
					builder.Append(offset).Append(", ");
				}
				else
				{
					builder.Append("STRUCT_OFFSET(").Append(context.OuterStructSourceName).Append(", ").Append(SourceName).Append("), ");
				}
			}
			return builder;
		}

		/// <summary>
		/// Append the required end of code to define the property as a member
		/// </summary>
		/// <param name="builder">Output builder</param>
		/// <param name="context">Context of the call</param>
		/// <param name="name">Name of the property.  This is needed in some cases where the name in the declarations doesn't match the property name.</param>
		/// <param name="nameSuffix">Suffix to the property name</param>
		/// <returns>Output builder</returns>
		protected StringBuilder AppendMemberDefEnd(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix)
		{
			builder
				.AppendMetaDataParams(this, context, name, nameSuffix)
				.Append(" };")
				.AppendObjectHashes(this, context)
				.Append("\r\n");
			return builder;
		}

		/// <summary>
		/// Append the a type reference to the member definition
		/// </summary>
		/// <param name="builder">Output builder</param>
		/// <param name="context">Context of the call</param>
		/// <param name="obj">Referenced object</param>
		/// <param name="registered">True if the registered singleton name is to be used</param>
		/// <param name="appendNull">True if a "nullptr" is to be appended if the object is null</param>
		/// <returns>Output builder</returns>
		protected static StringBuilder AppendMemberDefRef(StringBuilder builder, IUhtPropertyMemberContext context, UhtObject? obj, bool registered, bool appendNull = false)
		{
			if (obj != null)
			{
				builder.AppendSingletonName(context, obj, registered).Append(", ");
			}
			else if (appendNull)
			{
				builder.Append("nullptr, ");
			}
			return builder;
		}

		/// <summary>
		/// Append the required code to add the properties to a pointer array
		/// </summary>
		/// <param name="builder">Output builder</param>
		/// <param name="context">Context of the call</param>
		/// <param name="name">Name of the property.  This is needed in some cases where the name in the declarations doesn't match the property name.</param>
		/// <param name="nameSuffix">Suffix to the property name</param>
		/// <param name="tabs">Number of tabs prefix the line with</param>
		/// <returns>Output builder</returns>
		public virtual StringBuilder AppendMemberPtr(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, int tabs)
		{
			builder.AppendTabs(tabs).Append("(const UECodeGen_Private::FPropertyParamsBase*)&").AppendNameDef(context, name, nameSuffix).Append(",\r\n");
			return builder;
		}

		/// <summary>
		/// Append a P_GET macro
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <returns>Destination builder</returns>
		public virtual StringBuilder AppendFunctionThunkParameterGet(StringBuilder builder)
		{
			builder.Append("P_GET_");
			if (ArrayDimensions != null)
			{
				builder.Append("ARRAY");
			}
			else
			{
				builder.Append(PGetMacroText);
			}
			if (PropertyFlags.HasAnyFlags(EPropertyFlags.OutParm))
			{
				if (PGetPassAsNoPtr)
				{
					builder.Append("_REF_NO_PTR");
				}
				else
				{
					builder.Append("_REF");
				}
			}
			builder.Append('(');
			if (ArrayDimensions != null)
			{
				builder.AppendFunctionThunkParameterArrayType(this).Append(',');
			}
			else
			{
				switch (PGetTypeArgument)
				{
					case UhtPGetArgumentType.None:
						break;

					case UhtPGetArgumentType.EngineClass:
						builder.Append('F').Append(EngineClassName).Append(',');
						break;

					case UhtPGetArgumentType.TypeText:
						builder.AppendPropertyText(this, UhtPropertyTextType.FunctionThunkParameterArgType).Append(',');
						break;
				}
			}
			builder.AppendFunctionThunkParameterName(this).Append(')');
			return builder;
		}

		/// <summary>
		/// Append the text for a function thunk call argument
		/// </summary>
		/// <param name="builder">Output builder</param>
		/// <returns>Output builder</returns>
		public virtual StringBuilder AppendFunctionThunkParameterArg(StringBuilder builder)
		{
			return builder.AppendFunctionThunkParameterName(this);
		}

		/// <summary>
		/// Apppend the name of a function thunk paramter
		/// </summary>
		/// <param name="builder">Output builder</param>
		/// <returns>Output builder</returns>
		public StringBuilder AppendFunctionThunkParameterName(StringBuilder builder)
		{
			builder.Append(FunctionParameterThunkPrefix);
			if (PropertyFlags.HasAnyFlags(EPropertyFlags.OutParm))
			{
				builder.Append("Out_");
			}
			builder.Append(EngineName);
			return builder;
		}

		/// <summary>
		/// Append the appropriate values to initialize the property to a "NULL" value;
		/// </summary>
		/// <param name="builder"></param>
		/// <param name="isInitializer"></param>
		/// <returns></returns>
		public abstract StringBuilder AppendNullConstructorArg(StringBuilder builder, bool isInitializer);

		/// <summary>
		/// Return the basic declaration type text for user facing messages
		/// </summary>
		/// <returns></returns>
		public string GetUserFacingDecl()
		{
			StringBuilder builder = new();
			AppendText(builder, UhtPropertyTextType.Generic);
			return builder.ToString();
		}

		/// <summary>
		/// Return the RigVM type
		/// </summary>
		/// <returns></returns>
		public string GetRigVMType()
		{
			using BorrowStringBuilder borrower = new(StringBuilderCache.Small);
			AppendText(borrower.StringBuilder, UhtPropertyTextType.RigVMType);
			return borrower.StringBuilder.ToString();
		}

		/// <summary>
		/// Appends any applicable objects and child properties
		/// </summary>
		/// <param name="builder">Output builder</param>
		/// <param name="startingLength">Initial length of the builder prior to appending the hashes</param>
		/// <param name="context">Context used to lookup the hashes</param>
		public virtual void AppendObjectHashes(StringBuilder builder, int startingLength, IUhtPropertyMemberContext context)
		{
		}
		#endregion

		#region Parsing support
		/// <summary>
		/// Parse a default value for the property and return a sanitized string representation.
		/// 
		/// All tokens in the token reader must be consumed.  Otherwise the default value will be considered to be invalid.
		/// </summary>
		/// <param name="defaultValueReader">Reader containing the default value</param>
		/// <param name="innerDefaultValue">Sanitized representation of default value</param>
		/// <returns>True if a default value was parsed.</returns>
		public abstract bool SanitizeDefaultValue(IUhtTokenReader defaultValueReader, StringBuilder innerDefaultValue);
		#endregion

		#region Resolution support
		/// <inheritdoc/>
		protected override bool ResolveSelf(UhtResolvePhase phase)
		{
			bool result = base.ResolveSelf(phase);

			switch (phase)
			{
				case UhtResolvePhase.Final:
					if (ArrayDimensions != null)
					{
						ReadOnlySpan<char> dim = ArrayDimensions.AsSpan();

						bool again;
						do
						{
							again = false;

							// Remove any irrelevant whitespace
							dim = dim.Trim();

							// Remove any outer brackets
							if (dim.Length > 0 && dim[0] == '(')
							{
								for (int index = 1, depth = 1; index < dim.Length; ++index)
								{
									if (dim[index] == ')')
									{
										if (--depth == 0)
										{
											if (index == dim.Length - 1)
											{
												dim = dim[1..index];
												again = true;
											}
											break;
										}
									}
									else if (dim[index] == '(')
									{
										++depth;
									}
								}
							}

							// Remove any well known casts
							if (dim.Length > 0)
							{
								foreach (string cast in s_casts)
								{
									if (dim.StartsWith(cast))
									{
										dim = dim[cast.Length..];
										again = true;
										break;
									}
								}
							}
						} while (again && dim.Length > 0);

						//COMPATIBILITY-TODO - This method is more robust, but causes differences.  See UhtSession for future
						// plans on fix in 
						//// Now try to see if this is an enum
						//UhtEnum? Enum = null;
						//int Sep = Dim.IndexOf("::");
						//if (Sep >= 0)
						//{
						//	//COMPATIBILITY-TODO "Bob::Type::Value" did not generate a match with the old code
						//	if (Dim.LastIndexOf("::") != Sep)
						//	{
						//		break;
						//	}
						//	Dim = Dim.Slice(0, Sep);
						//}
						//else
						//{
						//	Enum = Session.FindRegularEnumValue(Dim.ToString());
						//}
						//if (Enum == null)
						//{
						//	Enum = Session.FindType(Outer, UhtFindOptions.Enum | UhtFindOptions.SourceName, Dim.ToString()) as UhtEnum;
						//}
						//if (Enum != null)
						//{
						//	MetaData.Add(UhtNames.ArraySizeEnum, Enum.GetPathName());
						//}

						if (dim.Length > 0 && !UhtFCString.IsDigit(dim[0]))
						{
							UhtEnum? enumObj = Session.FindRegularEnumValue(dim.ToString());
							enumObj ??= Session.FindType(Outer, UhtFindOptions.Enum | UhtFindOptions.SourceName, dim.ToString()) as UhtEnum;
							if (enumObj != null)
							{
								MetaData.Add(UhtNames.ArraySizeEnum, enumObj.PathName);
							}
						}
					}
					break;
			}
			return result;
		}

		/// <summary>
		/// Check properties to see if any instances are referenced.
		/// This method does NOT cache the result.
		/// </summary>
		/// <param name="deepScan">If true, the ScanForInstancedReferenced method on the properties will also be called.</param>
		/// <returns></returns>
		public virtual bool ScanForInstancedReferenced(bool deepScan)
		{
			return false;
		}
		#endregion

		#region Validation support
		/// <inheritdoc/>
		protected override UhtValidationOptions Validate(UhtValidationOptions options)
		{
			options = base.Validate(options);

			// The outer should never be null...
			if (Outer == null)
			{
				return options;
			}

			// Shadowing checks are done at this level, not in the properties themselves
			if (options.HasAnyFlags(UhtValidationOptions.Shadowing) && PropertyCategory != UhtPropertyCategory.Return)
			{

				// First check for duplicate name in self and then duplicate name in parents
				UhtType? existing = Outer.FindType(UhtFindOptions.PropertyOrFunction | UhtFindOptions.EngineName | UhtFindOptions.SelfOnly, EngineName);
				if (existing == this)
				{
					existing = Outer.FindType(UhtFindOptions.PropertyOrFunction | UhtFindOptions.EngineName | UhtFindOptions.ParentsOnly | UhtFindOptions.NoGlobal | UhtFindOptions.NoIncludes, EngineName);
				}

				if (existing != null && existing != this)
				{
					if (existing is UhtProperty existingProperty)
					{
						//@TODO: This exception does not seem sound either, but there is enough existing code that it will need to be
						// fixed up first before the exception it is removed.
						bool existingPropDeprecated = existingProperty.PropertyFlags.HasAnyFlags(EPropertyFlags.Deprecated);
						bool newPropDeprecated = PropertyCategory == UhtPropertyCategory.Member && PropertyFlags.HasAnyFlags(EPropertyFlags.Deprecated);

						//@TODO: Work around to the issue that C++ UHT does not recognize shadowing errors between function parameters and properties that are 
						// in the same structure but appear later in the file
						// Note: The old UHT would check for all property type where this version only checks for member variables.
						// In the old code if you defined the property after the function with argument with the same name, UHT would
						// not issue an error.  However, if the property was defined PRIOR to the function with the matching argument name,
						// UHT would generate an error.
						bool appearsLater = existingProperty.HeaderFile == HeaderFile && existingProperty.LineNumber > LineNumber;
						if (!newPropDeprecated && !existingPropDeprecated && !appearsLater)
						{
							LogShadowingError(existingProperty);
						}
					}
					else if (existing is UhtFunction existingFunction)
					{
						if (PropertyCategory == UhtPropertyCategory.Member)
						{
							LogShadowingError(existingFunction);
						}
					}
				}
			}

			Validate((UhtStruct)Outer, this, options);
			return options;
		}

		private void LogShadowingError(UhtType shadows)
		{
			this.LogError($"{PropertyCategory.GetHintText()}: '{SourceName}' cannot be defined in '{Outer?.SourceName}' as it is already defined in scope '{shadows.Outer?.SourceName}' (shadowing is not allowed)");
		}

		/// <summary>
		/// Validate the property settings
		/// </summary>
		/// <param name="outerStruct">The outer structure for the property.  For properties inside containers, this will be the owning struct of the container</param>
		/// <param name="outermostProperty">Outer most property being validated.  For properties in containers, 
		/// this will be the container property.  For properties outside of containers or the container itself, this will be the property.</param>
		/// <param name="options"></param>
		public virtual void Validate(UhtStruct outerStruct, UhtProperty outermostProperty, UhtValidationOptions options)
		{
			// Check for deprecation
			if (options.HasAnyFlags(UhtValidationOptions.Deprecated) && !Deprecated)
			{
				ValidateDeprecated();
			}

			// Validate the types allowed with arrays
			if (ArrayDimensions != null)
			{
				switch (PropertyCategory)
				{
					case UhtPropertyCategory.Return:
						this.LogError("Arrays aren't allowed as return types");
						break;

					case UhtPropertyCategory.RegularParameter:
					case UhtPropertyCategory.ReplicatedParameter:
						this.LogError("Arrays aren't allowed as function parameters");
						break;
				}

				if (this is UhtContainerBaseProperty)
				{
					this.LogError("Static arrays of containers are not allowed");
				}

				if (this is UhtBoolProperty)
				{
					this.LogError("Bool arrays are not allowed");
				}
			}

			if (!options.HasAnyFlags(UhtValidationOptions.IsKey) && PropertyFlags.HasAnyFlags(EPropertyFlags.PersistentInstance) && !PropertyCaps.HasAnyFlags(UhtPropertyCaps.CanBeInstanced))
			{
				this.LogError("'Instanced' is only allowed on an object property, an array of objects, a set of objects, or a map with an object value type.");
			}

			if (PropertyFlags.HasAnyFlags(EPropertyFlags.Config) && !PropertyCaps.HasAnyFlags(UhtPropertyCaps.CanHaveConfig))
			{
				this.LogError("Not allowed to use 'config' with object variables");
			}

			if (PropertyFlags.HasAnyFlags(EPropertyFlags.ExposeOnSpawn))
			{
				if (PropertyFlags.HasAnyFlags(EPropertyFlags.DisableEditOnInstance))
				{
					this.LogWarning("Property cannot have both 'DisableEditOnInstance' and 'ExposeOnSpawn' flags");
				}
				if (!PropertyFlags.HasAnyFlags(EPropertyFlags.BlueprintVisible))
				{
					this.LogWarning("Property cannot have 'ExposeOnSpawn' without 'BlueprintVisible' flag.");
				}
			}

			if (PropertyFlags.HasAnyFlags(EPropertyFlags.BlueprintAssignable) && !PropertyCaps.HasAnyFlags(UhtPropertyCaps.CanBeBlueprintAssignable))
			{
				this.LogError("'BlueprintAssignable' is only allowed on multicast delegate properties");
			}

			if (PropertyFlags.HasAnyFlags(EPropertyFlags.BlueprintCallable) && !PropertyCaps.HasAnyFlags(UhtPropertyCaps.CanBeBlueprintCallable))
			{
				this.LogError("'BlueprintCallable' is only allowed on multicast delegate properties");
			}

			if (PropertyFlags.HasAnyFlags(EPropertyFlags.BlueprintAuthorityOnly) && !PropertyCaps.HasAnyFlags(UhtPropertyCaps.CanBeBlueprintAuthorityOnly))
			{
				this.LogError("'BlueprintAuthorityOnly' is only allowed on multicast delegate properties");
			}

			// Check for invalid transients
			EPropertyFlags transients = PropertyFlags & (EPropertyFlags.DuplicateTransient | EPropertyFlags.TextExportTransient | EPropertyFlags.NonPIEDuplicateTransient);
			if (transients != 0 && outerStruct is not UhtClass)
			{
				this.LogError($"'{String.Join(", ", transients.ToStringList(false))}' specifier(s) are only allowed on class member variables");
			}

			if (!options.HasAnyFlags(UhtValidationOptions.IsKeyOrValue))
			{
				switch (PropertyCategory)
				{
					case UhtPropertyCategory.Member:
						ValidateMember(outerStruct, options);
						break;

					case UhtPropertyCategory.RegularParameter:
					case UhtPropertyCategory.ReplicatedParameter:
						ValidateFunctionArgument((UhtFunction)outerStruct, options);
						break;

					case UhtPropertyCategory.Return:
						break;
				}
			}
			return;
		}

		/// <summary>
		/// Validate that we don't reference any deprecated classes
		/// </summary>
		public virtual void ValidateDeprecated()
		{
		}

		/// <summary>
		/// Verify function argument
		/// </summary>
		protected virtual void ValidateFunctionArgument(UhtFunction func, UhtValidationOptions options)
		{
			if (func.FunctionFlags.HasAnyFlags(EFunctionFlags.Net))
			{
				if (PropertyFlags.HasExactFlags(EPropertyFlags.ReferenceParm | EPropertyFlags.ConstParm, EPropertyFlags.ReferenceParm))
				{
					this.LogError($"Replicated parameters cannot be passed by non-const reference");
				}

				if (func.FunctionFlags.HasAnyFlags(EFunctionFlags.NetRequest))
				{
					if (PropertyFlags.HasExactFlags(EPropertyFlags.OutParm | EPropertyFlags.RepSkip, EPropertyFlags.OutParm))
					{
						// This is difficult to trigger since NotReplicated also sets the property category
						this.LogError("Service request functions cannot contain out parameters, unless marked NotReplicated");
					}
				}
				else
				{
					if (PropertyFlags.HasAnyFlags(EPropertyFlags.OutParm))
					{
						this.LogError("Replicated functions cannot contain out parameters");
					}

					if (PropertyFlags.HasAnyFlags(EPropertyFlags.RepSkip))
					{
						// This is difficult to trigger since NotReplicated also sets the property category
						this.LogError("Only service request functions cannot contain NotReplicated parameters");
					}
				}
			}

			// The following checks are not performed on the value of a container
			if (func.FunctionFlags.HasAnyFlags(EFunctionFlags.BlueprintEvent | EFunctionFlags.BlueprintCallable))
			{
				// Check that the parameter name is valid and does not conflict with pre-defined types
				if (s_invalidParamNames.Contains(SourceName))
				{
					this.LogError($"Parameter name '{SourceName}' in function is invalid, '{SourceName}' is a reserved name.");
				}
			}
		}

		/// <summary>
		/// Validate member settings
		/// </summary>
		/// <param name="structObj">Containing struct.  This is either a UhtScriptStruct or UhtClass</param>
		/// <param name="options">Validation options</param>
		protected virtual void ValidateMember(UhtStruct structObj, UhtValidationOptions options)
		{
			if (!options.HasAnyFlags(UhtValidationOptions.IsKeyOrValue))
			{
				// First check if the category was specified at all and if the property was exposed to the editor.
				if (!MetaData.TryGetValue(UhtNames.Category, out string? category))
				{
					if (PropertyFlags.HasAnyFlags(EPropertyFlags.Edit | EPropertyFlags.BlueprintVisible))
					{
						if (Package.IsPartOfEngine)
						{
							this.LogError("An explicit Category specifier is required for any property exposed to the editor or Blueprints in an Engine module.");
						}
					}
				}

				// If the category was specified explicitly, it wins
				if (!String.IsNullOrEmpty(category) && !PropertyFlags.HasAnyFlags(EPropertyFlags.Edit | EPropertyFlags.BlueprintVisible |
					EPropertyFlags.BlueprintAssignable | EPropertyFlags.BlueprintCallable))
				{
					this.LogWarning("Property has a Category set but is not exposed to the editor or Blueprints with EditAnywhere, BlueprintReadWrite, " +
						"VisibleAnywhere, BlueprintReadOnly, BlueprintAssignable, BlueprintCallable keywords.");
				}
			}

			// Make sure that editblueprint variables are editable
			if (!PropertyFlags.HasAnyFlags(EPropertyFlags.Edit))
			{
				if (PropertyFlags.HasAnyFlags(EPropertyFlags.DisableEditOnInstance))
				{
					this.LogError("Property cannot have 'DisableEditOnInstance' without being editable");
				}

				if (PropertyFlags.HasAnyFlags(EPropertyFlags.DisableEditOnTemplate))
				{
					this.LogError("Property cannot have 'DisableEditOnTemplate' without being editable");
				}
			}

			string exposeOnSpawnValue = MetaData.GetValueOrDefault(UhtNames.ExposeOnSpawn);
			if (exposeOnSpawnValue.Equals("true", StringComparison.OrdinalIgnoreCase) && !PropertyCaps.HasAnyFlags(UhtPropertyCaps.CanExposeOnSpawn))
			{
				this.LogError("ExposeOnSpawn - Property cannot be exposed");
			}

			if (PropertyExportFlags.HasAnyFlags(UhtPropertyExportFlags.FieldNotify))
			{
				if (Outer is UhtClass classObj)
				{
					if (classObj.ClassType != UhtClassType.Class)
					{
						this.LogError("FieldNofity are not valid on UInterface.");
					}
				}
				else
				{
					this.LogError("FieldNofity property are only valid as UClass member variable.");
				}
			}
		}

		/// <summary>
		/// Generate an error if the class has been deprecated
		/// </summary>
		/// <param name="classObj">Class to check</param>
		protected void ValidateDeprecatedClass(UhtClass? classObj)
		{
			if (classObj == null)
			{
				return;
			}

			if (!classObj.Deprecated)
			{
				return;
			}

			if (PropertyCategory == UhtPropertyCategory.Member)
			{
				this.LogError($"Property is using a deprecated class: '{classObj.SourceName}'.  Property should be marked deprecated as well.");
			}
			else
			{
				this.LogError($"Function is using a deprecated class: '{classObj.SourceName}'.  Function should be marked deprecated as well.");
			}
		}

		/// <summary>
		/// Check to see if the property is valid as a member of a networked structure
		/// </summary>
		/// <param name="referencingProperty">The property referencing the structure property.  All error should be logged on the referencing property.</param>
		/// <returns>True if the property is valid, false if not.  If the property is not valid, an error should also be generated.</returns>
		public virtual bool ValidateStructPropertyOkForNet(UhtProperty referencingProperty)
		{
			return true;
		}

		/// <summary>
		/// Test to see if this property references something that requires the argument to be marked as const
		/// </summary>
		/// <param name="errorType">If const is required, returns the type that is forcing the const</param>
		/// <returns>True if the argument must be marked as const</returns>
		public virtual bool MustBeConstArgument([NotNullWhen(true)] out UhtType? errorType)
		{
			errorType = null;
			return false;
		}

		/// <summary>
		/// Test to see if the property contains any editor only properties.  This does not 
		/// test to see if the property itself is editor only.  This is implemented for struct properties.
		/// </summary>
		/// <returns></returns>
		public virtual bool ContainsEditorOnlyProperties()
		{
			return false;
		}
		#endregion

		#region Reference support
		/// <inheritdoc/>
		public override void CollectReferences(IUhtReferenceCollector collector)
		{
			CollectReferencesInternal(collector, false);
			if (PropertyFlags.HasAnyFlags(EPropertyFlags.ParmFlags))
			{
				collector.AddForwardDeclaration(GetForwardDeclarations());
			}
		}

		/// <summary>
		/// Collect the references for the property.  This method is used by container properties to
		/// collect the contained property's references.
		/// </summary>
		/// <param name="collector">Reference collector</param>
		/// <param name="isTemplateProperty">If true, this is a property in a container</param>
		public virtual void CollectReferencesInternal(IUhtReferenceCollector collector, bool isTemplateProperty)
		{
		}

		/// <summary>
		/// Return a string of all the forward declarations.
		/// This method should be removed once C++ UHT is removed.
		/// This information can be collected via CollectReferences.
		/// </summary>
		/// <returns></returns>
		public virtual string? GetForwardDeclarations()
		{
			return null;
		}

		/// <summary>
		/// Enumerate all reference types.  This method is used exclusively by FindNoExportStructsRecursive
		/// </summary>
		/// <returns>Type enumerator</returns>
		public virtual IEnumerable<UhtType> EnumerateReferencedTypes()
		{
			return Enumerable.Empty<UhtType>();
		}
		#endregion

		#region Incremental GC Support
		/// <summary>
		/// Determines whether or not GC barriers need to run after passing this to a function
		/// </summary>
		/// <returns>True if GC barriers need to run</returns>				
		public bool NeedsGCBarrierWhenPassedToFunction(UhtFunction function)
		{
			if (RefQualifier != UhtPropertyRefQualifier.NonConstRef)
			{
				return false;
			}
			return NeedsGCBarrierWhenPassedToFunctionImpl(function);
		}
		
		/// <summary>
		/// Customization point for subclasses for NeedsGCBarrierWhenPassedToFunction
		/// </summary>
		/// <returns>True if GC barriers need to run</returns>		
		protected virtual bool NeedsGCBarrierWhenPassedToFunctionImpl(UhtFunction function)
		{
			return false;
		}
		#endregion
		
		#region Helper methods
		/// <summary>
		/// Generate a new name suffix based on the current suffix and the new suffix
		/// </summary>
		/// <param name="outerSuffix">Current suffix</param>
		/// <param name="newSuffix">Suffix to be added</param>
		/// <returns>Combination of the two suffixes</returns>
		protected static string GetNameSuffix(string outerSuffix, string newSuffix)
		{
			return String.IsNullOrEmpty(outerSuffix) ? newSuffix : $"{outerSuffix}{newSuffix}";
		}

		/// <summary>
		/// Test to see if the two properties are the same type
		/// </summary>
		/// <param name="other">Other property to test</param>
		/// <returns>True if the properies are the same type</returns>
		public abstract bool IsSameType(UhtProperty other);

		/// <summary>
		/// Test to see if the two properties are the same type and ConstParm/OutParm flags somewhat match
		/// </summary>
		/// <param name="other">The other property to test</param>
		/// <returns></returns>
		public bool MatchesType(UhtProperty other)
		{
			if (PropertyFlags.HasAnyFlags(EPropertyFlags.OutParm))
			{
				if (!other.PropertyFlags.HasAnyFlags(EPropertyFlags.OutParm))
				{
					return false;
				}

				if (PropertyFlags.HasAnyFlags(EPropertyFlags.ConstParm) && !other.PropertyFlags.HasAnyFlags(EPropertyFlags.ConstParm))
				{
					return false;
				}
			}
			if (IsStaticArray != other.IsStaticArray)
			{
				return false;
			}
			return IsSameType(other);
		}
		#endregion
	}

	/// <summary>
	/// Assorted StringBuilder extensions for properties
	/// </summary>
	public static class UhtPropertyStringBuilderExtensions
	{

		/// <summary>
		/// Add the given property text
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="property">Property in question</param>
		/// <param name="textType">Type of text to append</param>
		/// <param name="isTemplateArgument">If true, this is a template argument property</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendPropertyText(this StringBuilder builder, UhtProperty property, UhtPropertyTextType textType, bool isTemplateArgument = false)
		{
			if (textType == UhtPropertyTextType.GetterRetVal || textType == UhtPropertyTextType.SetterParameterArgType)
			{
				return builder.AppendFullDecl(property, textType, true);
			}

			return property.AppendText(builder, textType, isTemplateArgument);
		}

		/// <summary>
		/// Append the member declaration
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="property">Property in question</param>
		/// <param name="context">Context of the property</param>
		/// <param name="name">Property name</param>
		/// <param name="nameSuffix">Name suffix</param>
		/// <param name="tabs">Number of tabs in the formatting</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendMemberDecl(this StringBuilder builder, UhtProperty property, IUhtPropertyMemberContext context, string name, string nameSuffix, int tabs)
		{
			return property.AppendMemberDecl(builder, context, name, nameSuffix, tabs);
		}

		/// <summary>
		/// Append the member definition
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="property">Property in question</param>
		/// <param name="context">Context of the property</param>
		/// <param name="name">Property name</param>
		/// <param name="nameSuffix">Name suffix</param>
		/// <param name="offset">Offset of the property in the parent</param>
		/// <param name="tabs">Number of tabs in the formatting</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendMemberDef(this StringBuilder builder, UhtProperty property, IUhtPropertyMemberContext context, string name, string nameSuffix, string? offset, int tabs)
		{
			return property.AppendMemberDef(builder, context, name, nameSuffix, offset, tabs);
		}

		/// <summary>
		/// Append the member pointer
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="property">Property in question</param>
		/// <param name="context">Context of the property</param>
		/// <param name="name">Property name</param>
		/// <param name="nameSuffix">Name suffix</param>
		/// <param name="tabs">Number of tabs in the formatting</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendMemberPtr(this StringBuilder builder, UhtProperty property, IUhtPropertyMemberContext context, string name, string nameSuffix, int tabs)
		{
			return property.AppendMemberPtr(builder, context, name, nameSuffix, tabs);
		}

		/// <summary>
		/// Append the full declaration including such things as const, *, and &amp;
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="property">Property in question</param>
		/// <param name="textType">Type of text to append</param>
		/// <param name="skipParameterName">If true, don't append the parameter name</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendFullDecl(this StringBuilder builder, UhtProperty property, UhtPropertyTextType textType, bool skipParameterName)
		{
			return property.AppendFullDecl(builder, textType, skipParameterName);
		}

		/// <summary>
		/// Append the function thunk parameter get
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="property">Property in question</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendFunctionThunkParameterGet(this StringBuilder builder, UhtProperty property)
		{
			return property.AppendFunctionThunkParameterGet(builder);
		}

		/// <summary>
		/// Append the function thunk parameter array type
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="property">Property in question</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendFunctionThunkParameterArrayType(this StringBuilder builder, UhtProperty property)
		{
			return property.AppendText(builder, UhtPropertyTextType.FunctionThunkParameterArrayType);
		}

		/// <summary>
		/// Append the function thunk parameter argument
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="property">Property in question</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendFunctionThunkParameterArg(this StringBuilder builder, UhtProperty property)
		{
			return property.AppendFunctionThunkParameterArg(builder);
		}

		/// <summary>
		/// Append the function thunk return
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="property">Property in question</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendFunctionThunkReturn(this StringBuilder builder, UhtProperty property)
		{
			return property.AppendText(builder, UhtPropertyTextType.FunctionThunkRetVal);
		}

		/// <summary>
		/// Append the function thunk parameter name
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="property">Property in question</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendFunctionThunkParameterName(this StringBuilder builder, UhtProperty property)
		{
			return property.AppendFunctionThunkParameterName(builder);
		}

		/// <summary>
		/// Append the sparse type
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="property">Property in question</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendSparse(this StringBuilder builder, UhtProperty property)
		{
			return property.AppendText(builder, UhtPropertyTextType.Sparse);
		}

		/// <summary>
		/// Append the property's null constructor arg
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="property">Property in question</param>
		/// <param name="isInitializer">If true this is in an initializer context.</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendNullConstructorArg(this StringBuilder builder, UhtProperty property, bool isInitializer)
		{
			property.AppendNullConstructorArg(builder, isInitializer);
			return builder;
		}

		/// <summary>
		/// Append the replication notify function or a 'nullptr'
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="property">Property in question</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendNotifyFunc(this StringBuilder builder, UhtProperty property)
		{
			if (property.RepNotifyName != null)
			{
				builder.AppendUTF8LiteralString(property.RepNotifyName);
			}
			else
			{
				builder.Append("nullptr");
			}
			return builder;
		}

		/// <summary>
		/// Append the parameter flags
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="propertyFlags">Property flags</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendFlags(this StringBuilder builder, EPropertyFlags propertyFlags)
		{
			propertyFlags &= ~EPropertyFlags.ComputedFlags;
			return builder.Append("(EPropertyFlags)0x").AppendFormat(CultureInfo.InvariantCulture, "{0:x16}", (ulong)propertyFlags);
		}

		/// <summary>
		/// Append the property array dimensions as a CPP_ARRAY_DIM macro or '1' if not a fixed array.
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="property">Property in question</param>
		/// <param name="context">Context of the property</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendArrayDim(this StringBuilder builder, UhtProperty property, IUhtPropertyMemberContext context)
		{
			if (property.ArrayDimensions != null)
			{
				builder.Append("CPP_ARRAY_DIM(").Append(property.SourceName).Append(", ").Append(context.OuterStruct.SourceName).Append(')');
			}
			else
			{
				builder.Append('1');
			}
			return builder;
		}

		/// <summary>
		/// Given an object, append the hash (if applicable) to the builder
		/// </summary>
		/// <param name="builder">Output builder</param>
		/// <param name="referingType">Type asking for an object hash</param>
		/// <param name="startingLength">Initial length of the builder prior to appending the hashes</param>
		/// <param name="context">Context used to lookup the hashes</param>
		/// <param name="obj">Object being appended</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendObjectHash(this StringBuilder builder, int startingLength, UhtType referingType, IUhtPropertyMemberContext context, UhtObject? obj)
		{
			if (obj == null)
			{
				return builder;
			}
			else if (obj is UhtClass classObj)
			{
				if (classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.NoExport))
				{
					return builder;
				}
			}
			else if (obj is UhtScriptStruct scriptStruct)
			{
				if (scriptStruct.ScriptStructFlags.HasAnyFlags(EStructFlags.NoExport))
				{
					return builder;
				}
			}

			builder.Append(builder.Length == startingLength ? " // " : " ");
			uint hash = context.GetTypeHash(obj);
			if (hash == 0)
			{
				string type = referingType is UhtProperty ? "property" : "object";
				referingType.LogError($"The {type} \"{referingType.SourceName}\" references type \"{obj.SourceName}\" but the code generation hash is zero.  Check for circular dependencies or missing includes.");
			}
			builder.Append(context.GetTypeHash(obj));
			return builder;
		}

		/// <summary>
		/// Given an object, append the hash (if applicable) to the builder
		/// </summary>
		/// <param name="builder">Output builder</param>
		/// <param name="referingType">Type asking for an object hash</param>
		/// <param name="context">Context used to lookup the hashes</param>
		/// <param name="obj">Object being appended</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendObjectHash(this StringBuilder builder, UhtType referingType, IUhtPropertyMemberContext context, UhtObject? obj)
		{
			return builder.AppendObjectHash(builder.Length, referingType, context, obj);
		}

		/// <summary>
		/// Append the object hashes for all referenced objects
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="property">Property in question</param>
		/// <param name="context">Context of the property</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendObjectHashes(this StringBuilder builder, UhtProperty property, IUhtPropertyMemberContext context)
		{
			property.AppendObjectHashes(builder, builder.Length, context);
			return builder;
		}

		/// <summary>
		/// Append the singleton name for the given type
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="context">Context of the property</param>
		/// <param name="type">Type to append</param>
		/// <param name="registered">If true, append the registered type singleton.</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendSingletonName(this StringBuilder builder, IUhtPropertyMemberContext context, UhtObject? type, bool registered)
		{
			return builder.Append(context.GetSingletonName(type, registered));
		}

		/// <summary>
		/// Append the getter wrapper name
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="property">Property in question</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendPropertyGetterWrapperName(this StringBuilder builder, UhtProperty property)
		{
			return builder.Append("Get").Append(property.SourceName).Append("_WrapperImpl");
		}

		/// <summary>
		/// Append the setter wrapper name
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="property">Property in question</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendPropertySetterWrapperName(this StringBuilder builder, UhtProperty property)
		{
			return builder.Append("Set").Append(property.SourceName).Append("_WrapperImpl");
		}
	}
}
