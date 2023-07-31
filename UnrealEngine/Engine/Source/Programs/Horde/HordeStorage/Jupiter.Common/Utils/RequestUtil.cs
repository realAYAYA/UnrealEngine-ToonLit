// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Threading.Tasks;
using Datadog.Trace;
using Microsoft.AspNetCore.Http;

namespace Jupiter
{
    public static class RequestUtil
    {
        public static async Task<byte[]> ReadRawBody(HttpRequest request)
        {
            long? contentLength = request.GetTypedHeaders().ContentLength;

            if (contentLength == null)
            {
                throw new Exception("Expected content-length on all raw body requests");
            }

            using IScope scope = Tracer.Instance.StartActive("readbody");
            scope.Span.SetTag("Content-Length", contentLength.ToString());
            await using MemoryStream ms = new MemoryStream((int)contentLength);
            DateTime readStart = DateTime.Now;
            await request.Body.CopyToAsync(ms);
            TimeSpan duration = DateTime.Now - readStart;

            double rate = contentLength.Value / duration.TotalSeconds;
            StatsdClient.DogStatsd.Histogram("jupiter.stream_throughput", rate, tags: new string[] {"sourceIdentifier:" + "readbody"});

            return ms.GetBuffer();
        }
    }
}
