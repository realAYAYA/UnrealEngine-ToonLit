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
	/// FLazyObjectProperty
	/// </summary>
	[UnrealHeaderTool]
	[UhtEngineClass(Name = "LazyObjectProperty", IsProperty = true)]
	public class UhtLazyObjectPtrProperty : UhtObjectPropertyBase
	{
		/// <inheritdoc/>
		public override string EngineClassName => "LazyObjectProperty";

		/// <inheritdoc/>
		protected override string CppTypeText => "TLazyObjectPtr";

		/// <inheritdoc/>
		protected override string PGetMacroText => "LAZYOBJECT";

		/// <inheritdoc/>
		protected override bool PGetPassAsNoPtr => true;

		/// <inheritdoc/>
		protected override UhtPGetArgumentType PGetTypeArgument => UhtPGetArgumentType.TypeText;

		/// <summary>
		/// Construct new property
		/// </summary>
		/// <param name="propertySettings">Property settings</param>
		/// <param name="classObj">Referenced class</param>
		public UhtLazyObjectPtrProperty(UhtPropertySettings propertySettings, UhtClass classObj)
			: base(propertySettings, classObj, null)
		{
			PropertyFlags |= EPropertyFlags.UObjectWrapper;
			PropertyCaps |= UhtPropertyCaps.PassCppArgsByRef | UhtPropertyCaps.RequiresNullConstructorArg;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendText(StringBuilder builder, UhtPropertyTextType textType, bool isTemplateArgument)
		{
			switch (textType)
			{
				default:
					builder.Append("TLazyObjectPtr<").Append(Class.SourceName).Append('>');
					break;
			}
			return builder;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDecl(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, int tabs)
		{
			return AppendMemberDecl(builder, context, name, nameSuffix, tabs, "FLazyObjectPropertyParams");
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDef(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, string? offset, int tabs)
		{
			AppendMemberDefStart(builder, context, name, nameSuffix, offset, tabs, "FLazyObjectPropertyParams", "UECodeGen_Private::EPropertyGenFlags::LazyObject");
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
			if (other is UhtLazyObjectPtrProperty otherObject)
			{
				return Class == otherObject.Class && MetaClass == otherObject.MetaClass;
			}
			return false;
		}

		/// <inheritdoc/>
		public override void Validate(UhtStruct outerStruct, UhtProperty outermostProperty, UhtValidationOptions options)
		{
			base.Validate(outerStruct, outermostProperty, options);

			// UFunctions with a smart pointer as input parameter wont compile anyway, because of missing P_GET_... macro.
			// UFunctions with a smart pointer as return type will crash when called via blueprint, because they are not supported in VM.
			if (PropertyCategory != UhtPropertyCategory.Member)
			{
				outerStruct.LogError("UFunctions cannot take a lazy pointer as a parameter.");
			}
		}

		#region Keyword
		[UhtPropertyType(Keyword = "TLazyObjectPtr")]
		[SuppressMessage("CodeQuality", "IDE0051:Remove unused private members", Justification = "Attribute accessed method")]
		[SuppressMessage("Style", "IDE0060:Remove unused parameter", Justification = "Attribute accessed method")]
		private static UhtProperty? LazyObjectPtrProperty(UhtPropertyResolvePhase resolvePhase, UhtPropertySettings propertySettings, IUhtTokenReader tokenReader, UhtToken matchedToken)
		{
			UhtClass? propertyClass = ParseTemplateObject(propertySettings, tokenReader, matchedToken, true);
			if (propertyClass == null)
			{
				return null;
			}

			if (propertyClass.IsChildOf(propertyClass.Session.UClass))
			{
				tokenReader.LogError("Class variables cannot be lazy, they are always strong.");
			}

			return new UhtLazyObjectPtrProperty(propertySettings, propertyClass);
		}
		#endregion
	}
}
