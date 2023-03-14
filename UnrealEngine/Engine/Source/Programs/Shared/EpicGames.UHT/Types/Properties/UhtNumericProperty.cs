// Copyright Epic Games, Inc. All Rights Reserved.

using System.Text;
using System.Text.Json.Serialization;

namespace EpicGames.UHT.Types
{

	/// <summary>
	/// Base class for all numeric properties
	/// </summary>
	public abstract class UhtNumericProperty : UhtProperty
	{
		/// <inheritdoc/>
		protected override string PGetMacroText => "PROPERTY";

		/// <inheritdoc/>
		protected override UhtPGetArgumentType PGetTypeArgument => UhtPGetArgumentType.EngineClass;

		/// <summary>
		/// Describes the integer as either being sized on unsized
		/// </summary>
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public UhtPropertyIntType IntType { get; }

		/// <summary>
		/// Construct new property
		/// </summary>
		/// <param name="propertySettings">Property settings</param>
		/// <param name="intType">Type of integer</param>
		protected UhtNumericProperty(UhtPropertySettings propertySettings, UhtPropertyIntType intType) : base(propertySettings)
		{
			IntType = intType;
			PropertyCaps |= UhtPropertyCaps.RequiresNullConstructorArg | UhtPropertyCaps.SupportsRigVM;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendNullConstructorArg(StringBuilder builder, bool isInitializer)
		{
			builder.Append('0');
			return builder;
		}
	}
}
