// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Globalization;
using System.Text;
using System.Text.Json.Serialization;
using EpicGames.Core;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Tokenizer;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Types
{
	/// <summary>
	/// FStructProperty
	/// </summary>
	[UnrealHeaderTool]
	[UhtEngineClass(Name = "StructProperty", IsProperty = true)]
	public class UhtStructProperty : UhtProperty
	{
		/// <inheritdoc/>
		public override string EngineClassName => "StructProperty";

		/// <inheritdoc/>
		protected override string CppTypeText => "invalid";

		/// <inheritdoc/>
		protected override string PGetMacroText => "STRUCT";

		/// <inheritdoc/>
		protected override UhtPGetArgumentType PGetTypeArgument => UhtPGetArgumentType.TypeText;

		/// <summary>
		/// USTRUCT referenced by the property
		/// </summary>
		[JsonConverter(typeof(UhtTypeSourceNameJsonConverter<UhtScriptStruct>))]
		public UhtScriptStruct ScriptStruct { get; set; }

		/// <summary>
		/// Construct property
		/// </summary>
		/// <param name="propertySettings">Property settings</param>
		/// <param name="scriptStruct">USTRUCT being referenced</param>
		public UhtStructProperty(UhtPropertySettings propertySettings, UhtScriptStruct scriptStruct) : base(propertySettings)
		{
			ScriptStruct = scriptStruct;
			HeaderFile.AddReferencedHeader(scriptStruct);
			PropertyCaps |= UhtPropertyCaps.SupportsRigVM;

			UpdateCaps();
		}

		/// <inheritdoc/>
		protected override bool ResolveSelf(UhtResolvePhase phase)
		{
			bool results = base.ResolveSelf(phase);
			switch (phase)
			{
				case UhtResolvePhase.Final:
					if (ScanForInstancedReferenced(true))
					{
						PropertyFlags |= EPropertyFlags.ContainsInstancedReference;
					}
					UpdateCaps();
					break;
			}
			return results;
		}

		private void UpdateCaps()
		{
			if (ScriptStruct.HasNoOpConstructor)
			{
				PropertyCaps |= UhtPropertyCaps.RequiresNullConstructorArg;
			}

			// There is a good chance Blueprint type was set during property resolve phase.  Check this flag again.
			const UhtPropertyCaps BlueprintCaps = UhtPropertyCaps.CanExposeOnSpawn | UhtPropertyCaps.IsParameterSupportedByBlueprint | UhtPropertyCaps.IsMemberSupportedByBlueprint;
			if (!PropertyCaps.HasExactFlags(BlueprintCaps, BlueprintCaps))
			{
				if (ScriptStruct.MetaData.GetBoolean(UhtNames.BlueprintType))
				{
					PropertyCaps |= BlueprintCaps;
				}
				else if (ScriptStruct.MetaData.GetBooleanHierarchical(UhtNames.BlueprintType))
				{
					PropertyCaps |= UhtPropertyCaps.IsParameterSupportedByBlueprint | UhtPropertyCaps.IsMemberSupportedByBlueprint;
				}
			}
		}

		/// <inheritdoc/>
		public override bool ScanForInstancedReferenced(bool deepScan)
		{
			return ScriptStruct.ScanForInstancedReferenced(deepScan);
		}

		/// <inheritdoc/>
		public override void CollectReferencesInternal(IUhtReferenceCollector collector, bool templateProperty)
		{
			collector.AddCrossModuleReference(ScriptStruct, true);
		}

		/// <inheritdoc/>
		public override string? GetForwardDeclarations()
		{
			return !ScriptStruct.IsCoreType ? $"struct {ScriptStruct.SourceName};" : null;
		}

		/// <inheritdoc/>
		public override IEnumerable<UhtType> EnumerateReferencedTypes()
		{
			yield return ScriptStruct;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendText(StringBuilder builder, UhtPropertyTextType textType, bool isTemplateArgument)
		{
			switch (textType)
			{
				default:
					builder.Append(ScriptStruct.SourceName);
					break;
			}
			return builder;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDecl(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, int tabs)
		{
			return AppendMemberDecl(builder, context, name, nameSuffix, tabs, "FStructPropertyParams");
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDef(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, string? offset, int tabs)
		{
			AppendMemberDefStart(builder, context, name, nameSuffix, offset, tabs, "FStructPropertyParams", "UECodeGen_Private::EPropertyGenFlags::Struct");
			AppendMemberDefRef(builder, context, ScriptStruct, true);
			AppendMemberDefEnd(builder, context, name, nameSuffix);
			return builder;
		}

		/// <inheritdoc/>
		public override void AppendObjectHashes(StringBuilder builder, int startingLength, IUhtPropertyMemberContext context)
		{
			builder.AppendObjectHash(startingLength, this, context, ScriptStruct);
		}

		/// <inheritdoc/>
		public override StringBuilder AppendNullConstructorArg(StringBuilder builder, bool isInitializer)
		{
			bool hasNoOpConstructor = ScriptStruct.HasNoOpConstructor;
			if (isInitializer && hasNoOpConstructor)
			{
				builder.Append("ForceInit");
			}
			else
			{
				builder.AppendPropertyText(this, UhtPropertyTextType.Construction);
				if (hasNoOpConstructor)
				{
					builder.Append("(ForceInit)");
				}
				else
				{
					builder.Append("()");
				}
			}
			return builder;
		}

		/// <inheritdoc/>
		public override bool SanitizeDefaultValue(IUhtTokenReader defaultValueReader, StringBuilder innerDefaultValue)
		{
			if (!Session.TryGetStructDefaultValue(ScriptStruct.SourceName, out UhtStructDefaultValue structDefaultValue))
			{
				structDefaultValue = Session.DefaultStructDefaultValue;
			}
			return structDefaultValue.Delegate(this, defaultValueReader, innerDefaultValue);
		}

		/// <inheritdoc/>
		public override bool IsSameType(UhtProperty other)
		{
			if (other is UhtStructProperty otherObject)
			{
				return ScriptStruct == otherObject.ScriptStruct;
			}
			return false;
		}

		/// <inheritdoc/>
		protected override void ValidateFunctionArgument(UhtFunction function, UhtValidationOptions options)
		{
			base.ValidateFunctionArgument(function, options);

			if (function.FunctionFlags.HasAnyFlags(EFunctionFlags.Net))
			{
				if (!function.FunctionFlags.HasAnyFlags(EFunctionFlags.NetRequest | EFunctionFlags.NetResponse))
				{
					Session.ValidateScriptStructOkForNet(this, ScriptStruct);
				}
			}
		}

		/// <inheritdoc/>
		protected override void ValidateMember(UhtStruct structObj, UhtValidationOptions options)
		{
			base.ValidateMember(structObj, options);
			if (PropertyFlags.HasAnyFlags(EPropertyFlags.Net))
			{
				Session.ValidateScriptStructOkForNet(this, ScriptStruct);
			}
		}

		///<inheritdoc/>
		public override bool ValidateStructPropertyOkForNet(UhtProperty referencingProperty)
		{
			return referencingProperty.Session.ValidateScriptStructOkForNet(referencingProperty, ScriptStruct);
		}

		///<inheritdoc/>
		public override bool ContainsEditorOnlyProperties()
		{
			foreach (UhtType child in ScriptStruct.Children)
			{
				if (child is UhtProperty property)
				{
					if (property.PropertyFlags.HasAnyFlags(EPropertyFlags.EditorOnly) || property.ContainsEditorOnlyProperties())
					{
						return true;
					}
				}
			}
			return false;
		}

		#region Structure default value sanitizers
		[UhtStructDefaultValue(Name = "FVector")]
		[SuppressMessage("CodeQuality", "IDE0051:Remove unused private members", Justification = "Attribute accessed method")]
		[SuppressMessage("Style", "IDE0060:Remove unused parameter", Justification = "Attribute accessed method")]
		private static bool VectorStructDefaultValue(UhtStructProperty property, IUhtTokenReader defaultValueReader, StringBuilder innerDefaultValue)
		{
			const string Format = "{0:F6},{1:F6},{2:F6}";

			defaultValueReader.Require("FVector");
			if (defaultValueReader.TryOptional("::"))
			{
				switch (defaultValueReader.GetIdentifier().Value.ToString())
				{
					case "ZeroVector": return true;
					case "UpVector": innerDefaultValue.AppendFormat(CultureInfo.InvariantCulture, Format, 0, 0, 1); return true;
					case "ForwardVector": innerDefaultValue.AppendFormat(CultureInfo.InvariantCulture, Format, 1, 0, 0); return true;
					case "RightVector": innerDefaultValue.AppendFormat(CultureInfo.InvariantCulture, Format, 0, 1, 0); return true;
					default: return false;
				}
			}
			else
			{
				defaultValueReader.Require("(");
				if (defaultValueReader.TryOptional("ForceInit"))
				{
				}
				else
				{
					double x, y, z;
					x = y = z = defaultValueReader.GetConstDoubleExpression();
					if (defaultValueReader.TryOptional(','))
					{
						y = defaultValueReader.GetConstDoubleExpression();
						defaultValueReader.Require(',');
						z = defaultValueReader.GetConstDoubleExpression();
					}
					innerDefaultValue.AppendFormat(CultureInfo.InvariantCulture, Format, x, y, z);
				}
				defaultValueReader.Require(")");
				return true;
			}
		}

		[UhtStructDefaultValue(Name = "FRotator")]
		[SuppressMessage("CodeQuality", "IDE0051:Remove unused private members", Justification = "Attribute accessed method")]
		[SuppressMessage("Style", "IDE0060:Remove unused parameter", Justification = "Attribute accessed method")]
		private static bool RotatorStructDefaultValue(UhtStructProperty property, IUhtTokenReader defaultValueReader, StringBuilder innerDefaultValue)
		{
			defaultValueReader.Require("FRotator");
			if (defaultValueReader.TryOptional("::"))
			{
				switch (defaultValueReader.GetIdentifier().Value.ToString())
				{
					case "ZeroRotator": return true;
					default: return false;
				}
			}
			else
			{
				defaultValueReader.Require("(");
				if (defaultValueReader.TryOptional("ForceInit"))
				{
				}
				else
				{
					double x = defaultValueReader.GetConstDoubleExpression();
					defaultValueReader.Require(',');
					double y = defaultValueReader.GetConstDoubleExpression();
					defaultValueReader.Require(',');
					double z = defaultValueReader.GetConstDoubleExpression();
					innerDefaultValue.AppendFormat(CultureInfo.InvariantCulture, "{0:F6},{1:F6},{2:F6}", x, y, z);
				}
				defaultValueReader.Require(")");
				return true;
			}
		}

		[UhtStructDefaultValue(Name = "FVector2D")]
		[SuppressMessage("CodeQuality", "IDE0051:Remove unused private members", Justification = "Attribute accessed method")]
		[SuppressMessage("Style", "IDE0060:Remove unused parameter", Justification = "Attribute accessed method")]
		private static bool Vector2DStructDefaultValue(UhtStructProperty property, IUhtTokenReader defaultValueReader, StringBuilder innerDefaultValue)
		{
			const string Format = "(X={0:F3},Y={1:F3})";

			defaultValueReader.Require("FVector2D");
			if (defaultValueReader.TryOptional("::"))
			{
				switch (defaultValueReader.GetIdentifier().Value.ToString())
				{
					case "ZeroVector": return true;
					case "UnitVector": innerDefaultValue.AppendFormat(CultureInfo.InvariantCulture, Format, 1.0, 1.0); return true;
					default: return false;
				}
			}
			else
			{
				defaultValueReader.Require("(");
				if (defaultValueReader.TryOptional("ForceInit"))
				{
				}
				else
				{
					double x = defaultValueReader.GetConstDoubleExpression();
					defaultValueReader.Require(',');
					double y = defaultValueReader.GetConstDoubleExpression();
					innerDefaultValue.AppendFormat(CultureInfo.InvariantCulture, Format, x, y);
				}
				defaultValueReader.Require(")");
				return true;
			}
		}

		[UhtStructDefaultValue(Name = "FLinearColor")]
		[SuppressMessage("CodeQuality", "IDE0051:Remove unused private members", Justification = "Attribute accessed method")]
		[SuppressMessage("Style", "IDE0060:Remove unused parameter", Justification = "Attribute accessed method")]
		private static bool LinearColorStructDefaultValue(UhtStructProperty property, IUhtTokenReader defaultValueReader, StringBuilder innerDefaultValue)
		{
			const string Format = "(R={0:F6},G={1:F6},B={2:F6},A={3:F6})";

			defaultValueReader.Require("FLinearColor");
			if (defaultValueReader.TryOptional("::"))
			{
				switch (defaultValueReader.GetIdentifier().Value.ToString())
				{
					case "White": innerDefaultValue.AppendFormat(CultureInfo.InvariantCulture, Format, 1.0, 1.0, 1.0, 1.0); return true;
					case "Gray": innerDefaultValue.AppendFormat(CultureInfo.InvariantCulture, Format, 0.5, 0.5, 0.5, 1.0); return true;
					case "Black": innerDefaultValue.AppendFormat(CultureInfo.InvariantCulture, Format, 0.0, 0.0, 0.0, 1.0); return true;
					case "Transparent": innerDefaultValue.AppendFormat(CultureInfo.InvariantCulture, Format, 0.0, 0.0, 0.0, 0.0); return true;
					case "Red": innerDefaultValue.AppendFormat(CultureInfo.InvariantCulture, Format, 1.0, 0.0, 0.0, 1.0); return true;
					case "Green": innerDefaultValue.AppendFormat(CultureInfo.InvariantCulture, Format, 0.0, 1.0, 0.0, 1.0); return true;
					case "Blue": innerDefaultValue.AppendFormat(CultureInfo.InvariantCulture, Format, 0.0, 0.0, 1.0, 1.0); return true;
					case "Yellow": innerDefaultValue.AppendFormat(CultureInfo.InvariantCulture, Format, 1.0, 1.0, 0.0, 1.0); return true;
					default: return false;
				}
			}
			else
			{
				defaultValueReader.Require("(");
				if (defaultValueReader.TryOptional("ForceInit"))
				{
				}
				else
				{
					double r = defaultValueReader.GetConstDoubleExpression();
					defaultValueReader.Require(',');
					double g = defaultValueReader.GetConstDoubleExpression();
					defaultValueReader.Require(',');
					double b = defaultValueReader.GetConstDoubleExpression();
					double a = 1.0;
					if (defaultValueReader.TryOptional(','))
					{
						a = defaultValueReader.GetConstDoubleExpression();
					}
					innerDefaultValue.AppendFormat(CultureInfo.InvariantCulture, Format, r, g, b, a);
				}
				defaultValueReader.Require(")");
				return true;
			}
		}

		[UhtStructDefaultValue(Name = "FColor")]
		[SuppressMessage("CodeQuality", "IDE0051:Remove unused private members", Justification = "Attribute accessed method")]
		[SuppressMessage("Style", "IDE0060:Remove unused parameter", Justification = "Attribute accessed method")]
		private static bool ColorStructDefaultValue(UhtStructProperty property, IUhtTokenReader defaultValueReader, StringBuilder innerDefaultValue)
		{
			const string Format = "(R={0},G={1},B={2},A={3})";

			defaultValueReader.Require("FColor");
			if (defaultValueReader.TryOptional("::"))
			{
				switch (defaultValueReader.GetIdentifier().Value.ToString())
				{
					case "White": innerDefaultValue.AppendFormat(CultureInfo.InvariantCulture, Format, 255, 255, 255, 255); return true;
					case "Black": innerDefaultValue.AppendFormat(CultureInfo.InvariantCulture, Format, 0, 0, 0, 255); return true;
					case "Red": innerDefaultValue.AppendFormat(CultureInfo.InvariantCulture, Format, 255, 0, 0, 255); return true;
					case "Green": innerDefaultValue.AppendFormat(CultureInfo.InvariantCulture, Format, 0, 255, 0, 255); return true;
					case "Blue": innerDefaultValue.AppendFormat(CultureInfo.InvariantCulture, Format, 0, 0, 255, 255); return true;
					case "Yellow": innerDefaultValue.AppendFormat(CultureInfo.InvariantCulture, Format, 255, 255, 0, 255); return true;
					case "Cyan": innerDefaultValue.AppendFormat(CultureInfo.InvariantCulture, Format, 0, 255, 255, 255); return true;
					case "Magenta": innerDefaultValue.AppendFormat(CultureInfo.InvariantCulture, Format, 255, 0, 255, 255); return true;
					default: return false;
				}
			}
			else
			{
				defaultValueReader.Require("(");
				if (defaultValueReader.TryOptional("ForceInit"))
				{
				}
				else
				{
					int r = defaultValueReader.GetConstIntExpression();
					defaultValueReader.Require(',');
					int g = defaultValueReader.GetConstIntExpression();
					defaultValueReader.Require(',');
					int b = defaultValueReader.GetConstIntExpression();
					int a = 255;
					if (defaultValueReader.TryOptional(','))
					{
						a = defaultValueReader.GetConstIntExpression();
					}
					innerDefaultValue.AppendFormat(CultureInfo.InvariantCulture, Format, r, g, b, a);
				}
				defaultValueReader.Require(")");
				return true;
			}
		}

		[UhtStructDefaultValue(Options = UhtStructDefaultValueOptions.Default)]
		[SuppressMessage("CodeQuality", "IDE0051:Remove unused private members", Justification = "Attribute accessed method")]
		private static bool DefaultStructDefaultValue(UhtStructProperty property, IUhtTokenReader defaultValueReader, StringBuilder innerDefaultValue)
		{
			defaultValueReader
				.Require(property.ScriptStruct.SourceName)
				.Require('(')
				.Require(')');
			innerDefaultValue.Append("()");
			return true;
		}
		#endregion
	}
}
