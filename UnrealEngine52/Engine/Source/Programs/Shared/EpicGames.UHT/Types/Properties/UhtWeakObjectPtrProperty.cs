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
	/// Represents a FWeakObjectProperty
	/// </summary>
	[UnrealHeaderTool]
	[UhtEngineClass(Name = "WeakObjectProperty", IsProperty = true)]
	public class UhtWeakObjectPtrProperty : UhtObjectPropertyBase
	{
		/// <inheritdoc/>
		public override string EngineClassName => "WeakObjectProperty";

		/// <inheritdoc/>
		protected override string PGetMacroText => PropertyFlags.HasAnyFlags(EPropertyFlags.AutoWeak) ? "AUTOWEAKOBJECT" : "WEAKOBJECT";

		/// <inheritdoc/>
		protected override bool PGetPassAsNoPtr => true;

		/// <inheritdoc/>
		protected override UhtPGetArgumentType PGetTypeArgument => UhtPGetArgumentType.TypeText;

		/// <summary>
		/// Construct a new property
		/// </summary>
		/// <param name="propertySettings">Property settings</param>
		/// <param name="propertyClass">Class being referenced</param>
		/// <param name="extraFlags">Extra property flags to add to the definition</param>
		public UhtWeakObjectPtrProperty(UhtPropertySettings propertySettings, UhtClass propertyClass, EPropertyFlags extraFlags = EPropertyFlags.None)
			: base(propertySettings, propertyClass, null)
		{
			PropertyFlags |= EPropertyFlags.UObjectWrapper | extraFlags;
			PropertyCaps |= UhtPropertyCaps.PassCppArgsByRef | UhtPropertyCaps.RequiresNullConstructorArg | UhtPropertyCaps.IsMemberSupportedByBlueprint;
			PropertyCaps &= ~(UhtPropertyCaps.IsParameterSupportedByBlueprint);
		}

		/// <inheritdoc/>
		public override StringBuilder AppendText(StringBuilder builder, UhtPropertyTextType textType, bool isTemplateArgument)
		{
			switch (textType)
			{
				default:
					if (PropertyFlags.HasAnyFlags(EPropertyFlags.AutoWeak))
					{
						builder.Append("TAutoWeakObjectPtr<").Append(Class.SourceName).Append('>');
					}
					else
					{
						builder.Append("TWeakObjectPtr<").Append(Class.SourceName).Append('>');
					}
					break;
			}
			return builder;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDecl(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, int tabs)
		{
			return AppendMemberDecl(builder, context, name, nameSuffix, tabs, "FWeakObjectPropertyParams");
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDef(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, string? offset, int tabs)
		{
			AppendMemberDefStart(builder, context, name, nameSuffix, offset, tabs, "FWeakObjectPropertyParams", "UECodeGen_Private::EPropertyGenFlags::WeakObject");
			AppendMemberDefRef(builder, context, Class, false);
			AppendMemberDefEnd(builder, context, name, nameSuffix);
			return builder;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendNullConstructorArg(StringBuilder builder, bool isInitializer)
		{
			builder.Append("NULL");
			return builder;
		}

		/// <inheritdoc/>
		public override bool IsSameType(UhtProperty other)
		{
			if (other is UhtWeakObjectPtrProperty otherObject)
			{
				return Class == otherObject.Class && MetaClass == otherObject.MetaClass;
			}
			return false;
		}

		#region Keywords
		private static UhtProperty CreateWeakProperty(UhtPropertySettings propertySettings, IUhtTokenReader tokenReader, UhtClass classObj, EPropertyFlags extraFlags = EPropertyFlags.None)
		{
			if (classObj.IsChildOf(classObj.Session.UClass))
			{
				tokenReader.LogError("Class variables cannot be weak, they are always strong.");
			}

			if (propertySettings.DisallowPropertyFlags.HasAnyFlags(EPropertyFlags.AutoWeak))
			{
				return new UhtObjectProperty(propertySettings, classObj, null, extraFlags | EPropertyFlags.UObjectWrapper);
			}
			return new UhtWeakObjectPtrProperty(propertySettings, classObj, extraFlags);
		}

		[UhtPropertyType(Keyword = "TWeakObjectPtr")]
		[SuppressMessage("CodeQuality", "IDE0051:Remove unused private members", Justification = "Attribute accessed method")]
		[SuppressMessage("Style", "IDE0060:Remove unused parameter", Justification = "Attribute accessed method")]
		private static UhtProperty? WeakObjectProperty(UhtPropertyResolvePhase resolvePhase, UhtPropertySettings propertySettings, IUhtTokenReader tokenReader, UhtToken matchedToken)
		{
			UhtClass? propertyClass = ParseTemplateObject(propertySettings, tokenReader, matchedToken, true);
			if (propertyClass == null)
			{
				return null;
			}

			return CreateWeakProperty(propertySettings, tokenReader, propertyClass);
		}

		[UhtPropertyType(Keyword = "TAutoWeakObjectPtr")]
		[SuppressMessage("CodeQuality", "IDE0051:Remove unused private members", Justification = "Attribute accessed method")]
		[SuppressMessage("Style", "IDE0060:Remove unused parameter", Justification = "Attribute accessed method")]
		private static UhtProperty? AutoWeakObjectProperty(UhtPropertyResolvePhase resolvePhase, UhtPropertySettings propertySettings, IUhtTokenReader tokenReader, UhtToken matchedToken)
		{
			UhtClass? propertyClass = ParseTemplateObject(propertySettings, tokenReader, matchedToken, true);
			if (propertyClass == null)
			{
				return null;
			}

			return CreateWeakProperty(propertySettings, tokenReader, propertyClass, EPropertyFlags.AutoWeak);
		}
		#endregion
	}
}
