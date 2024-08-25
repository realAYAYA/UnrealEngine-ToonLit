// Copyright Epic Games, Inc. All Rights Reserved.

using System.Text;
using EpicGames.Core;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Types
{
	/// <summary>
	/// FObjectProperty
	/// </summary>
	[UhtEngineClass(Name = "ObjectProperty", IsProperty = true)]
	public class UhtObjectProperty : UhtObjectPropertyBase
	{
		/// <inheritdoc/>
		public override string EngineClassName => "ObjectProperty";

		/// <inheritdoc/>
		protected override UhtPGetArgumentType PGetTypeArgument => UhtPGetArgumentType.TypeText;

		/// <summary>
		/// Construct a new property
		/// </summary>
		/// <param name="propertySettings">Property settings</param>
		/// <param name="classObj">Referenced class</param>
		/// <param name="metaClass">Optional reference class (used by class properties)</param>
		/// <param name="extraFlags">Extra flags to add to the property</param>
		public UhtObjectProperty(UhtPropertySettings propertySettings, UhtClass classObj, UhtClass? metaClass = null, EPropertyFlags extraFlags = EPropertyFlags.None)
			: base(propertySettings, classObj, metaClass)
		{
			PropertyFlags |= extraFlags;
			PropertyCaps |= UhtPropertyCaps.RequiresNullConstructorArg | UhtPropertyCaps.CanBeInstanced | UhtPropertyCaps.CanExposeOnSpawn |
				UhtPropertyCaps.IsParameterSupportedByBlueprint | UhtPropertyCaps.IsMemberSupportedByBlueprint | UhtPropertyCaps.SupportsRigVM;
		}

		/// <inheritdoc/>
		protected override bool ResolveSelf(UhtResolvePhase phase)
		{
			bool results = base.ResolveSelf(phase);
			switch (phase)
			{
				case UhtResolvePhase.Final:
					if (Class.HierarchyHasAnyClassFlags(EClassFlags.DefaultToInstanced))
					{
						PropertyFlags |= EPropertyFlags.InstancedReference;
						MetaData.Add(UhtNames.EditInline, true);
					}
					break;
			}
			return results;
		}

		/// <inheritdoc/>
		public override bool ScanForInstancedReferenced(bool deepScan)
		{
			return Class.HierarchyHasAnyClassFlags(EClassFlags.DefaultToInstanced);
		}

		/// <inheritdoc/>
		public override StringBuilder AppendText(StringBuilder builder, UhtPropertyTextType textType, bool isTemplateArgument)
		{
			switch (textType)
			{
				case UhtPropertyTextType.FunctionThunkRetVal:
					if (PropertyFlags.HasAnyFlags(EPropertyFlags.ConstParm))
					{
						builder.Append("const ");
					}
					builder.Append(Class.SourceName).Append('*');
					break;

				case UhtPropertyTextType.FunctionThunkParameterArgType:
					builder.Append(Class.SourceName);
					break;

				default:
					builder.Append(Class.SourceName).Append('*');
					break;
			}
			return builder;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDecl(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, int tabs)
		{
			return AppendMemberDecl(builder, context, name, nameSuffix, tabs, "FObjectPropertyParams");
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDef(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, string? offset, int tabs)
		{
			AppendMemberDefStart(builder, context, name, nameSuffix, offset, tabs, "FObjectPropertyParams", "UECodeGen_Private::EPropertyGenFlags::Object");
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
			if (other is UhtObjectProperty otherObject)
			{
				return Class == otherObject.Class && MetaClass == otherObject.MetaClass;
			}
			return false;
		}
	}
}
