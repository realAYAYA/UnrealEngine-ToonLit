/*******************************************************************************

Copyright (c) 2010, Perforce Software, Inc.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1.  Redistributions of source code must retain the above copyright
	notice, this list of conditions and the following disclaimer.

2.  Redistributions in binary form must reproduce the above copyright
	notice, this list of conditions and the following disclaimer in the
	documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL PERFORCE SOFTWARE, INC. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

/*******************************************************************************
 * Name		: P4CommandResult.cs
 *
 * Author	: dbb
 *
 * Description	: Class encapsulating the result of a P4Command
 *
 ******************************************************************************/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	/// <summary>
	/// Class representing the results of executing a command on a Perforce Server.
	/// </summary>
	public class P4CommandResult
	{
		private bool success;

		// The command that was run
		private string _cmd = null;
		// The arguments for command that was run
		private string[] _args = null;

		// various flavors of output collected over the run of this command
		private P4ClientInfoMessageList infoOutput = null;
		private P4ClientErrorList errorList = null;
		private String textOutput = null;
		private TaggedObjectList taggedOutput = null;
		private byte[] binaryOutput = null;

		/// <summary>
		/// Did the command not return any errors?
		/// </summary>
		public bool Success { get { return success; } }

		/// <summary>
		/// The info results (if any) of the command execution
		/// </summary>
		public P4ClientInfoMessageList InfoOutput { get { return infoOutput; } }

		/// <summary>
		/// The errors  (if any) of the command execution
		/// </summary>
		public P4ClientErrorList ErrorList { get { return errorList; } }

		/// <summary>
		/// The text output from the command
		/// </summary>
		public String TextOutput { get { return textOutput; } }

		/// <summary>
		/// The tagged output of the command
		/// </summary>
		public TaggedObjectList TaggedOutput { get { return taggedOutput; } }

		/// <summary>
		/// The binary output of the command
		/// </summary>
		public byte[] BinaryOutput { get { return binaryOutput; } }

		/// <summary>
		/// The command that was run
		/// </summary>
		public string Cmd {get { return _cmd;}}
		/// <summary>
		/// The arguments for command that was run
		/// </summary>
		public string[] CmdArgs { get { return _args; } }

		/// <summary>
		/// The time the command completed and the results were collected.
		/// </summary>
		public DateTime TimeStamp { get; private set; }

		private P4CommandResult() { }

		/// <summary>
		/// Create a P4CommandResult by running a command
		/// </summary>
		/// <param name="cmd">Command to run</param>
		/// <param name="flags">Flags for the command</param>
		public P4CommandResult(P4Command cmd, StringList flags)
		{
			try
			{
				_cmd = cmd.Cmd;
				if ((flags != null) || (cmd.Args != null))
				{
					int argc = ((flags != null) ? flags.Count : 0) + ((cmd.Args != null) ? cmd.Args.Count : 0);
					if (argc > 0)
					{
						_args = new string[argc];
						argc = 0;
						if (flags != null)
						{

							for (int idx = 0; idx < flags.Count; idx++)
							{
								_args[argc++] = flags[idx];
							}
						}
						if (cmd.Args != null)
						{
							for (int idx = 0; idx < cmd.Args.Count; idx++)
							{
								_args[argc++] = cmd.Args[idx];
							}
						}
					}
				}
				success = cmd.RunInt(flags);
			}
			finally
			{
				TimeStamp = DateTime.Now;

				infoOutput = cmd.InfoOutput;
				errorList = cmd.ErrorOutput;
				textOutput = cmd.TextOutput;
				taggedOutput = cmd.TaggedOutput;
				binaryOutput = cmd.BinaryOutput;

				cmd.pServer.LastResults = this;
			}
		}

		/// <summary>
		/// Create a P4CommandResult by running a command
		/// </summary>
		/// <param name="cmd">Command to run</param>
		public P4CommandResult(P4Command cmd) : this(cmd, null) { }
	}
}
