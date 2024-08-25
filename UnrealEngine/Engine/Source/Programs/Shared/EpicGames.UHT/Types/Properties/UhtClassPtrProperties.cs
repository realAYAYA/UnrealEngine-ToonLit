// Copyright Epic Games, Inc. All Rights Reserved.

using System.Text;
using EpicGames.Core;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Types
{

	/// <summary>
	/// FClassPtrProperty
	/// </summary>
	[UhtEngineClass(Name = "ClassPtrProperty", IsProperty = true)]
	public class UhtClassPtrProperty : UhtClassProperty
	{
		/// <inheritdoc/>
		public override string EngineClassName => "ClassProperty";

		/// <inheritdoc/>
		protected override string CppTypeText => "ObjectPtr";

		/// <inheritdoc/>
		protected override string PGetMacroText => "OBJECTPTR";

		/// <inheritdoc/>
		protected override UhtPGetArgumentType PGetTypeArgument => UhtPGetArgumentType.TypeText;

		/// <summary>
		/// Construct a new property
		/// </summary>
		/// <param name="propertySettings"></param>
		/// <param name="classObj">Referenced class</param>
		/// <param name="metaClass">Meta data class</param>
		/// <param name="extraFlags">Extra property flags to apply to the property</param>
		public UhtClassPtrProperty(UhtPropertySettings propertySettings, UhtClass classObj, UhtClass metaClass, EPropertyFlags extraFlags = EPropertyFlags.None)
			: base(propertySettings, classObj, metaClass)
		{
			PropertyFlags |= extraFlags | EPropertyFlags.TObjectPtr | EPropertyFlags.UObjectWrapper;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendText(StringBuilder builder, UhtPropertyTextType textType, bool isTemplateArgument)
		{
			switch (textType)
			{
				default:
					builder.Append("TObjectPtr<").Append(Class.SourceName).Append('>');
					break;
			}
			return builder;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDecl(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, int tabs)
		{
			return AppendMemberDecl(builder, context, name, nameSuffix, tabs, "FClassPropertyParams");
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDef(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, string? offset, int tabs)
		{
			AppendMemberDefStart(builder, context, name, nameSuffix, offset, tabs, "FClassPropertyParams",
				"UECodeGen_Private::EPropertyGenFlags::Class | UECodeGen_Private::EPropertyGenFlags::ObjectPtr");
			AppendMemberDefRef(builder, context, Class, false);
			AppendMemberDefRef(builder, context, MetaClass, false);
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
				outerStruct.LogError("UFunctions cannot take a TObjectPtr as a function parameter or return value.");
			}
		}
	}
}
