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
	/// FSoftObjectProperty
	/// </summary>
	[UnrealHeaderTool]
	[UhtEngineClass(Name = "SoftObjectProperty", IsProperty = true)]
	public class UhtSoftObjectProperty : UhtObjectPropertyBase
	{
		/// <inheritdoc/>
		public override string EngineClassName => "SoftObjectProperty";

		/// <inheritdoc/>
		protected override string CppTypeText => "SoftObjectPtr";

		/// <inheritdoc/>
		protected override string PGetMacroText => "SOFTOBJECT";

		/// <inheritdoc/>
		protected override UhtPGetArgumentType PGetTypeArgument => UhtPGetArgumentType.TypeText;

		/// <summary>
		/// Construct a new property
		/// </summary>
		/// <param name="propertySettings">Property settings</param>
		/// <param name="classObj">UCLASS being referenced</param>
		/// <param name="metaClass">Optional meta class (used by SoftClassProperty)</param>
		public UhtSoftObjectProperty(UhtPropertySettings propertySettings, UhtClass classObj, UhtClass? metaClass = null)
			: base(propertySettings, classObj, metaClass)
		{
			PropertyFlags |= EPropertyFlags.UObjectWrapper;
			PropertyCaps |= UhtPropertyCaps.PassCppArgsByRef | UhtPropertyCaps.RequiresNullConstructorArg | UhtPropertyCaps.CanExposeOnSpawn |
				UhtPropertyCaps.CanHaveConfig | UhtPropertyCaps.IsParameterSupportedByBlueprint | UhtPropertyCaps.IsMemberSupportedByBlueprint;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendNullConstructorArg(StringBuilder builder, bool isInitializer)
		{
			builder.Append("NULL");
			return builder;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendText(StringBuilder builder, UhtPropertyTextType textType, bool isTemplateArgument)
		{
			switch (textType)
			{
				default:
					builder.Append("TSoftObjectPtr<").Append(Class.SourceName).Append('>');
					break;
			}
			return builder;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDecl(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, int tabs)
		{
			return AppendMemberDecl(builder, context, name, nameSuffix, tabs, "FSoftObjectPropertyParams");
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDef(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, string? offset, int tabs)
		{
			AppendMemberDefStart(builder, context, name, nameSuffix, offset, tabs, "FSoftObjectPropertyParams", "UECodeGen_Private::EPropertyGenFlags::SoftObject");
			AppendMemberDefRef(builder, context, Class, false);
			AppendMemberDefEnd(builder, context, name, nameSuffix);
			return builder;
		}

		/// <inheritdoc/>
		public override bool IsSameType(UhtProperty other)
		{
			if (other is UhtSoftObjectProperty otherObject)
			{
				return Class == otherObject.Class && MetaClass == otherObject.MetaClass;
			}
			return false;
		}

		#region Keyword
		[UhtPropertyType(Keyword = "TSoftObjectPtr")]
		[SuppressMessage("CodeQuality", "IDE0051:Remove unused private members", Justification = "Attribute accessed method")]
		[SuppressMessage("Style", "IDE0060:Remove unused parameter", Justification = "Attribute accessed method")]
		private static UhtProperty? SoftObjectPtrProperty(UhtPropertyResolvePhase resolvePhase, UhtPropertySettings propertySettings, IUhtTokenReader tokenReader, UhtToken matchedToken)
		{
			UhtClass? propertyClass = UhtObjectPropertyBase.ParseTemplateObject(propertySettings, tokenReader, matchedToken, true);
			if (propertyClass == null)
			{
				return null;
			}

			if (propertyClass.IsChildOf(propertyClass.Session.UClass))
			{
				tokenReader.LogError("Class variables cannot be stored in TSoftObjectPtr, use TSoftClassPtr instead.");
			}

			return new UhtSoftObjectProperty(propertySettings, propertyClass);
		}
		#endregion
	}
}
