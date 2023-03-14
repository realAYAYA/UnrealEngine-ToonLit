// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics.CodeAnalysis;
using System.Text;
using EpicGames.Core;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Tokenizer;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Types
{
	/// <summary>
	/// FFieldPathProperty
	/// </summary>
	[UnrealHeaderTool]
	[UhtEngineClass(Name = "FieldPathProperty", IsProperty = true)]
	public class UhtFieldPathProperty : UhtProperty
	{
		/// <inheritdoc/>
		public override string EngineClassName => "FieldPathProperty";

		/// <inheritdoc/>
		protected override string CppTypeText => "TFieldPath";

		/// <inheritdoc/>
		protected override string PGetMacroText => "TFIELDPATH";

		/// <inheritdoc/>
		protected override UhtPGetArgumentType PGetTypeArgument => UhtPGetArgumentType.TypeText;

		/// <summary>
		/// Class name without the prefix
		/// </summary>
		public string FieldClassName { get; set; }

		/// <summary>
		/// Construct new property
		/// </summary>
		/// <param name="propertySettings">Property settings</param>
		/// <param name="fieldClassName">Field class name</param>
		public UhtFieldPathProperty(UhtPropertySettings propertySettings, string fieldClassName) : base(propertySettings)
		{
			FieldClassName = fieldClassName;
			PropertyCaps |= UhtPropertyCaps.IsParameterSupportedByBlueprint | UhtPropertyCaps.IsMemberSupportedByBlueprint;
		}

		/// <inheritdoc/>
		public override string? GetForwardDeclarations()
		{
			return $"class F{FieldClassName};";
		}

		/// <inheritdoc/>
		public override StringBuilder AppendText(StringBuilder builder, UhtPropertyTextType textType, bool isTemplateArgument)
		{
			switch (textType)
			{
				case UhtPropertyTextType.SparseShort:
					builder.Append("TFieldPath");
					break;

				default:
					builder.Append("TFieldPath<F").Append(FieldClassName).Append('>');
					break;
			}
			return builder;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDecl(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, int tabs)
		{
			return AppendMemberDecl(builder, context, name, nameSuffix, tabs, "FFieldPathPropertyParams");
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDef(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, string? offset, int tabs)
		{
			AppendMemberDefStart(builder, context, name, nameSuffix, offset, tabs, "FFieldPathPropertyParams", "UECodeGen_Private::EPropertyGenFlags::FieldPath");
			builder.Append("&F").Append(FieldClassName).Append("::StaticClass, ");
			AppendMemberDefEnd(builder, context, name, nameSuffix);
			return builder;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendNullConstructorArg(StringBuilder builder, bool isInitializer)
		{
			builder.Append("nullptr");
			return builder;
		}

		/// <inheritdoc/>
		public override bool SanitizeDefaultValue(IUhtTokenReader defaultValueReader, StringBuilder innerDefaultValue)
		{
			return false;
		}

		/// <inheritdoc/>
		public override bool IsSameType(UhtProperty other)
		{
			if (other is UhtFieldPathProperty otherFieldPath)
			{
				return FieldClassName == otherFieldPath.FieldClassName;
			}
			return false;
		}

		#region Keyword
		[UhtPropertyType(Keyword = "TFieldPath", Options = UhtPropertyTypeOptions.Immediate)]
		[SuppressMessage("CodeQuality", "IDE0051:Remove unused private members", Justification = "Attribute accessed method")]
		[SuppressMessage("Style", "IDE0060:Remove unused parameter", Justification = "Attribute accessed method")]
		private static UhtProperty? FieldPathProperty(UhtPropertyResolvePhase resolvePhase, UhtPropertySettings propertySettings, IUhtTokenReader tokenReader, UhtToken matchedToken)
		{
			using UhtMessageContext tokenContext = new("TFieldPath");
			UhtToken identifier = new();
			tokenReader
				.Require("TFieldPath")
				.Require('<')
				.RequireIdentifier((ref UhtToken token) => identifier = token)
				.Require('>');

			StringView fieldClassName = new(identifier.Value, 1);
			if (!propertySettings.Outer.Session.IsValidPropertyTypeName(fieldClassName))
			{
				throw new UhtException($"Undefined property type: {identifier.Value}");
			}
			return new UhtFieldPathProperty(propertySettings, fieldClassName.ToString());
		}
		#endregion
	}
}
