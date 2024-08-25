// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.Json.Serialization;
using EpicGames.Core;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Types
{

	/// <summary>
	/// How the enumeration was declared
	/// </summary>
	public enum UhtEnumCppForm
	{
		/// <summary>
		/// enum Name {...}
		/// </summary>
		Regular,

		/// <summary>
		/// namespace Name { enum Type { ... } }
		/// </summary>
		Namespaced,

		/// <summary>
		/// enum class Name {...}
		/// </summary>
		EnumClass
	}

	/// <summary>
	/// Underlying type of the enumeration
	/// </summary>
	public enum UhtEnumUnderlyingType
	{

		/// <summary>
		/// Not specified
		/// </summary>
		Unspecified,

		/// <summary>
		/// Uint8
		/// </summary>
		Uint8,

		/// <summary>
		/// Uint16
		/// </summary>
		Uint16,

		/// <summary>
		/// Uint32
		/// </summary>
		Uint32,

		/// <summary>
		/// Uint64
		/// </summary>
		Uint64,

		/// <summary>
		/// Int8
		/// </summary>
		Int8,

		/// <summary>
		/// Int16
		/// </summary>
		Int16,

		/// <summary>
		/// Int32
		/// </summary>
		Int32,

		/// <summary>
		/// Int64
		/// </summary>
		Int64,

		/// <summary>
		/// Int
		/// </summary>
		Int,
	}

	/// <summary>
	/// Represents an enumeration value
	/// </summary>
	public struct UhtEnumValue
	{
		/// <summary>
		/// Name of the enumeration value
		/// </summary>
		public string Name { get; set; }

		/// <summary>
		/// Value of the enumeration or -1 if not parsed.
		/// </summary>
		public long Value { get; set; }
	}

	/// <summary>
	/// Represents a UEnum
	/// </summary>
	[UhtEngineClass(Name = "Enum")]
	public class UhtEnum : UhtField, IUhtMetaDataKeyConversion
	{
		/// <summary>
		/// Engine enumeration flags
		/// </summary>
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public EEnumFlags EnumFlags { get; set; } = EEnumFlags.None;

		/// <summary>
		/// C++ form of the enumeration
		/// </summary>
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public UhtEnumCppForm CppForm { get; set; } = UhtEnumCppForm.Regular;

		/// <summary>
		/// Underlying integer enumeration type
		/// </summary>
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public UhtEnumUnderlyingType UnderlyingType { get; set; } = UhtEnumUnderlyingType.Uint8;

		/// <summary>
		/// Full enumeration type.  For namespace enumerations, this includes the namespace name and the enum type name
		/// </summary>
		public string CppType { get; set; } = String.Empty;

		/// <summary>
		/// Collection of enumeration values
		/// </summary>
		public List<UhtEnumValue> EnumValues { get; }

		/// <inheritdoc/>
		[JsonIgnore]
		public override UhtEngineType EngineType => UhtEngineType.Enum;

		/// <inheritdoc/>
		public override string EngineClassName => "Enum";

		///<inheritdoc/>
		[JsonIgnore]
		protected override UhtSpecifierValidatorTable? SpecifierValidatorTable => Session.GetSpecifierValidatorTable(UhtTableNames.Enum);

		/// <summary>
		/// Construct a new enumeration
		/// </summary>
		/// <param name="outer">Outer type</param>
		/// <param name="lineNumber">Line number of declaration</param>
		public UhtEnum(UhtType outer, int lineNumber) : base(outer, lineNumber)
		{
			MetaData.KeyConversion = this;
			EnumValues = new List<UhtEnumValue>();
		}

		/// <summary>
		/// Test to see if the value is a known enum value
		/// </summary>
		/// <param name="value">Value in question</param>
		/// <returns>True if the value is known</returns>
		public bool IsValidEnumValue(long value)
		{
			foreach (UhtEnumValue enumValue in EnumValues)
			{
				if (enumValue.Value == value)
				{
					return true;
				}
			}
			return false;
		}

		/// <summary>
		/// Return the index of the given enumeration value name
		/// </summary>
		/// <param name="name">Value name in question</param>
		/// <returns>Index of the value or -1 if not found.</returns>
		public int GetIndexByName(string name)
		{
			name = CleanEnumValueName(name);
			for (int index = 0; index < EnumValues.Count; ++index)
			{
				if (EnumValues[index].Name == name)
				{
					return index;
				}
			}
			return -1;
		}

		/// <summary>
		/// Converts meta data name and index to a full meta data key name
		/// </summary>
		/// <param name="name">Meta data key name</param>
		/// <param name="nameIndex">Meta data key index</param>
		/// <returns>Meta data name with the enum value name</returns>
		public string GetMetaDataKey(string name, int nameIndex)
		{
			string enumName = EnumValues[nameIndex].Name;
			if (CppForm != UhtEnumCppForm.Regular)
			{
				int scopeIndex = enumName.IndexOf("::", StringComparison.Ordinal);
				if (scopeIndex >= 0)
				{
					enumName = enumName[(scopeIndex + 2)..];
				}
			}
			return $"{enumName}.{name}";
		}

		/// <summary>
		/// Given an enumeration value name, return the full enumeration name
		/// </summary>
		/// <param name="shortEnumName">Enum value name</param>
		/// <returns>If required, enum type name combined with value name.  Otherwise just the value name.</returns>
		/// <exception cref="UhtIceException">Unexpected enum form</exception>
		public string GetFullEnumName(string shortEnumName)
		{
			switch (CppForm)
			{
				case UhtEnumCppForm.Namespaced:
				case UhtEnumCppForm.EnumClass:
					return $"{SourceName}::{shortEnumName}";

				case UhtEnumCppForm.Regular:
					return shortEnumName;

				default:
					throw new UhtIceException("Unexpected UhtEnumCppForm value");
			}
		}

		/// <summary>
		/// Add a new enum value.
		/// </summary>
		/// <param name="shortEnumName">Name of the enum value.</param>
		/// <param name="value">Enumeration value or -1 if the value can't be determined.</param>
		/// <returns></returns>
		public int AddEnumValue(string shortEnumName, long value)
		{
			int enumIndex = EnumValues.Count;
			EnumValues.Add(new UhtEnumValue { Name = GetFullEnumName(shortEnumName), Value = value });
			return enumIndex;
		}

		/// <summary>
		/// Reconstruct the full enum name.  Any existing enumeration name will be stripped and replaced 
		/// with this enumeration name.
		/// </summary>
		/// <param name="name">Name to reconstruct.</param>
		/// <returns>Reconstructed enum name</returns>
		private string CleanEnumValueName(string name)
		{
			int lastColons = name.LastIndexOf("::", StringComparison.Ordinal);
			return lastColons == -1 ? GetFullEnumName(name) : GetFullEnumName(name[(lastColons + 2)..]);
		}

		#region Validation support
		///<inheritdoc/>
		protected override void ValidateDocumentationPolicy(UhtDocumentationPolicy policy)
		{
			if (policy.ClassOrStructCommentRequired)
			{
				if (!MetaData.ContainsKey(UhtNames.ToolTip))
				{
					this.LogError(MetaData.LineNumber, $"Enum '{SourceName}' does not provide a tooltip / comment (DocumentationPolicy)");
				}
			}

			Dictionary<string, string> toolTips = new(StringComparer.OrdinalIgnoreCase);
			for (int enumIndex = 0; enumIndex < EnumValues.Count; ++enumIndex)
			{
				if (!MetaData.TryGetValue(UhtNames.Name, enumIndex, out string? entryName))
				{
					continue;
				}

				if (!MetaData.TryGetValue(UhtNames.ToolTip, enumIndex, out string? toolTip))
				{
					this.LogError(MetaData.LineNumber, $"Enum entry '{SourceName}::{entryName}' does not provide a tooltip / comment (DocumentationPolicy)");
					continue;
				}

				if (toolTips.TryGetValue(toolTip, out string? dupName))
				{
					this.LogError(MetaData.LineNumber, $"Enum entries '{SourceName}::{entryName}' and '{SourceName}::{dupName}' have identical tooltips / comments (DocumentationPolicy)");
				}
				else
				{
					toolTips.Add(toolTip, entryName);
				}
			}
		}
		#endregion

		/// <inheritdoc/>
		public override void CollectReferences(IUhtReferenceCollector collector)
		{
			collector.AddExportType(this);
			collector.AddDeclaration(this, true);
			collector.AddCrossModuleReference(this, true);
			collector.AddCrossModuleReference(Package, true);
		}
	}
}
