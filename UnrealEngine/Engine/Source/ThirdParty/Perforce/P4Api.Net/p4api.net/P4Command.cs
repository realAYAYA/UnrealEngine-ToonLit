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
 * Name		: P4Command.cs
 *
 * Author	: dbb
 *
 * Description	: Classes encapsulting running a command on the P4 server, then
 *  collecting and returning the bundled results.
 *
 ******************************************************************************/

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	/// <summary>
	/// Class wrapping command execution.
	/// </summary>
	public class P4Command : IDisposable
	{
        /// <summary>
        /// Keep track of associated P4Server
        /// </summary>
		public P4Server pServer { get; private set; }

        /// <summary>
        /// Keep track of associated Connection
        /// </summary>
		public Connection Connection { get; private set; }

		/// <summary>
		/// Command opcode
		/// </summary>
		private String cmd = String.Empty;

		/// <summary>
		/// Unique Id set each time command is run
		/// </summary>
		public uint CommandId { get; private set; }
		/// <summary>
		/// The arguments used by the command
		/// </summary>
		private StringList args;

		/// <summary>
		/// Tagged protocol flag
		/// </summary>
		private bool tagged;

		private Dictionary<String, String> responses;

		/// <summary>
		/// Capture info results so they can be reformatted
		/// </summary>
		public P4Server.InfoResultsDelegate infoResultsCallbackFn = null;

		// various flavors of output collected over the run of this command
		P4ClientInfoMessageList infoOutput = null;

		// Our override so the info output can be built up as a list and not a string
		private P4Server.InfoResultsDelegate onInfoResultsDelegate = null;
 
        /// <summary>
        /// Handle any Resolve callbacks  from the server
        /// </summary>
		public P4Server.ResolveHandlerDelegate CmdResolveHandler { get; set; }

        /// <summary>
        /// Handle any Resolve callbacks  from the server
        /// </summary>
		public P4Server.ResolveAHandlerDelegate CmdResolveAHandler { get; set; }

        /// <summary>
        /// Handle any input prompts from the server
        /// </summary>
		public P4Server.PromptHandlerDelegate CmdPromptHandler { get; set; }

		/// <summary>
		/// Get the info results from the command execution
		/// </summary>
		public P4ClientInfoMessageList InfoOutput
		{
			get { return infoOutput; }
		}

		/// <summary>
		/// Get the error results from the command execution
		/// </summary>
		public P4ClientErrorList ErrorOutput
		{
			get 
			{
				P4ClientError conErr = pServer.ConnectionError;
				P4ClientErrorList errors = pServer.GetErrorResults(CommandId);
				if (conErr != null)
				{
					if ((errors == null) || (errors.Count == 0))
					{
						errors = new P4ClientErrorList(conErr);
					}
					else
					{
						errors.Insert(0, conErr);
					}
				}
				return errors; 
			}
		}

		/// <summary>
		/// Get the text output from the command execution
		/// </summary>
		public String TextOutput
		{
			get { return pServer.GetTextResults(CommandId); }
		}

		/// <summary>
		/// Get the tagged results from the command execution
		/// </summary>
		public TaggedObjectList TaggedOutput
		{
			get { return pServer.GetTaggedOutput(CommandId); }
		}

		/// <summary>
		/// Get the binary from the command execution
		/// </summary>
		public byte[] BinaryOutput
		{
			get { return pServer.GetBinaryResults(CommandId); }
		}

		/// <summary>
		/// Create a new command
		/// </summary>
		public P4Command(P4Server server)
			:this (server, null)
		{
		}

		/// <summary>
		/// Create a new command
		/// </summary>
		public P4Command(P4Server server, P4Server.PromptHandlerDelegate promptHandler)
		{
			if (server == null)
			{
				throw new ArgumentNullException("server",
					"P4Command requires a P4Server");
			}
			pServer = server;

			CommandId = server.getCmdId();

			onInfoResultsDelegate =
				new P4Server.InfoResultsDelegate(OnInfoOut);

			if (promptHandler != null)
				CmdPromptHandler = promptHandler;
			else
				CmdPromptHandler =
					new P4Server.PromptHandlerDelegate(HandlePrompt);

		}

		/// <summary>
		/// Constructer
		/// </summary>
		/// <param name="connection">Connection to the target Repository</param>
		/// <param name="command">Command String i.e 'submit'</param>
		/// <param name="taggedOutput">Run in tagged protocol</param>
		/// <param name="arguments">Arguments for the command</param>
		public P4Command(	Connection connection,
							String command,
							bool taggedOutput,
							params String[] arguments)
			: this(connection.getP4Server(), command, null, taggedOutput, arguments)
		{
			Connection = connection;
		}

		/// <summary>
		/// Constructer
		/// </summary>
		/// <param name="repository">Target Repository</param>
		/// <param name="command">Command String i.e 'submit'</param>
		/// <param name="taggedOutput">Run in tagged protocol</param>
		/// <param name="arguments">Arguments for the command</param>
		public P4Command(	Repository repository,
							String command,
							bool taggedOutput,
							params String[] arguments)
			: this(repository.Connection.getP4Server(), command, null, taggedOutput, arguments)
		{
			Connection = repository.Connection;
		}

        /// <summary>
        /// Constructer
        /// </summary>
        /// <param name="repository">Target Repository</param>
        /// <param name="command">Command String i.e 'submit'</param>
        /// <param name="promptHandler">Delegate for handler</param>
        /// <param name="taggedOutput">Run in tagged protocol</param>
        /// <param name="arguments">Arguments for the command</param>
        public P4Command(	Repository repository,
							String command,
							P4Server.PromptHandlerDelegate promptHandler,
							bool taggedOutput,
							params String[] arguments)
			: this(repository.Connection.getP4Server(), command, promptHandler, taggedOutput, arguments)
		{
			Connection = repository.Connection;
		}

		/// <summary>
		/// Constructer
		/// </summary>
		/// <param name="server">Target P4Server</param>
		/// <param name="command">Command String i.e 'submit'</param>
		/// <param name="taggedOutput">Run in tagged protocol</param>
		/// <param name="arguments">Arguments for the command</param>
		public P4Command(	P4Server server,
							String command,
							bool taggedOutput,
							params String[] arguments)
			: this(server, command, null, taggedOutput, arguments)
		{
		}

        /// <summary>
        /// Constructer
        /// </summary>
        /// <param name="server">Target P4Server</param>
        /// <param name="command">Command String i.e 'submit'</param>
        /// <param name="promptHandler">Handler function</param>
        /// <param name="taggedOutput">Run in tagged protocol</param>
        /// <param name="arguments">Arguments for the command</param>
        public P4Command(   P4Server server,
							String command,
							P4Server.PromptHandlerDelegate promptHandler,
							bool taggedOutput,
							params String[] arguments)
			: this(server, promptHandler)
		{
			cmd = command;
			tagged = taggedOutput;
			args = arguments;
		}

		/// <summary>
		/// Command String i.e 'submit'
		/// </summary>
		public String Cmd
		{
			get { return cmd; }
			set { cmd = value; }
		}

		/// <summary>
		/// Arguments for the command
		/// </summary>
		public StringList Args
		{
			get { return args; }
			set { args = value; }
		}

		/// <summary>
		/// Run in tagged protocol
		/// </summary>
		public bool Tagged
		{
			get { return tagged; }
			set { tagged = value; }
		}

		/// <summary>
		/// Dictionary of responses to prompts from the server, where the key
		/// is the expected prompt from the server and the value is the 
		/// desired response.
		/// </summary>
		public Dictionary<String, String> Responses
		{
			get { return responses; }
			set { responses = value; }
		}

        /// <summary>
        /// Use the infoResultsReceived event to build up a list of info data.
        /// </summary>
        /// <param name="cmdId">Unique Id for the run of the command</param>
        /// <param name="msgId">Message</param>
        /// <param name="level">level of the message</param>
        /// <param name="info">message text</param>
        private void OnInfoOut(uint cmdId, int msgId, int level, String info)
		{
			infoOutput.Add(new P4ClientInfoMessage(msgId, level, info));
		}

        /// <summary>
        /// Respond to a prompt from the server for input
        /// </summary>
        /// <param name="cmdId">Unique Id for the run of the command</param>
        /// <param name="msg"></param>
        /// <param name="displayText"></param>
        /// <returns></returns>
        private String HandlePrompt(uint cmdId, String msg, bool displayText)
		{
            // NOTE: I'm separating these to make debugging cmdId problems easier
            if (cmdId != CommandId)
                return null;
            if (responses == null)
				return null;

			if (responses.ContainsKey(msg))
				return responses[msg];
			if (responses.ContainsKey("DefaultResponse"))
				return responses["DefaultResponse"];
			if (responses.ContainsKey(String.Empty))
				return responses[String.Empty];
			return null;
		}

		/// <summary>
		/// Data to be processed by the command
		/// </summary>
		public String DataSet
		{
			get { return pServer.GetDataSet(CommandId); }
			set { pServer.SetDataSet(CommandId, value); }
		}
		/// <summary>
		/// Run the command supplying additional arguments
		/// </summary>
		/// <param name="flags">Additional arguments inserted in front of the current arguments</param>
		/// <returns>Success/Failure</returns>
		public P4CommandResult Run(StringList flags)
		{
			lock (this)
			{
				P4CommandResult results = null;
				results = new P4CommandResult(this, flags);
				return results;
			}
		}

		/// <summary>
		/// Run the command using the existing arguments
		/// </summary>
		/// <returns></returns>
		public P4CommandResult Run()
		{
			return new P4CommandResult(this);
		}

		/// <summary>
		/// Run the command supplying additional arguments
		/// </summary>
		/// <param name="flags">Additional arguments inserted in front of the current arguments</param>
		/// <returns></returns>
		internal bool RunInt(StringList flags)
		{
			lock (this)
			{
				// Capture the the info output
				if (onInfoResultsDelegate != null)
					pServer.InfoResultsReceived += onInfoResultsDelegate;

				// Handle any Resolve callbacks from the server
				if (CmdResolveHandler != null)
					pServer.ResolveHandler = CmdResolveHandler;

				// Handle any Resolve callbacks from the server
				if (CmdResolveAHandler != null)
					pServer.ResolveAHandler = CmdResolveAHandler;

				// Handle any prompts for input from the server
				if (CmdPromptHandler != null)
					pServer.PromptHandler = CmdPromptHandler;

				// clear any saved results
				infoOutput = new P4ClientInfoMessageList();

				Exception lastError = null;

				bool success = false;
				try
				{
					StringList paramList = flags + args;

					pServer.EchoCommand(cmd, paramList);

					while (true)
					{
						//retries--;
						try
						{
							success = pServer.RunCommand(	cmd,
															CommandId,
															tagged,
															paramList,
															paramList == null ? 0 : paramList.Count);
							break;
						}
						catch (P4Exception ex)
						{
							if (ex is P4CommandCanceledException)
							{
								throw;
							}
							if (ex is P4CommandTimeOutException)
							{
								if (Connection != null)
								{
									Connection.Disconnect();
								}
								throw;
							}
							if (lastError != null)
							{
								if (Connection != null)
								{
									Connection.Disconnect();
								}
								// been here before, so don't try again
								string msg = string.Format("The connection to the Perforce server at {0} has been lost", pServer.Port);
								P4Exception p4ex = new P4LostConnectionException(ErrorSeverity.E_FATAL, msg);
								throw;
							}
							lastError = ex;

							if ((ex.Message.Contains("socket: WSA")) ||
								P4ClientError.IsTCPError(ex.ErrorCode) || P4ClientError.IsSSLError(ex.ErrorCode))
							{
								try
								{
									pServer.Reconnect();
								}
								catch
								{
									if (Connection != null)
									{
										Connection.Disconnect();
									}
									string msg = string.Format("The connection to the Perforce server at {0} has been lost", pServer.Port);
									P4Exception p4ex = new P4LostConnectionException(ErrorSeverity.E_FATAL, msg);
									throw;
								}
							}
							else
							{
								throw;
							}
						}
						catch (Exception)
						{
							throw;
						}
					}
					if (success)
					{
						// info output is gathered by OnInfoOut()
					}
					else
					{
						// info output is gathered by OnInfoOut()
					}
				}
				catch (Exception ex)
				{
					LogFile.LogException("P4Command", ex);
					throw;
				}
				finally
				{
					// Cancel the redirected the output, this will reset the callbacks if this command does not have callbacks set
					pServer.InfoResultsReceived -= onInfoResultsDelegate;
					pServer.PromptHandler = null;
					pServer.ResolveHandler = null;
					pServer.ResolveAHandler = null;
				}

				return success;
			}
		}

		/// <summary>
		/// Dispose of any resources 
		/// </summary>
		public virtual void Dispose()
		{
			if ((pServer != null) && (pServer.KeepAlive != null))
			{
				pServer.KeepAlive.CommandCompleted(CommandId);
                pServer.Disconnect();
            }
        }
    }
}
