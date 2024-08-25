// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
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
	/// FByteProperty
	/// </summary>
	[UnrealHeaderTool]
	[UhtEngineClass(Name = "ByteProperty", IsProperty = true)]
	public class UhtByteProperty : UhtNumericProperty
	{
		/// <summary>
		/// Referenced enumeration (TEnumAsByte)
		/// </summary>
		[JsonConverter(typeof(UhtNullableTypeSourceNameJsonConverter<UhtEnum>))]
		public UhtEnum? Enum { get; set; }

		/// <inheritdoc/>
		public override string EngineClassName => "ByteProperty";

		/// <inheritdoc/>
		protected override string CppTypeText => "uint8";

		/// <inheritdoc/>
		protected override string PGetMacroText => Enum == null || Enum.CppForm != UhtEnumCppForm.EnumClass ? "PROPERTY" : "ENUM";

		/// <inheritdoc/>
		protected override UhtPGetArgumentType PGetTypeArgument => Enum == null || Enum.CppForm != UhtEnumCppForm.EnumClass ? UhtPGetArgumentType.EngineClass : UhtPGetArgumentType.TypeText;

		/// <summary>
		/// Construct a new property
		/// </summary>
		/// <param name="propertySettings">Property settings</param>
		/// <param name="enumObj">Optional referenced enum</param>
		public UhtByteProperty(UhtPropertySettings propertySettings, UhtEnum? enumObj = null) : base(propertySettings)
		{
			Enum = enumObj;
			PropertyCaps |= UhtPropertyCaps.CanExposeOnSpawn | UhtPropertyCaps.IsParameterSupportedByBlueprint |
				UhtPropertyCaps.IsMemberSupportedByBlueprint | UhtPropertyCaps.SupportsRigVM;
			if (Enum != null)
			{
				PropertyCaps |= UhtPropertyCaps.IsRigVMEnum;
				PropertyCaps |= UhtPropertyCaps.IsRigVMEnumAsByte;
			}
		}

		/// <inheritdoc/>
		public override IEnumerable<UhtType> EnumerateReferencedTypes()
		{
			if (Enum != null)
			{
				yield return Enum;
			}
		}

		/// <inheritdoc/>
		public override void CollectReferencesInternal(IUhtReferenceCollector collector, bool templateProperty)
		{
			collector.AddCrossModuleReference(Enum, true);
		}

		/// <inheritdoc/>
		public override StringBuilder AppendText(StringBuilder builder, UhtPropertyTextType textType, bool isTemplateArgument)
		{
			if (Enum != null)
			{
				return UhtEnumProperty.AppendEnumText(builder, this, Enum, textType, isTemplateArgument);
			}
			else
			{
				return base.AppendText(builder, textType, isTemplateArgument);
			}
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDecl(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, int tabs)
		{
			return AppendMemberDecl(builder, context, name, nameSuffix, tabs, "FBytePropertyParams");
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDef(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, string? offset, int tabs)
		{
			AppendMemberDefStart(builder, context, name, nameSuffix, offset, tabs, "FBytePropertyParams", "UECodeGen_Private::EPropertyGenFlags::Byte");
			AppendMemberDefRef(builder, context, Enum, true, true);
			AppendMemberDefEnd(builder, context, name, nameSuffix);
			return builder;
		}

		/// <inheritdoc/>
		public override void AppendObjectHashes(StringBuilder builder, int startingLength, IUhtPropertyMemberContext context)
		{
			builder.AppendObjectHash(startingLength, this, context, Enum);
		}

		/// <inheritdoc/>
		public override StringBuilder AppendFunctionThunkParameterArg(StringBuilder builder)
		{
			if (Enum != null)
			{
				return UhtEnumProperty.AppendEnumFunctionThunkParameterArg(builder, this, Enum);
			}
			else
			{
				return base.AppendFunctionThunkParameterArg(builder);
			}
		}

		/// <inheritdoc/>
		public override bool SanitizeDefaultValue(IUhtTokenReader defaultValueReader, StringBuilder innerDefaultValue)
		{
			if (Enum != null)
			{
				return UhtEnumProperty.SanitizeEnumDefaultValue(this, Enum, defaultValueReader, innerDefaultValue);
			}

			int value = defaultValueReader.GetConstIntExpression();
			innerDefaultValue.Append(value);
			return value >= Byte.MinValue && value <= Byte.MaxValue;
		}

		/// <inheritdoc/>
		public override bool IsSameType(UhtProperty other)
		{
			if (other is UhtByteProperty otherByte)
			{
				return Enum == otherByte.Enum;
			}
			else if (other is UhtEnumProperty otherEnum)
			{
				return Enum == otherEnum.Enum;
			}
			return false;
		}

		/// <inheritdoc/>
		protected override void ValidateFunctionArgument(UhtFunction function, UhtValidationOptions options)
		{
			base.ValidateFunctionArgument(function, options);

			if (function.FunctionFlags.HasAnyFlags(EFunctionFlags.BlueprintEvent | EFunctionFlags.BlueprintCallable))
			{
				if (Enum != null && Enum.CppForm == UhtEnumCppForm.EnumClass && Enum.UnderlyingType != UhtEnumUnderlyingType.Uint8)
				{
					this.LogError("Invalid enum param for Blueprints - currently only uint8 supported");
				}
			}
		}

		#region Keyword
		[UhtPropertyType(Keyword = "uint8", Options = UhtPropertyTypeOptions.Simple | UhtPropertyTypeOptions.Immediate)]
		[SuppressMessage("CodeQuality", "IDE0051:Remove unused private members", Justification = "Attribute accessed method")]
		[SuppressMessage("Style", "IDE0060:Remove unused parameter", Justification = "Attribute accessed method")]
		private static UhtProperty? UInt8Property(UhtPropertyResolvePhase resolvePhase, UhtPropertySettings propertySettings, IUhtTokenReader tokenReader, UhtToken matchedToken)
		{
			if (propertySettings.IsBitfield)
			{
				return new UhtBoolProperty(propertySettings, UhtBoolType.UInt8);
			}
			else
			{
				return new UhtByteProperty(propertySettings);
			}
		}
		#endregion
	}
}
