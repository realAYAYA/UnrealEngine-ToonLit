// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using EpicGames.Core;
using EpicGames.Serialization;
using Microsoft.AspNetCore.Mvc;

namespace EpicGames.AspNet
{
    public static class CbConvertersAspNet
    {
        public static void AddAspnetConverters()
        {
            CbConverter.TypeToConverter[typeof(ProblemDetails)] = new CbProblemDetailsConverter();
        }
    }

    /// <summary>
    /// Converter for asp.net problem details type
    /// </summary>
    /// <typeparam name="T"></typeparam>
    class CbProblemDetailsConverter : CbConverterBase<ProblemDetails>
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
                Title = field["title"].AsString(),
                Detail = field["detail"].AsString(),
                Type = field["type"].AsString(),
                Instance = field["instance"].AsString(),
                Status = field["status"].AsInt32()
            };
            return result;
        }

        /// <inheritdoc/>
        public override void Write(CbWriter writer, ProblemDetails problemDetails)
        {
            writer.WriteObject(ToCbObject(problemDetails));
        }

        /// <inheritdoc/>
        public override void WriteNamed(CbWriter writer, Utf8String name, ProblemDetails problemDetails)
        {
            writer.WriteField(name, ToCbObject(problemDetails).AsField());
        }

        private static CbObject ToCbObject(ProblemDetails problemDetails)
        {
            CbWriter objectWriter = new CbWriter();
            objectWriter.BeginObject();

            objectWriter.WriteString("title", problemDetails.Title);
            if (!String.IsNullOrEmpty(problemDetails.Detail))
            {
                objectWriter.WriteString("detail", problemDetails.Detail);
            }

            if (!String.IsNullOrEmpty(problemDetails.Type))
            {
                objectWriter.WriteString("type", problemDetails.Type);
            }

            if (!String.IsNullOrEmpty(problemDetails.Instance))
            {
                objectWriter.WriteString("instance", problemDetails.Instance);
            }

            if (problemDetails.Status.HasValue)
            {
                objectWriter.WriteInteger("status", problemDetails.Status.Value);
            }
            
            objectWriter.EndObject();

            return objectWriter.ToObject();
        }
    }
}
