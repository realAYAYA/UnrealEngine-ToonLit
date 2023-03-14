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
	/// FObjectPtrProperty
	/// </summary>
	[UnrealHeaderTool]
	[UhtEngineClass(Name = "ObjectPtrProperty", IsProperty = true)]
	public class UhtObjectPtrProperty : UhtObjectProperty
	{
		/// <inheritdoc/>
		public override string EngineClassName => "ObjectPtrProperty";

		/// <inheritdoc/>
		protected override string CppTypeText => "ObjectPtr";

		/// <inheritdoc/>
		protected override string PGetMacroText => "OBJECTPTR";

		/// <inheritdoc/>
		protected override UhtPGetArgumentType PGetTypeArgument => UhtPGetArgumentType.TypeText;

		/// <summary>
		/// Construct a new property
		/// </summary>
		/// <param name="propertySettings">Property settings</param>
		/// <param name="classObj">Referenced class</param>
		/// <param name="extraFlags">Extra property flags to apply</param>
		public UhtObjectPtrProperty(UhtPropertySettings propertySettings, UhtClass classObj, EPropertyFlags extraFlags = EPropertyFlags.None)
			: base(propertySettings, classObj)
		{
			PropertyFlags |= extraFlags | EPropertyFlags.UObjectWrapper;
			PropertyCaps |= UhtPropertyCaps.PassCppArgsByRef;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendText(StringBuilder builder, UhtPropertyTextType textType, bool isTemplateArgument)
		{
			switch (textType)
			{
				case UhtPropertyTextType.GetterSetterArg:
					if (isTemplateArgument)
					{
						builder.Append("TObjectPtr<").Append(Class.SourceName).Append('>');
					}
					else
					{
						builder.Append(Class.SourceName).Append('*');
					}
					break;

				case UhtPropertyTextType.FunctionThunkRetVal:
					if (PropertyFlags.HasAnyFlags(EPropertyFlags.ConstParm))
					{
						builder.Append("const ");
					}
					builder.Append("TObjectPtr<").Append(Class.SourceName).Append('>');
					break;

				default:
					builder.Append("TObjectPtr<").Append(Class.SourceName).Append('>');
					break;
			}
			return builder;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDecl(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, int tabs)
		{
			return AppendMemberDecl(builder, context, name, nameSuffix, tabs, "FObjectPtrPropertyParams");
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDef(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, string? offset, int tabs)
		{
			AppendMemberDefStart(builder, context, name, nameSuffix, offset, tabs, "FObjectPtrPropertyParams", "UECodeGen_Private::EPropertyGenFlags::Object | UECodeGen_Private::EPropertyGenFlags::ObjectPtr");
			AppendMemberDefRef(builder, context, Class, false);
			AppendMemberDefEnd(builder, context, name, nameSuffix);
			return builder;
		}

		/// <inheritdoc/>
		public override void Validate(UhtStruct outerStruct, UhtProperty outermostProperty, UhtValidationOptions options)
		{
			base.Validate(outerStruct, outermostProperty, options);

			// UFunctions with a smart pointer as input parameter wont compile anyway, because of missing P_GET_... macro.
			// UFunctions with a smart pointer as return type will crash when called via blueprint, because they are not supported in VM.
			if (!options.HasAnyFlags(UhtValidationOptions.IsKey) && PropertyCategory != UhtPropertyCategory.Member)
			{
				outerStruct.LogError("UFunctions cannot take a TObjectPtr as a parameter.");
			}
		}

		#region Keyword
		[UhtPropertyType(Keyword = "TObjectPtr")]
		[SuppressMessage("CodeQuality", "IDE0051:Remove unused private members", Justification = "Attribute accessed method")]
		[SuppressMessage("Style", "IDE0060:Remove unused parameter", Justification = "Attribute accessed method")]
		private static UhtProperty? ObjectPtrProperty(UhtPropertyResolvePhase resolvePhase, UhtPropertySettings propertySettings, IUhtTokenReader tokenReader, UhtToken matchedToken)
		{
			UhtSession session = propertySettings.Outer.Session;
			int typeStartPos = tokenReader.PeekToken().InputStartPos;

			UhtClass? propertyClass = ParseTemplateObject(propertySettings, tokenReader, matchedToken, true);
			if (propertyClass == null)
			{
				return null;
			}

			ConditionalLogPointerUsage(propertySettings, session.Config!.EngineObjectPtrMemberBehavior, session.Config!.EnginePluginObjectPtrMemberBehavior,
				session.Config!.NonEngineObjectPtrMemberBehavior, "ObjectPtr", tokenReader, typeStartPos, null);

			if (propertyClass.IsChildOf(propertyClass.Session.UClass))
			{
				// UObject specifies that there is no limiter
				return new UhtClassPtrProperty(propertySettings, propertyClass, propertyClass.Session.UObject);
			}
			else
			{
				return new UhtObjectPtrProperty(propertySettings, propertyClass);
			}
		}
		#endregion
	}
}
