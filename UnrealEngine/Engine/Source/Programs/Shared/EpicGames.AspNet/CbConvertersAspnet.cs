// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using EpicGames.Core;
using EpicGames.Serialization;
using Microsoft.AspNetCore.Mvc;

namespace EpicGames.AspNet
{
	/// <summary>
	/// 
	/// </summary>
	public static class CbConvertersAspNet
	{
		/// <summary>
		/// 
		/// </summary>
		public static void AddAspnetConverters()
		{
			CbConverter.TypeToConverter[typeof(ProblemDetails)] = new CbProblemDetailsConverter();
		}
	}

	/// <summary>
	/// Converter for asp.net problem details type
	/// </summary>
	class CbProblemDetailsConverter : CbConverter<ProblemDetails>
	{
		/// <inheritdoc/>
		public override ProblemDetails Read(CbField field)
		{
			if (!field.IsObject())
			{
				throw new CbException($"Error converting field \"{field.Name}\" to ProblemDetails. Expected CbObject.");
			}

			ProblemDetails result = new ProblemDetails
			{
				Title = field[new Utf8String("title")].AsString(),
				Detail = field[new Utf8String("detail")].AsString(),
				Type = field[new Utf8String("type")].AsString(),
				Instance = field[new Utf8String("instance")].AsString(),
				Status = field[new Utf8String("status")].AsInt32()
			};
			return result;
		}

		/// <inheritdoc/>
		public override void Write(CbWriter writer, ProblemDetails problemDetails)
		{
			writer.WriteObject(ToCbObject(problemDetails));
		}

		/// <inheritdoc/>
		public override void WriteNamed(CbWriter writer, CbFieldName name, ProblemDetails problemDetails)
		{
			writer.WriteField(name, ToCbObject(problemDetails).AsField());
		}

		private static CbObject ToCbObject(ProblemDetails problemDetails)
		{
			CbWriter objectWriter = new CbWriter();
			objectWriter.BeginObject();

			objectWriter.WriteString(new Utf8String("title"), problemDetails.Title);
			if (!String.IsNullOrEmpty(problemDetails.Detail))
			{
				objectWriter.WriteString(new Utf8String("detail"), problemDetails.Detail);
			}

			if (!String.IsNullOrEmpty(problemDetails.Type))
			{
				objectWriter.WriteString(new Utf8String("type"), problemDetails.Type);
			}

			if (!String.IsNullOrEmpty(problemDetails.Instance))
			{
				objectWriter.WriteString(new Utf8String("instance"), problemDetails.Instance);
			}

			if (problemDetails.Status.HasValue)
			{
				objectWriter.WriteInteger(new Utf8String("status"), problemDetails.Status.Value);
			}

			objectWriter.EndObject();

			return objectWriter.ToObject();
		}
	}
}
