// Copyright Epic Games, Inc. All Rights Reserved.

using System;

namespace EpicGames.Horde.Compute
{
	/// <summary>
	/// Base class for compute errors
	/// </summary>
	public abstract class ComputeException : Exception
	{
		/// <summary>
		/// Constructor
		/// </summary>
		protected ComputeException(string message)
			: base(message)
		{
		}
	}

	/// <summary>
	/// Exception thrown for internal reasons
	/// </summary>
	public sealed class ComputeInternalException : ComputeException
	{
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="message"></param>
		public ComputeInternalException(string message)
			: base(message)
		{
		}
	}

	/// <summary>
	/// Exception thrown on a remote machine
	/// </summary>
	public sealed class ComputeRemoteException : ComputeException
	{
		readonly string _description;

		/// <summary>
		/// Constructor
		/// </summary>
		public ComputeRemoteException(ExceptionMessage message)
			: this(message.Message, message.Description)
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public ComputeRemoteException(string message, string description)
			: base(message)
		{
			_description = description;
		}

		/// <inheritdoc/>
		public override string ToString() => _description;
	}
}
