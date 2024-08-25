// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using EpicGames.Horde.Storage;

#pragma warning disable CS1591

namespace Horde.Server.Ddc
{
	public class NamespaceNotFoundException : Exception
	{
		public NamespaceId Namespace { get; }

		public NamespaceNotFoundException(NamespaceId @namespace) : base($"Could not find namespace {@namespace}")
		{
			Namespace = @namespace;
		}

		public NamespaceNotFoundException(NamespaceId @namespace, string message) : base(message)
		{
			Namespace = @namespace;
		}
	}
}
