// Copyright Epic Games, Inc. All Rights Reserved.

using OpenTracing;

namespace Horde.Agent
{
	static class Tracing
	{
		public static ISpanBuilder WithResourceName(this ISpanBuilder builder, string? resourceName)
		{
			if (resourceName != null)
			{
				builder = builder.WithTag(Datadog.Trace.OpenTracing.DatadogTags.ResourceName, resourceName);
			}
			return builder;
		}

		public static ISpanBuilder WithServiceName(this ISpanBuilder builder, string? serviceName)
		{
			if (serviceName != null)
			{
				builder = builder.WithTag(Datadog.Trace.OpenTracing.DatadogTags.ServiceName, serviceName);
			}
			return builder;
		}
	}
}
