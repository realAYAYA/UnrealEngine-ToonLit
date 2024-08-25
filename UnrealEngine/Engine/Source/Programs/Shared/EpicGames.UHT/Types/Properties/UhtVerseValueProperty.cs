// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics.CodeAnalysis;
using System.Text;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Tokenizer;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Types
{

	/// <summary>
	/// FStrProperty
	/// </summary>
	[UnrealHeaderTool]
	[UhtEngineClass(Name = "VerseValueProperty", IsProperty = true)]
	public class UhtVerseValueProperty : UhtProperty
	{
		/// <inheritdoc/>
		public override string EngineClassName => "VValueProperty";

		/// <inheritdoc/>
		protected override string CppTypeText => "Verse::TWriteBarrier<Verse::VValue>";

		/// <inheritdoc/>
		protected override string PGetMacroText => "PROPERTY";

		/// <inheritdoc/>
		protected override UhtPGetArgumentType PGetTypeArgument => UhtPGetArgumentType.EngineClass;

		/// <summary>
		/// Construct a new property
		/// </summary>
		/// <param name="propertySettings">Property settings</param>
		public UhtVerseValueProperty(UhtPropertySettings propertySettings) : base(propertySettings)
		{
		}

		/// <inheritdoc/>
		public override StringBuilder AppendNullConstructorArg(StringBuilder builder, bool isInitializer)
		{
			builder.Append("TEXT(\"\")");
			return builder;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDecl(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, int tabs)
		{
			return AppendMemberDecl(builder, context, name, nameSuffix, tabs, "FVerseValuePropertyParams");
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDef(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, string? offset, int tabs)
		{
			AppendMemberDefStart(builder, context, name, nameSuffix, offset, tabs, "FVerseValuePropertyParams", "UECodeGen_Private::EPropertyGenFlags::VValue");
			AppendMemberDefEnd(builder, context, name, nameSuffix);
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
			return other is UhtVerseValueProperty;
		}

		/// <inheritdoc/>
		public override void Validate(UhtStruct outerStruct, UhtProperty outermostProperty, UhtValidationOptions options)
		{
			base.Validate(outerStruct, outermostProperty, options);
			if (!DefineScope.HasAnyFlags(UhtDefineScope.VerseVM))
			{
				this.LogError("Verse property types must be wrapped in a \"#if WITH_VERSE_VM\" block");
			}
		}

		[UhtPropertyType(Keyword = "Verse")]
		[UhtPropertyType(Keyword = "TWriteBarrier", Options = UhtPropertyTypeOptions.Immediate)]
		[SuppressMessage("CodeQuality", "IDE0051:Remove unused private members", Justification = "Attribute accessed method")]
		[SuppressMessage("Style", "IDE0060:Remove unused parameter", Justification = "Attribute accessed method")]
		private static UhtProperty? VerseProperty(UhtPropertyResolvePhase resolvePhase, UhtPropertySettings propertySettings, IUhtTokenReader tokenReader, UhtToken matchedToken)
		{
			if (propertySettings.PropertyCategory != UhtPropertyCategory.Member)
			{
				tokenReader.LogError("Verse value properties can not be used as function parameters or returns");
			}

			tokenReader
				.OptionalNamespace("Verse")
				.Require("TWriteBarrier")
				.Require("<")
				.OptionalNamespace("Verse")
				.Require("VValue")
				.Require(">");

			UhtVerseValueProperty property = new(propertySettings);
			return property;
		}
	}
}
