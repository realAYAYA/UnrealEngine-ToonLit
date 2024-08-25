// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics.CodeAnalysis;
using System.Text;
using System.Text.Json.Serialization;
using EpicGames.Core;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Tokenizer;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Types
{

	/// <summary>
	/// Type of boolean
	/// </summary>
	public enum UhtBoolType
	{

		/// <summary>
		/// Native bool
		/// </summary>
		Native,

		/// <summary>
		/// Used for all bitmask uint booleans
		/// </summary>
		UInt8,

		/// <summary>
		/// Currently unused
		/// </summary>
		UInt16,

		/// <summary>
		/// Currently unused
		/// </summary>
		UInt32,

		/// <summary>
		/// Currently unused
		/// </summary>
		UInt64,
	}

	/// <summary>
	/// Represents the FBoolProperty engine type
	/// </summary>
	[UnrealHeaderTool]
	[UhtEngineClass(Name = "BoolProperty", IsProperty = true)]
	public class UhtBoolProperty : UhtProperty
	{
		/// <inheritdoc/>
		public override string EngineClassName => "BoolProperty";

		/// <inheritdoc/>
		protected override string CppTypeText
		{
			get
			{
				switch (BoolType)
				{
					case UhtBoolType.Native:
						return "bool";
					case UhtBoolType.UInt8:
						return "uint8";
					case UhtBoolType.UInt16:
						return "uint16";
					case UhtBoolType.UInt32:
						return "uint32";
					case UhtBoolType.UInt64:
						return "uint64";
					default:
						throw new UhtIceException("Unexpected boolean type");
				}
			}
		}

		/// <inheritdoc/>
		protected override string PGetMacroText
		{
			get
			{
				switch (BoolType)
				{
					case UhtBoolType.Native:
						return "UBOOL";
					case UhtBoolType.UInt8:
						return "UBOOL8";
					case UhtBoolType.UInt16:
						return "UBOOL16";
					case UhtBoolType.UInt32:
						return "UBOOL32";
					case UhtBoolType.UInt64:
						return "UBOOL64";
					default:
						throw new UhtIceException("Unexpected boolean type");
				}
			}
		}

		/// <summary>
		/// If true, the boolean is a native bool and not a UBOOL
		/// </summary>
		protected bool IsNativeBool => BoolType == UhtBoolType.Native;

		/// <summary>
		/// Type of the boolean
		/// </summary>
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public UhtBoolType BoolType { get; }

		/// <summary>
		/// Construct a new boolean property
		/// </summary>
		/// <param name="propertySettings">Property settings</param>
		/// <param name="boolType">Type of the boolean</param>
		public UhtBoolProperty(UhtPropertySettings propertySettings, UhtBoolType boolType) : base(propertySettings)
		{
			PropertyCaps |= UhtPropertyCaps.RequiresNullConstructorArg | UhtPropertyCaps.IsParameterSupportedByBlueprint |
				UhtPropertyCaps.IsMemberSupportedByBlueprint | UhtPropertyCaps.SupportsRigVM;
			if (boolType == UhtBoolType.Native || boolType == UhtBoolType.UInt8)
			{
				PropertyCaps |= UhtPropertyCaps.CanExposeOnSpawn;
			}
			BoolType = boolType;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendText(StringBuilder builder, UhtPropertyTextType textType, bool isTemplateArgument = false)
		{
			switch (textType)
			{
				case UhtPropertyTextType.Generic:
				case UhtPropertyTextType.Construction:
				case UhtPropertyTextType.FunctionThunkParameterArrayType:
				case UhtPropertyTextType.FunctionThunkRetVal:
				case UhtPropertyTextType.RigVMType:
				case UhtPropertyTextType.ExportMember:
				case UhtPropertyTextType.GenericFunctionArgOrRetVal:
					builder.Append(CppTypeText);
					break;

				case UhtPropertyTextType.Sparse:
				case UhtPropertyTextType.SparseShort:
				case UhtPropertyTextType.GenericFunctionArgOrRetValImpl:
				case UhtPropertyTextType.ClassFunctionArgOrRetVal:
				case UhtPropertyTextType.EventFunctionArgOrRetVal:
				case UhtPropertyTextType.InterfaceFunctionArgOrRetVal:
				case UhtPropertyTextType.EventParameterMember:
				case UhtPropertyTextType.EventParameterFunctionMember:
				case UhtPropertyTextType.GetterSetterArg:
					builder.Append("bool");
					break;
			}
			return builder;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDecl(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, int tabs)
		{
			if (Outer is not UhtProperty)
			{
				builder.AppendTabs(tabs).Append("static void ").AppendNameDecl(context, name, nameSuffix).Append("_SetBit(void* Obj);\r\n");
			}
			builder.AppendTabs(tabs).Append("static const UECodeGen_Private::").Append("FBoolPropertyParams").Append(' ').AppendNameDecl(context, name, nameSuffix).Append(";\r\n");
			return builder;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDef(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, string? offset, int tabs)
		{
			if (Outer == context.OuterStruct)
			{
				builder.AppendTabs(tabs).Append("void ").AppendNameDef(context, name, nameSuffix).Append("_SetBit(void* Obj)\r\n");
				builder.AppendTabs(tabs).Append("{\r\n");
				builder.AppendTabs(tabs + 1).Append("((").Append(context.OuterStructSourceName).Append("*)Obj)->").Append(SourceName).Append(" = 1;\r\n");
				builder.AppendTabs(tabs).Append("}\r\n");
			}

			AppendMemberDefStart(builder, context, name, nameSuffix, offset, tabs, "FBoolPropertyParams",
				IsNativeBool ?
				"UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool" :
				"UECodeGen_Private::EPropertyGenFlags::Bool ",
				false);

			builder.Append("sizeof(").Append(CppTypeText).Append("), ");

			if (Outer == context.OuterStruct)
			{
				builder
					.Append("sizeof(").Append(context.OuterStructSourceName).Append("), ")
					.Append('&').AppendNameDef(context, name, nameSuffix).Append("_SetBit, ");
			}
			else
			{
				builder.Append("0, nullptr, ");
			}

			AppendMemberDefEnd(builder, context, name, nameSuffix);
			return builder;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendFullDecl(StringBuilder builder, UhtPropertyTextType textType, bool skipParameterName = false)
		{
			AppendText(builder, textType);

			//@todo we currently can't have out bools.. so this isn't really necessary, but eventually out bools may be supported, so leave here for now
			if (textType.IsParameter() && PropertyFlags.HasAnyFlags(EPropertyFlags.OutParm))
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
			else if (textType == UhtPropertyTextType.ExportMember && !IsNativeBool)
			{
				builder.Append(":1");
			}
			return builder;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendNullConstructorArg(StringBuilder builder, bool isInitializer)
		{
			builder.Append("false");
			return builder;
		}

		/// <inheritdoc/>
		public override bool SanitizeDefaultValue(IUhtTokenReader defaultValueReader, StringBuilder innerDefaultValue)
		{
			UhtToken identifier = defaultValueReader.GetIdentifier();
			if (identifier.IsValue("true") || identifier.IsValue("false"))
			{
				innerDefaultValue.Append(identifier.Value.ToString());
				return true;
			}
			return false;
		}

		/// <inheritdoc/>
		public override bool IsSameType(UhtProperty other)
		{
			// We don't test BoolType.
			return other is UhtBoolProperty;
		}

		#region Keyword
		[UhtPropertyType(Keyword = "bool", Options = UhtPropertyTypeOptions.Simple | UhtPropertyTypeOptions.Immediate)]
		[SuppressMessage("CodeQuality", "IDE0051:Remove unused private members", Justification = "Attribute accessed method")]
		[SuppressMessage("Style", "IDE0060:Remove unused parameter", Justification = "Attribute accessed method")]
		private static UhtProperty? BoolProperty(UhtPropertyResolvePhase resolvePhase, UhtPropertySettings propertySettings, IUhtTokenReader tokenReader, UhtToken matchedToken)
		{
			if (propertySettings.IsBitfield)
			{
				tokenReader.LogError("bool bitfields are not supported.");
				return null;
			}
			return new UhtBoolProperty(propertySettings, UhtBoolType.Native);
		}
		#endregion
	}
}
