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
 * Name		: P4Bridge.cs
 *
 * Author	: dbb
 *
 * Description	: Class containing the dll imports for the P4Bridge dll.
 *
 ******************************************************************************/
using System;
using System.Collections;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.ComponentModel;
namespace Perforce.P4
{

    public class P4Debugging
    {
		const string bridgeDll = "p4bridge";
		static P4Debugging()
		{
			Assembly p4apinet = Assembly.GetExecutingAssembly();
			PortableExecutableKinds peKind;
			ImageFileMachine machine;
			p4apinet.ManifestModule.GetPEKind(out peKind, out machine);

			// only set this path if it is Any CPU (ILOnly)
			if (peKind.ToString() == "ILOnly")
			{
				/*
				string currentArchSubPath = "x86";

				// Is this a 64 bits process?
				if (IntPtr.Size == 8)
				{
					currentArchSubPath = "x64";
				}
				SetDllDirectory(currentArchSubPath);
				*/
			}
		}

		// EPIC BEGIN
		/// <summary>
		/// Simulates a crash (access violation)
		/// </summary>
		[DllImport(bridgeDll, EntryPoint = "DebugCrash", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
		public static extern void DebugCrash();

		//[DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
		//static extern bool SetDllDirectory(string lpPathName);
		// EPIC END

		/* object allocation debugging functions, mostly for testing */
		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl)]
        public static extern int GetAllocObjCount();

        [DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl)]
        public static extern int GetAllocObj(int type);

        [DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr GetAllocObjName(int type);

        [DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl)]
        public static extern void SetLogFunction(P4CallBacks.LogMessageDelegate logfn);

        [DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl)]
        public static extern long GetStringAllocs();

        [DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl)]
        public static extern long GetStringReleases();


        static public String GetAllocObjectName(int type)
        {
            return P4Server.MarshalPtrToStringUtf8_Int(P4Debugging.GetAllocObjName(type));
        }

        static public int GetAllocObjectCount()
        {
            return P4Debugging.GetAllocObjCount();
        }

        static public int GetAllocObject(int type)
        {
            return P4Debugging.GetAllocObj(type);
        }

        static public void SetBridgeLogFunction(P4CallBacks.LogMessageDelegate logfn)
        {
            P4Debugging.SetLogFunction(logfn);
        }
    }

    /// <summary>
    /// Class wrapper for the definitions of delegates required to model the 
    /// callbacks from the bridge dll.
    /// </summary>
    public class P4CallBacks
	{
		/***********************************************************************
		 * 
		 * Delegates
		 * 
		 **********************************************************************/

		/// <summary>
		/// Delegate definition for the Logging callback from the dll.
		/// </summary>
		/// <param name="log_level">Log Level Lower values are more serious 
		///     than higher values.</param>
		/// <param name="file">Name of the cpp file containing the call.
		/// </param>
		/// <param name="line">Line number in the cpp file of the call.</param>
		/// <param name="message">Descriptive message to be logged.</param>
		public delegate void LogMessageDelegate(int log_level,
												String file,
												int line,
												String message);

		/// <summary>
		/// Delegate definition for the tagged output delegate.
		/// </summary>
		/// <remarks>
		/// Each call of this delegate by the bridge provides a single Key:Value
		/// pair for the current object. When all the Key:Value pairs for an 
		/// object have been sent, the bridge will make one final call with null 
		/// for the values of the key and value to signify it is complete. Each 
		/// object generated by a command will have an object ID that unique for
		/// that command.
		/// </remarks>
		/// <param name="cmdID">Id if the command making the callback</param>
		/// <param name="objID">Object ID for the object</param>
		/// <param name="key">The Key of this Key:Value pair</param>
		/// <param name="value">The Key of this Key:Value pair</param>
		public delegate void TaggedOutputDelegate(uint cmdID, int objID, [MarshalAs(UnmanagedType.LPStr)] String key, IntPtr value);

		/// <summary>
		/// Delegate definition for the error callback.
		/// </summary>
		/// <param name="cmdID">Id if the command making the callback</param>
		/// <param name="severity">Severity of the error</param>
        /// <param name="errorId"></param>
		/// <param name="info">Error Message</param>
		public delegate void ErrorDelegate(uint cmdID, int severity, int errorId, IntPtr info);

		/// <summary>
		/// Delegate definition for the info results callback.
		/// </summary>
		/// <remarks>
		/// The "info" results generally are the output of commands that are run
		/// <b>not</b> using tagged protocol
		/// </remarks>
		/// <param name="cmdID">Id of the command making the callback</param>
		/// <param name="msgID">Unique Id of the message from the server</param>
		/// <param name="level"></param>
		/// <param name="info"></param>
		public delegate void InfoResultsDelegate(uint cmdID, int msgID, int level, IntPtr info);

		/// <summary>
		/// Delegate definition for the text results callback.
		/// </summary>
		/// <remarks>
		/// If this callback is used, the text output generated by a command 
		/// will be delivered by one or more call to the supplied delegate.
		/// It multiple calls are made, the entire text is obtained by 
		/// concatenating the text from each call.
		/// </remarks>
		/// <param name="cmdID">Id if the command making the callback</param>
		/// <param name="info">Text output produced by the command</param>
		public delegate void TextResultsDelegate(uint cmdID, IntPtr info);

		/// <summary>
		/// Delegate definition for the binary results callback.
		/// </summary>
		/// <remarks>
		/// If this callback is used, the binary output generated by a command 
		/// will be delivered by one or more call to the supplied delegate.
		/// It multiple calls are made, the entire output is obtained by 
		/// concatenating the data from each call.
		/// </remarks>
		/// <param name="cmdID">Id if the command making the callback</param>
		/// <param name="data">Binary data generated by the command</param>
		/// <param name="count">The size in bytes of the data</param>
		public delegate void BinaryResultsDelegate(uint cmdID, IntPtr data, int count);

		/// <summary>
		/// Delegate definition for the prompt callback.
		/// </summary>
		/// <param name="cmdID">Id if the command making the callback</param>
		/// <param name="msg">Prompt message from the server</param>
		/// <param name="rspBuf">Character buffer to receive the response</param>
		/// <param name="bufSz">Size of the buffer</param>
		/// <param name="dispayText">Display flog</param>
		public delegate void PromptDelegate(uint cmdID, 
											IntPtr msg,
											IntPtr rspBuf,
											int bufSz,
											bool dispayText);

        /// <summary>
		/// Delegate definition for the parallel operations callback.
		/// </summary>
		/// <param name="pServer">Pointer the the bridge server (needed?)</param>
		/// <param name="cmd">Prompt message from the server</param>
		/// <param name="args">array of arguments</param>
		/// <param name="argCount">number of arguments</param>
		/// <param name="dictIter">dictionary of variables</param>
		/// <param name="threads">number of threads to launch</param>
		public delegate int ParallelTransferDelegate(IntPtr pServer, String cmd, [MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 3)] String[] args, uint argCount, IntPtr dictIter, uint threads);

		/// <summary>
		/// Delegate definition for the Resolve callback passing a CLientMerge object.
		/// </summary>
		/// <param name="Merger">P4ClientMerge used to handle the resolve</param>
        /// <param name="cmdID">Unique Id for the run of the command</param>
		/// <returns>Resolve result</returns>
		public delegate int ResolveDelegate(uint cmdID, IntPtr Merger);

		/// <summary>
		/// Delegate definition for the Resolve callback passing a CLientResolve object.
		/// </summary>
		/// <param name="cmdID">Id if the command making the callback</param>
		/// <param name="Resolver">P4ClientReslove used to handle the resolve</param>
		/// <param name="preview">Preview only</param>
		/// <returns>Resolve result</returns>
		public delegate int ResolveADelegate(uint cmdID, IntPtr Resolver, bool preview);
	}

	/// <summary>
	/// Class containing the DLL imports for the P4Bridge DLL.
	/// </summary>
	internal class P4Bridge
	{

/***********************************************************************
 * 
 * DllImports
 * 
 **********************************************************************/

        const string bridgeDll = "p4bridge";

		static P4Bridge()
		{
			Assembly p4apinet = Assembly.GetExecutingAssembly();
			PortableExecutableKinds peKind;
			ImageFileMachine machine;
			p4apinet.ManifestModule.GetPEKind(out peKind, out machine);

			// only set this path if it is Any CPU (ILOnly)
			if (peKind.ToString()=="ILOnly")
			{
				/*
				string currentArchSubPath = "x86";

				// Is this a 64 bits process?
				if (IntPtr.Size == 8)
				{
					currentArchSubPath = "x64";
				}
				SetDllDirectory(currentArchSubPath);
				*/
			}
		}

		//[DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
		//static extern bool SetDllDirectory(string lpPathName);

		/// <summary>
		/// Create a new P4BridgeServer in the DLL and connect to the 
		///     specified P4 Server.
		/// </summary>
		/// <param name="server">Host:port for the P4 server.</param>
		/// <param name="user">User name for the login. 
		///     Can be null/blank if only running commands that do not require 
		///     a login.</param>
		/// <param name="password">Password for  the login. Can be null/blank if 
		///     only running commands that do not require a login.</param>
		/// <param name="ws_client">Workspace (client) to be used by the 
		///     connection. Can be null/blank if only running commands that do 
		///     not require a login.</param>
		/// <param name="logfn">Function pointer for the logging callback. Can
		///     be null if logging is not desired.</param>
		/// <returns>Handle (pointer) to the P4BridgeServer</returns>
		[DllImport(bridgeDll, EntryPoint = "Connect",
			CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
		public static extern
			IntPtr ConnectA(	String server,
								String user,
								String password,
								String ws_client,
								P4CallBacks.LogMessageDelegate logfn);

		/// <summary>
		/// Create a new P4BridgeServer in the DLL and connect to the 
		///     specified P4 Server.
		/// </summary>
		/// <param name="server">Host:port for the P4 server.</param>
		/// <param name="user">User name for the login. 
		///     Can be null/blank if only running commands that do not require 
		///     a login.</param>
		/// <param name="password">Password for  the login. Can be null/blank if 
		///     only running commands that do not require a login.</param>
		/// <param name="ws_client">Workspace (client) to be used by the 
		///     connection. Can be null/blank if only running commands that do 
		///     not require a login.</param>
		/// <param name="logfn">Function pointer for the logging callback. Can
		///     be null if logging is not desired.</param>
		/// <returns>Handle (pointer) to the P4BridgeServer</returns>
		[DllImport(bridgeDll, EntryPoint = "Connect",
			CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
		public static extern
			IntPtr ConnectW(String server,
								IntPtr user,
								IntPtr password,
								IntPtr ws_client,
								IntPtr logfn);

		/// <summary>
		/// Create a new P4BridgeServer in the DLL and connect to the 
		///     specified P4 Server.
		/// </summary>
		/// <param name="server">Host:port for the P4 server.</param>
		/// <param name="user">User name for the login. 
		///     Can be null/blank if only running commands that do not require 
		///     a login.</param>
		/// <param name="password">Password for  the login. Can be null/blank if 
		///     only running commands that do not require a login.</param>
		/// <param name="ws_client">Workspace (client) to be used by the 
		///     connection. Can be null/blank if only running commands that do 
		///     not require a login.</param>
		/// <param name="trust_flag">Flag to pass to the trust command.</param>
		/// <param name="fingerprint">Fingerprint to pass to the trust
		///		command.</param>
		/// <param name="logfn">Function pointer for the logging callback. Can
		///     be null if logging is not desired.</param>
		/// <returns>Handle (pointer) to the P4BridgeServer</returns>
		[DllImport(bridgeDll, EntryPoint = "TrustedConnect",
			CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
		public static extern
			IntPtr TrustedConnectA(String server,
								String user,
								String password,
								String ws_client,
								String trust_flag,
								String fingerprint,
								P4CallBacks.LogMessageDelegate logfn);

		/// <summary>
		/// Create a new P4BridgeServer in the DLL and connect to the 
		///     specified P4 Server.
		/// </summary>
		/// <param name="server">Host:port for the P4 server.</param>
		/// <param name="user">User name for the login. 
		///     Can be null/blank if only running commands that do not require 
		///     a login.</param>
		/// <param name="password">Password for  the login. Can be null/blank if 
		///     only running commands that do not require a login.</param>
		/// <param name="ws_client">Workspace (client) to be used by the 
		///     connection. Can be null/blank if only running commands that do 
		///     not require a login.</param>
		/// <param name="trust_flag">Flag to pass to the trust command.</param>
		/// <param name="fingerprint">Fingerprint to pass to the trust
		///		command.</param>
		/// <param name="logfn">Function pointer for the logging callback. Can
		///     be null if logging is not desired.</param>
		/// <returns>Handle (pointer) to the P4BridgeServer</returns>
		[DllImport(bridgeDll, EntryPoint = "TrustedConnect",
			CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
		public static extern
			IntPtr TrustedConnectW(String server,
								IntPtr user,
								IntPtr password,
								IntPtr ws_client,
								String trust_flag,
								String fingerprint,
								IntPtr logfn);

        /// <summary>
        /// Create a new P4BridgeServer in the DLL based on the path
        /// </summary>
        /// <param name="path">Path from which to assume configuration info</param>
        /// <returns>Handle (pointer) to the P4BridgeServer</returns>
        [DllImport(bridgeDll, EntryPoint = "ConnectionFromPath",
            CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern
            IntPtr ConnectionFromPath(String path);

        /// <summary>
        /// Get the error message generated by the previous connection (if any)
        /// </summary>
        /// <returns>Error Message. Null if no error occurred</returns>
        [DllImport(bridgeDll, EntryPoint = "GetConnectionError",
			CallingConvention = CallingConvention.Cdecl)]
		public static extern IntPtr GetConnectionError();

        /// <summary>
        /// Release error message from GetConnectionError
        /// </summary>
        /// <returns>Error Message. Null if no error occurred</returns>
        [DllImport(bridgeDll, EntryPoint = "ClearConnectionError",
            CallingConvention = CallingConvention.Cdecl)]
        public static extern void ClearConnectionError();

        /// <summary>
        /// Sets the character set used by the P4Bridge server if connection is 
        ///     to Unicode enabled server.
        /// </summary>
        /// <param name="pServer">Handle of the P4BridgeServer</param>
        /// <param name="charSet">Character set to use for Unicode data 
        ///     exchanged with the P4 Server</param>
        /// <param name="FileCharSet">Character set to use for files 
        /// stored in 
        ///     the client' host file space</param>
        /// <returns>null if no error, otherwise a pointer to an error 
        /// message.</returns>
        [DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			IntPtr SetCharacterSet(IntPtr pServer,
									String charSet,
									String FileCharSet);

		/// <summary>
		/// Close the connection
		/// </summary>
		/// <param name="pServer">P4BridgeServer Handle</param>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
        public static extern int Disconnect(IntPtr pServer);

        /// <summary>
        /// Close the connection
        /// </summary>
        /// <param name="pServer">P4BridgeServer Handle</param>
        [DllImport(bridgeDll,
            CallingConvention = CallingConvention.Cdecl)]
		public static extern int ReleaseConnection(IntPtr pServer);

		/// <summary>
		/// Check if the server supports Unicode
		/// </summary>
		/// <remarks>
		/// Note: Is set during connection so is valid immediately after 
		/// Connect() successfully completes.
		/// </remarks>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <returns></returns>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern bool IsUnicode(IntPtr pServer);

		/// <summary>
		/// Check to see what API level the server supports
		/// </summary>
		/// <remarks>
		/// Note: Is set during connection so is valid immediately after 
		/// Connect() successfully completes.
		/// </remarks>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <returns></returns>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern int APILevel(IntPtr pServer);

		/// <summary>
		/// Check if the server requires the login command be used
		/// </summary>
		/// <remarks>
		/// Note: Is set during connection so is valid immediately after 
		/// Connect() successfully completes.
		/// </remarks>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <returns></returns>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern bool UseLogin(IntPtr pServer);

		/// <summary>
		/// Check if the server supports extended submit options (2006.2 higher)
		/// </summary>
		/// <remarks>
		/// Note: Is set during connection so is valid immediately after 
		/// Connect() successfully completes.
		/// </remarks>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <returns></returns>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern bool SupportsExtSubmit(IntPtr pServer);

        /// <summary>
		/// Check if the ClientUser::HandleUrl method has been called
        /// and an url has been launched
		/// </summary>
		/// <remarks>
		/// P4BridgeClient::HandleUrl is the override that sets a bool
		/// and calls ClientUser::HandleUrl.
		/// </remarks>
        [DllImport(bridgeDll,
            CallingConvention = CallingConvention.Cdecl)]
        public static extern bool UrlLaunched();

        /// <summary>
        /// Raw SetProtocol tunnel for an existing connection
        /// </summary>
        /// <param name="pServer">P4BridgeServer handle</param>
        /// <param name="var">protocol variable name</param>
        /// <param name="val">protocol variable value</param>
        /// 
        [DllImport(bridgeDll,
            CallingConvention = CallingConvention.Cdecl)]
        public static extern void SetProtocol(IntPtr pServer, String var, String val);

        /// <summary>
        /// Run a command on the P4 Server
        /// </summary>
        /// <remarks>
        /// The A version is used to pass ASCII parameters
        /// </remarks>
        /// <param name="pServer">P4BridgeServer Handle</param>
        /// <param name="cmd">Command. i.e "fstat"</param>
        /// <param name="cmdId">Unique Id for the run of the command</param>
        /// <param name="tagged">If true, use tagged protocol the receive the 
        /// output</param>
        /// <param name="args">Arguments for the command</param>
        /// <param name="argc">Argument count</param>
        /// <returns></returns>
        [DllImport(bridgeDll, EntryPoint = "RunCommand",
			CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
		public static extern
			bool RunCommandA(	IntPtr pServer,
								String cmd,
								uint cmdId, 
								bool tagged,
								String[] args,
								int argc);

		/// <summary>
		/// Run a command on the P4 Server
		/// </summary>
		/// <remarks>
		/// The W version is used to pass Unicode (wide) parameters
		/// </remarks>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <param name="cmd">Command. i.e "fstat"</param>
		/// <param name="cmdId">Unique Id for the run of the command</param>
		/// <param name="tagged">If true, use tagged protocol the receive the 
		/// output</param>
		/// <param name="args">Arguments for the command</param>
		/// <param name="argc">Argument count</param>
		/// <returns></returns>
		[DllImport(bridgeDll, EntryPoint = "RunCommand",
			CallingConvention = CallingConvention.Cdecl,
			CharSet = CharSet.Ansi)]
		public static extern
			bool RunCommandW(	IntPtr pServer,
								String cmd,
								uint cmdId,
								bool tagged,
								IntPtr[] args,
								int argc);

		/// <summary>
		/// Cancel a running command
		/// </summary>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <param name="CmdId">Unique Id for the run of the command</param>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			void CancelCommand(IntPtr pServer, uint CmdId);

		/// <summary>
		/// Have we told the server to disconnect?
		/// </summary>
		/// <param name="pServer">P4BridgeServer Handle</param>
        /// <returns>boolean if we have an active p4 connection</returns>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			int IsConnected(IntPtr pServer);

		/***********************************************************************
		 * 
		 * Callback management
		 * 
		 **********************************************************************/

		/// <summary>
		/// Set the callback for tagged output
		/// </summary>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <param name="pNew">Pinned pointer to the callback delegate</param>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			void SetTaggedOutputCallbackFn(IntPtr pServer, IntPtr pNew);

		/// <summary>
		/// Set the callback for error output
		/// </summary>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <param name="pNew">Pinned pointer to the callback delegate</param>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			void SetErrorCallbackFn(IntPtr pServer, IntPtr pNew);

		/// <summary>
		/// Set the callback for Info (non tagged) output
		/// </summary>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <param name="pcb">Pinned pointer to the callback delegate</param>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			void SetInfoResultsCallbackFn(IntPtr pServer, IntPtr pcb);

		/// <summary>
		/// Set the callback for text output
		/// </summary>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <param name="pNew">Pinned pointer to the callback delegate</param>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			void SetTextResultsCallbackFn(IntPtr pServer, IntPtr pNew);

		/// <summary>
		/// Set the callback for binary output
		/// </summary>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <param name="pNew">Pinned pointer to the callback delegate</param>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			void SetBinaryResultsCallbackFn(IntPtr pServer, IntPtr pNew);

		/// <summary>
		/// Set the call back to receive input prompts from the sever
		/// </summary>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <param name="pNew">Pinned pointer to the callback delegate</param>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			 void SetPromptCallbackFn(IntPtr pServer, IntPtr pNew);

        /// <summary>
        /// Set the call back to receive parallel sync handling (see ClientTransfer)
        /// </summary>
        /// <param name="pServer">P4BridgeServer Handle</param>
        /// <param name="pNew">Pinned pointer to the callback delegate</param>
        [DllImport(bridgeDll,
            CallingConvention = CallingConvention.Cdecl)]
        public static extern
             void SetParallelTransferCallbackFn(IntPtr pServer, IntPtr pNew);
        
        /***********************************************************************
		  * 
		  * Output retrieval
		  * 
		  **********************************************************************/

        /// <summary>
        /// Get the number of entries in the tagged output for the last command
        /// </summary>
        /// <param name="pServer">P4BridgeServer Handle</param>
        /// <param name="cmdId">Unique Id for the run of the command</param>
        /// <returns>StrDictListIterator Handle (pointer) used to read the data</returns>
        [DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			int GetTaggedOutputCount(IntPtr pServer, uint cmdId);

		/// <summary>
		/// Get the tagged output for the last command
		/// </summary>
		/// <param name="pServer">P4BridgeServer Handle</param>
        /// <param name="cmdId">Unique Id for the run of the command</param>
		/// <returns>StrDictListIterator Handle (pointer) used to read the data</returns>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			IntPtr GetTaggedOutput(IntPtr pServer, uint cmdId);

		/// <summary>
		/// Get the error output for the last command
		/// </summary>
		/// <param name="pServer">P4BridgeServer Handle</param>
        /// <param name="cmdId">Unique Id for the run of the command</param>
		/// <returns>P4ClientErrorList Handle (pointer) used to read the data</returns>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			IntPtr GetErrorResults(IntPtr pServer, uint cmdId);

		/// <summary>
		/// Get the info output for the last command
		/// </summary>
		/// <param name="pServer">P4BridgeServer Handle</param>
        /// <param name="cmdId">Unique Id for the run of the command</param>
		/// <returns>Handle (pointer) to the string data</returns>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			int GetInfoResultsCount(IntPtr pServer, uint cmdId);

		/// <summary>
		/// Get the info output for the last command
		/// </summary>
		/// <param name="pServer">P4BridgeServer Handle</param>
        /// <param name="cmdId">Unique Id for the run of the command</param>
		/// <returns>Handle (pointer) to the string data</returns>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			IntPtr GetInfoResults(IntPtr pServer, uint cmdId);

		/// <summary>
		/// Get the text output for the last command
		/// </summary>
		/// <param name="pServer">P4BridgeServer Handle</param>
        /// <param name="cmdId">Unique Id for the run of the command</param>
		/// <returns>Handle (pointer) to the string data</returns>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			IntPtr GetTextResults(IntPtr pServer, uint cmdId);

		/// <summary>
		/// Get the byte count for the binary output for the last command
		/// </summary>
		/// <param name="pServer">P4BridgeServer Handle</param>
        /// <param name="cmdId">Unique Id for the run of the command</param>
		/// <returns>Handle (pointer) to the data bytes</returns>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			int GetBinaryResultsCount(IntPtr pServer, uint cmdId);

		/// <summary>
		/// Get the binary output for the last command
		/// </summary>
		/// <param name="pServer">P4BridgeServer Handle</param>
        /// <param name="cmdId">Unique Id for the run of the command</param>
		/// <returns>Handle (pointer) to the data bytes</returns>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			IntPtr GetBinaryResults(IntPtr pServer, uint cmdId);

		/***********************************************************************
		 * 
		 * Command Data Set
		 * 
		 **********************************************************************/

		/// <summary>
		/// Set the data set using Unicode data
		/// </summary>
		/// <remarks>
		/// The encoding should match that set by SetCharacterSet()
		/// </remarks>
		/// <param name="pServer">P4BridgeServer Handle</param>
        /// <param name="cmdId">Unique Id for the run of the command</param>
		/// <param name="data">New Unicode data set</param>
		[DllImport(bridgeDll, EntryPoint = "SetDataSet",
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			void SetDataSetW(IntPtr pServer, uint cmdId, IntPtr data);

		/// <summary>
		/// Set the data set using Unicode data
		/// </summary>
		/// <param name="pServer">P4BridgeServer Handle</param>
        /// <param name="cmdId">Unique Id for the run of the command</param>
		/// <param name="data">New ASCII data set</param>
		[DllImport(bridgeDll, EntryPoint = "SetDataSet",
			CallingConvention = CallingConvention.Cdecl,
			CharSet = CharSet.Ansi)]
		public static extern
			void SetDataSetA(IntPtr pServer, uint cmdId, String data);

		// Epic
		[DllImport(bridgeDll, EntryPoint = "SetConnectionHost", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
		public static extern void SetConnectionHost(IntPtr pServer, String hostname);


		/// <summary>
		/// Read the data set
		/// </summary>
		/// <remarks>
		/// The data will be encoded in ASII or Unicode, depending on the 
		/// server configuration and character set set previously with 
		/// SetCharacterSet().
		/// </remarks>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <param name="cmdId">Unique Id for the run of the command</param>
		/// <returns>The data in the data set</returns>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			IntPtr GetDataSet(IntPtr pServer, uint cmdId);

		/***********************************************************************
		 * 
		 * Connect parameters
		 * 
		 **********************************************************************/

		/// <summary>
		/// Set the connection parameters using Unicode strings.
		/// </summary>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <param name="newPort">New port </param>
		/// <param name="newUser">New workspace</param>
		/// <param name="newPassword">New password</param>
		/// <param name="newClient">New workspace</param>
		[DllImport(bridgeDll, EntryPoint = "set_connection",
					CallingConvention = CallingConvention.Cdecl)]
		public static extern
			void set_connectionW(IntPtr pServer,
									IntPtr newPort,
									IntPtr newUser,
									IntPtr newPassword,
									IntPtr newClient);

		/// <summary>
		/// Set the connection parameters using ASCII strings.
		/// </summary>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <param name="newPort">New port </param>
		/// <param name="newUser">New workspace</param>
		/// <param name="newPassword">New password</param>
		/// <param name="newClient">New workspace</param>
		[DllImport(bridgeDll, EntryPoint = "set_connection",
					CallingConvention = CallingConvention.Cdecl,
					CharSet = CharSet.Ansi)]
		public static extern
			void set_connectionA(	IntPtr pServer,
									String newPort,
									String newUser,
									String newPassword,
									String newClient);

		/// <summary>
		/// Set the client workspace using a Unicode string
		/// </summary>
		/// <remarks>
		/// The encoding should match that set by SetCharacterSet()
		/// </remarks>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <param name="workspace">Client Workspace name</param>
		[DllImport(bridgeDll, EntryPoint = "set_client",
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			void set_clientW(IntPtr pServer, IntPtr workspace);

		/// <summary>
		/// Set the client workspace using a ASCII string
		/// </summary>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <param name="workspace">Client Workspace name</param>
		[DllImport(bridgeDll, EntryPoint = "set_client",
			CallingConvention = CallingConvention.Cdecl,
			CharSet = CharSet.Ansi)]
		public static extern
			void set_clientA(IntPtr pServer, String workspace);

		/// <summary>
		/// Get the name of the client workspace
		/// </summary>
		/// <remarks>
		/// The encoding should match that set by SetCharacterSet()
		/// </remarks>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <returns>Client workspace name</returns>
		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl)]
		public static extern
			IntPtr get_client(IntPtr pServer);

		/// <summary>
		/// Set the user name using a Unicode string
		/// </summary>
		/// <remarks>
		/// The encoding should match that set by SetCharacterSet()
		/// </remarks>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <param name="workspace">User name</param>
		[DllImport(bridgeDll, EntryPoint = "set_user",
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			void set_userW(IntPtr pServer, IntPtr workspace);

		/// <summary>
		/// Set the user name using a ASCII string
		/// </summary>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <param name="workspace">User name</param>
		[DllImport(bridgeDll, EntryPoint = "set_user",
			CallingConvention = CallingConvention.Cdecl,
			CharSet = CharSet.Ansi)]
		public static extern
			void set_userA(IntPtr pServer, String workspace);

		/// <summary>
		/// Get the name of the current user
		/// </summary>
		/// <remarks>
		/// The encoding should match that set by SetCharacterSet()
		/// </remarks>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <returns>The user's name</returns>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl,
			CharSet = CharSet.Ansi)]
		public static extern
			IntPtr get_user(IntPtr pServer);

		/// <summary>
		/// Set the connection string using a Unicode string
		/// </summary>
		/// <remarks>
		/// The encoding should match that set by SetCharacterSet()
		/// </remarks>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <param name="workspace">Connection String</param>
		[DllImport(bridgeDll, EntryPoint = "set_port",
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			void set_portW(IntPtr pServer, IntPtr workspace);

		/// <summary>
		/// Set the connection string using a ASCII string
		/// </summary>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <param name="workspace">Connection String</param>
		[DllImport(bridgeDll, EntryPoint = "set_port",
			CallingConvention = CallingConvention.Cdecl,
			CharSet = CharSet.Ansi)]
		public static extern
			void set_portA(IntPtr pServer, String workspace);

		/// <summary>
		/// Get the connection string.
		/// </summary>
		/// <remarks>
		/// The encoding should match that set by SetCharacterSet()
		/// </remarks>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <returns>host:port used by the connection</returns>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			IntPtr get_port(IntPtr pServer);

		/// <summary>
		/// Set the user's password using a Unicode string
		/// </summary>
		/// <remarks>
		/// The encoding should match that set by SetCharacterSet()
		/// </remarks>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <param name="workspace">User's password</param>
		[DllImport(bridgeDll, EntryPoint = "set_password",
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			void set_passwordW(IntPtr pServer, IntPtr workspace);

		/// <summary>
		/// Set the user's password using an ASCII string
		/// </summary>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <param name="workspace">User's password</param>
		[DllImport(bridgeDll, EntryPoint = "set_password",
			CallingConvention = CallingConvention.Cdecl,
			CharSet = CharSet.Ansi)]
		public static extern
			void set_passwordA(IntPtr pServer, String workspace);


		/// <summary>
		/// Get the password used for the connection
		/// </summary>
		/// <remarks>
		/// The encoding should match that set by SetCharacterSet()
		/// </remarks>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <returns>The password</returns>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			IntPtr get_password(IntPtr pServer);

        /// <summary>
        /// Set the ticket file using an Unicode string
        /// </summary>
        /// <remarks>
        /// The encoding should match that set by SetCharacterSet()
        /// </remarks>
        /// <param name="pServer">P4BridgeServer Handle</param>
        /// <param name="ticketFile">ticket file name</param>
        [DllImport(bridgeDll, EntryPoint = "set_ticketFile",
            CallingConvention = CallingConvention.Cdecl)]
        public static extern
            void set_ticketFileW(IntPtr pServer, IntPtr ticketFile);

        /// <summary>
        /// Set the ticket file using an ASCII string
        /// </summary>
        /// <remarks>
        /// The encoding should match that set by SetCharacterSet()
        /// </remarks>
        /// <param name="pServer">P4BridgeServer Handle</param>
        /// <param name="ticketFile">ticket file name</param>
        [DllImport(bridgeDll, EntryPoint = "set_ticketFile",
            CallingConvention = CallingConvention.Cdecl,
            CharSet = CharSet.Ansi)]
        public static extern
            void set_ticketFileA(IntPtr pServer, IntPtr ticketFile);

        /// <summary>
        /// Get the ticket file using an Unicode string
        /// </summary>
        /// <remarks>
        /// The encoding should match that set by SetCharacterSet()
        /// </remarks>
        /// <param name="pServer">P4BridgeServer Handle</param>
        /// <returns>The ticket file path, if set for the connection.</returns>
        [DllImport(bridgeDll, EntryPoint = "get_ticketFile",
            CallingConvention = CallingConvention.Cdecl)]
        public static extern
            IntPtr get_ticketFileW(IntPtr pServer);

        /// <summary>
        /// Get the ticket using a path, port, and user
        /// </summary>
        /// <remarks>
        /// The encoding should match that set by SetCharacterSet()
        /// </remarks>
        /// <param name="path">Ticket file path</param>
        /// <param name="port">Server port</param>
        /// <param name="user">Current user</param>
        /// <returns>The current ticket, if any.</returns>

        [DllImport(bridgeDll, EntryPoint = "get_ticket",
            CallingConvention = CallingConvention.Cdecl)]
        public static extern
            IntPtr get_ticket(IntPtr path, IntPtr port, IntPtr user);

        /// <summary>
        /// Gets the current working directory for the P4BridgeServer.
        /// </summary>
        /// <param name="pServer">P4BridgeServer Handle</param>
        /// <returns>The current working directory for the P4BridgeServer.</returns>
        [DllImport(bridgeDll,
				CallingConvention = CallingConvention.Cdecl)]
		public static extern
			IntPtr get_cwd(IntPtr pServer);

		/// <summary>
		/// Sets the current working directory for the P4BridgeServer using a Unicode string.
		/// </summary>
		/// <remarks>
		/// The encoding should match that set by SetCharacterSet()
		/// </remarks>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <param name="cwd">The new working directory</param>
		[DllImport(bridgeDll, EntryPoint = "set_cwd",
				CallingConvention = CallingConvention.Cdecl)]
		public static extern
			void set_cwdW(IntPtr pServer, IntPtr cwd);

		/// <summary>
		/// Sets the current working directory for the P4BridgeServer using an ASCII string.
		/// </summary>
		/// <remarks>
		/// The encoding should match that set by SetCharacterSet()
		/// </remarks>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <param name="cwd">The new working directory</param>
		[DllImport(bridgeDll, EntryPoint = "set_cwd",
				CallingConvention = CallingConvention.Cdecl,
				CharSet = CharSet.Ansi)]
		public static extern
			void set_cwdA(IntPtr pServer, string cwd);

		/// <summary>
		/// Set the program name using a Unicode string
		/// </summary>
		/// <remarks>
		/// The encoding should match that set by SetCharacterSet()
		/// </remarks>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <param name="workspace">program name</param>
		[DllImport(bridgeDll, EntryPoint = "set_programName",
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			void set_programNameW(IntPtr pServer, IntPtr workspace);

		/// <summary>
		/// Set the program name using an ASCII string
		/// </summary>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <param name="workspace">program name</param>
		[DllImport(bridgeDll, EntryPoint = "set_programName",
			CallingConvention = CallingConvention.Cdecl,
			CharSet = CharSet.Ansi)]
		public static extern
			void set_programNameA(IntPtr pServer, String workspace);


		/// <summary>
		/// Get the program name used for the connection
		/// </summary>
		/// <remarks>
		/// The encoding should match that set by SetCharacterSet()
		/// </remarks>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <returns>program name</returns>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			IntPtr get_programName(IntPtr pServer);

		/// <summary>
		/// Set the program version using a Unicode string
		/// </summary>
		/// <remarks>
		/// The encoding should match that set by SetCharacterSet()
		/// </remarks>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <param name="programVer">program version</param>
		[DllImport(bridgeDll, EntryPoint = "set_programVer",
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			void set_programVerW(IntPtr pServer, IntPtr programVer);

		/// <summary>
		/// Set the program version using an ASCII string
		/// </summary>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <param name="programVer">program version</param>
		[DllImport(bridgeDll, EntryPoint = "set_programVer",
			CallingConvention = CallingConvention.Cdecl,
			CharSet = CharSet.Ansi)]
		public static extern
			void set_programVerA(IntPtr pServer, String programVer);

		/// <summary>
		/// Get the program version used for the connection
		/// </summary>
		/// <remarks>
		/// The encoding should match that set by SetCharacterSet()
		/// </remarks>
		/// <param name="pServer">P4BridgeServer Handle</param>
		/// <returns>program version</returns>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			IntPtr get_programVer(IntPtr pServer);

		/// <summary>
		/// Get the character set used for the connection.
		/// </summary>
		/// <param name="pServer">Pointer to the P4BridgeServer </param>
		/// <returns>The ANSI string representing the characterset name</returns>
		[DllImport(bridgeDll, EntryPoint = "get_charset",
				CallingConvention = CallingConvention.Cdecl,
				CharSet = CharSet.Ansi)]
		public static extern IntPtr get_charset( IntPtr pServer );

        [DllImport(bridgeDll, EntryPoint = "SetCharacterSet",
            CallingConvention = CallingConvention.Cdecl,
            CharSet = CharSet.Ansi)]
        public static extern void set_charset( IntPtr pServer, string charset, string fileset);

		/// <summary>
		/// Get the config file for the current connection, if any.
		/// </summary>
		/// <param name="pServer">Pointer to the P4BridgeServer </param>
		/// <returns>The config file</returns>
		[DllImport(bridgeDll,
				CallingConvention = CallingConvention.Cdecl)]
		public static extern
			IntPtr get_config( IntPtr pServer );

		/// <summary>
		/// Get the config file for the specified directory, if any.
		/// </summary>
		/// <param name="cwd">Directory</param>
		/// <returns>The config file</returns>
		[DllImport(bridgeDll, EntryPoint = "get_config_cwd",
				CallingConvention = CallingConvention.Cdecl,
				CharSet = CharSet.Ansi)]
		public static extern
			IntPtr get_configA( string cwd );

		[DllImport(bridgeDll, EntryPoint = "get_config_cwd",
				CallingConvention = CallingConvention.Cdecl)]
		public static extern
			IntPtr get_configW(IntPtr pcwd);

		/***********************************************************************
		 * 
		 * Environment Variables
		 * 
		 **********************************************************************/

		[DllImport(bridgeDll, EntryPoint = "Get",
				CallingConvention = CallingConvention.Cdecl,
				CharSet = CharSet.Ansi)]
		public static extern
			IntPtr GetA(string var);

		[DllImport(bridgeDll, EntryPoint = "Get",
				CallingConvention = CallingConvention.Cdecl)]
		public static extern
			IntPtr GetW(IntPtr var);

		[DllImport(bridgeDll, EntryPoint = "Set",
				CallingConvention = CallingConvention.Cdecl,
				CharSet = CharSet.Ansi)]
		public static extern
			void SetA(string var, string val);

		[DllImport(bridgeDll, EntryPoint = "Set",
				CallingConvention = CallingConvention.Cdecl)]
		public static extern
			void SetW(IntPtr var, IntPtr val);

        [DllImport(bridgeDll, EntryPoint = "Update",
                CallingConvention = CallingConvention.Cdecl)]
        public static extern
            void UpdateW(IntPtr var, IntPtr val);

        [DllImport(bridgeDll, EntryPoint = "ReloadEnviro",
               CallingConvention = CallingConvention.Cdecl)]
        public static extern
            void ReloadEnviro();

        [DllImport(bridgeDll, EntryPoint = "GetTicketFile",
                CallingConvention = CallingConvention.Cdecl)]
        public static extern
            IntPtr GetTicketFile();

        [DllImport(bridgeDll, EntryPoint = "GetTicket",
                CallingConvention = CallingConvention.Cdecl)]
        public static extern
            IntPtr GetTicket(IntPtr port, IntPtr user);

		/***********************************************************************
		 * 
		 * IsIgnored
		 * 
		 **********************************************************************/

		[DllImport(bridgeDll, EntryPoint = "IsIgnored",
				CallingConvention = CallingConvention.Cdecl,
				CharSet = CharSet.Ansi)]
		public static extern bool IsIgnoredA(string Path);

		[DllImport(bridgeDll, EntryPoint = "IsIgnored",
				CallingConvention = CallingConvention.Cdecl)]
		public static extern bool IsIgnoredW(IntPtr Path);

		/***********************************************************************
		 * 
		 * StrDictListIterator Functions
		 * 
		 * The StrDictListIterator is used to read tagged data from a command.
		 * It allows the client to step through the 'objects' represented in the
		 * tagged data as StrDict (string dictionaries)
		 * 
		 **********************************************************************/

		/// <summary>
		/// Get the Next item (a StrDict based object)
		/// </summary>
		/// <param name="pObj">StrDictListIterator Handle</param>
		/// <returns>StrDictList Handle, null if end of list</returns>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			IntPtr GetNextItem(IntPtr pObj);

		/// <summary>
		/// Get the next dictionary entry for the current item.
		/// </summary>
		/// <param name="pObj">StrDictListIterator Handle</param>
		/// <returns>Handle to a Key:Value pair</returns>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			IntPtr GetNextEntry(IntPtr pObj);

        /// <summary>
        /// Delete an object allocated on the bridge's heap when it is no 
        /// longer needed
        /// </summary>
        /// <param name="pObj">StrDictListIterator Handle</param>
        [DllImport(bridgeDll,
            CallingConvention = CallingConvention.Cdecl)]
        public static extern
            void Release(IntPtr pObj);

        /// <summary>
        /// Delete an array allocated on the bridge's heap when it is no 
        /// longer needed
        /// </summary>
        /// <param name="pObj">StrDictListIterator Handle</param>
        [DllImport(bridgeDll,
            CallingConvention = CallingConvention.Cdecl)]
        public static extern
            void ReleaseString(IntPtr pObj);

		/***********************************************************************
		 * 
		 * KeyValuePair Functions
		 * 
		 * KeyValuePair objects represent a dictionary entry in s StrDict
		 * 
		 **********************************************************************/

		/// <summary>
		/// Retrieve the key
		/// </summary>
		/// <param name="pObj">KeyValuePair Handle</param>
		/// <returns>Key string</returns>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			IntPtr GetKey(IntPtr pObj);

		/// <summary>
		/// Retrieve the value
		/// </summary>
		/// <param name="pObj">KeyValuePair Handle</param>
		/// <returns>String value</returns>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			IntPtr GetValue(IntPtr pObj);

		/***********************************************************************
		 * 
		 * P4ErrorList Functions
		 * 
		 * P4ErrorList represents the list of errors (and warnings) generated by
		 * the execution of a command.
		 * 
		 **********************************************************************/

		/// <summary>
		/// Severity of the error
		/// </summary>
		/// <param name="pObj">P4ErrorList Handle</param>
		/// <returns>Severity level</returns>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			int Severity(IntPtr pObj);

		/// <summary>
		/// Generic code of the error
		/// </summary>
		/// <param name="pObj">P4ErrorList Handle</param>
		/// <returns>Generic error code</returns>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			int ErrorCode(IntPtr pObj);

		/// <summary>
		/// Get the error's message.
		/// </summary>
		/// <param name="pObj">P4ErrorList Handle</param>
		/// <returns>Error Message</returns>
		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl)]
		public static extern
			IntPtr Message(IntPtr pObj);

		/**********************************************************************
		*
		*  GetNext: Get the next error message.
		*
		*    pObj: Pointer to the P4ClientError. 
		*    
		*  Return: Pointer to the next error message.
		*
		**********************************************************************/

		/// <summary>
		/// Get the next error in the list
		/// </summary>
		/// <param name="pObj">P4ErrorList Handle</param>
		/// <returns>null indicates the end of the list</returns>
		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl)]
		public static extern
			IntPtr Next(IntPtr pObj);

		/***********************************************************************
		 * 
		 * P4ClientInfoMessage Functions
		 * 
		 * P4ClientInfoMessage represents the list of info messages generated by
		 * the execution of a command.
		 * 
		 **********************************************************************/

		/// <summary>
		/// Severity of the error
		/// </summary>
		/// <param name="pObj">P4ClientInfoMessage Handle</param>
		/// <returns>Severity level</returns>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			char MessageLevel(IntPtr pObj);

		/// <summary>
		/// Code of the message
		/// </summary>
		/// <param name="pObj">P4ClientInfoMessage Handle</param>
		/// <returns>Generic error code</returns>
		[DllImport(bridgeDll,
			CallingConvention = CallingConvention.Cdecl)]
		public static extern
			int InfoMsgCode(IntPtr pObj);

		/// <summary>
		/// Get the message text.
		/// </summary>
		/// <param name="pObj">P4ClientInfoMessage Handle</param>
		/// <returns>Error Message</returns>
		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl)]
		public static extern
			IntPtr InfoMessage(IntPtr pObj);

		/// <summary>
		/// Get the next message in the list
		/// </summary>
		/// <param name="pObj">P4ClientInfoMessage Handle</param>
		/// <returns>null indicates the end of the list</returns>
		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl)]
		public static extern
			IntPtr NextInfoMsg(IntPtr pObj);
	}

	/**********************************************************************
	*
	*  P4ClientMerge
	*
	*  This simple class is a ClientMerge object.
	*
	*********************************************************************/
	/// <summary>
	/// Class containing the DLL imports for the P4Bridge DLL.
	/// </summary>
	internal class P4ClientMergeBridge
	{
		const string bridgeDll = "p4bridge";
		static P4ClientMergeBridge()
		{
			Assembly p4apinet = Assembly.GetExecutingAssembly();
			PortableExecutableKinds peKind;
			ImageFileMachine machine;
			p4apinet.ManifestModule.GetPEKind(out peKind, out machine);

			// only set this path if it is Any CPU (ILOnly)
			if (peKind.ToString() == "ILOnly")
			{
				/*
				string currentArchSubPath = "x86";

				// Is this a 64 bits process?
				if (IntPtr.Size == 8)
				{
					currentArchSubPath = "x64";
				}
				SetDllDirectory(currentArchSubPath);
				*/
			}
		}

		//[DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
		//static extern bool SetDllDirectory(string lpPathName);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CM_AutoResolve")]
		public static extern int AutoResolve(IntPtr pObj, int forceMerge);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CM_Resolve")]
		public static extern int Resolve(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CM_DetectResolve")]
		public static extern int DetectResolve(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CM_IsAcceptable")]
		public static extern int IsAcceptable(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CM_GetBaseFile")]
		public static extern IntPtr GetBaseFile(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CM_GetYourFile")]
		public static extern IntPtr GetYourFile(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CM_GetTheirFile")]
		public static extern IntPtr GetTheirFile(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CM_GetResultFile")]
		public static extern IntPtr GetResultFile(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CM_GetYourChunks")]
		public static extern int GetYourChunks(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CM_GetTheirChunks")]
		public static extern int GetTheirChunks(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CM_GetBothChunks")]
		public static extern int GetBothChunks(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CM_GetConflictChunks")]
		public static extern int GetConflictChunks(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CM_GetMergeDigest")]
		public static extern IntPtr GetMergeDigest(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CM_GetYourDigest")]
		public static extern IntPtr GetYourDigest(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CM_GetTheirDigest")]
		public static extern IntPtr GetTheirDigest(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CM_GetLastError")]
		public static extern IntPtr GetLastError(IntPtr pObj);

		/**********************************************************************
		*
		*  SetResolveCallbackFn: Set the callback for replying to a resolve 
		*		callback.
		*
		*    pServer: Pointer to the P4BridgeServer 
		*
		*    pNew: The new callback function pointer
		*    
		*  Return: None
		**********************************************************************/

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl)]
		public static extern void SetResolveCallbackFn(	IntPtr pServer,
														IntPtr pNew);
	}


	/**********************************************************************
	*
	*  P4ClientResolve
	*
	*  This simple class is a ClientResolve object.
	*
	*********************************************************************/
	/// <summary>
	/// Class containing the DLL imports for the P4Bridge DLL.
	/// </summary>
	internal class P4ClientResolveBridge
	{
		const string bridgeDll = "p4bridge";
		static P4ClientResolveBridge()
		{
			Assembly p4apinet = Assembly.GetExecutingAssembly();
			PortableExecutableKinds peKind;
			ImageFileMachine machine;
			p4apinet.ManifestModule.GetPEKind(out peKind, out machine);

			// only set this path if it is Any CPU (ILOnly)
			if (peKind.ToString() == "ILOnly")
			{
				/*
				string currentArchSubPath = "x86";

				// Is this a 64 bits process?
				if (IntPtr.Size == 8)
				{
					currentArchSubPath = "x64";
				}
				SetDllDirectory(currentArchSubPath);
				*/
			}
		}

		//[DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
		//static extern bool SetDllDirectory(string lpPathName);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CR_AutoResolve")]
		public static extern int AutoResolve(IntPtr pObj, int force);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CR_Resolve")]
		public static extern int Resolve(IntPtr pObj, bool preview);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CR_GetType")]
		public static extern IntPtr GetResolveType(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CR_GetMergeAction")]
		public static extern IntPtr GetMergeAction(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CR_GetYoursAction")]
		public static extern IntPtr GetYoursAction(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CR_GetTheirAction")]
		public static extern IntPtr GetTheirAction(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CR_GetMergePrompt")]
		public static extern IntPtr GetMergePrompt(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CR_GetYoursPrompt")]
		public static extern IntPtr GetYoursPrompt(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CR_GetTheirPrompt")]
		public static extern IntPtr GetTheirPrompt(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CR_GetMergeOpt")]
		public static extern IntPtr GetMergeOpt(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CR_GetYoursOpt")]
		public static extern IntPtr GetYoursOpt(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CR_GetTheirOpt")]
		public static extern IntPtr GetTheirOpt(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CR_GetSkipOpt")]
		public static extern IntPtr GetSkipOpt(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CR_GetHelpOpt")]
		public static extern IntPtr GetHelpOpt(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CR_GetAutoOpt")]
		public static extern IntPtr GetAutoOpt(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CR_GetPrompt")]
		public static extern IntPtr GetPrompt(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CR_GetTypePrompt")]
		public static extern IntPtr GetTypePrompt(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CR_GetUsageError")]
		public static extern IntPtr GetUsageError(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CR_GetHelp")]
		public static extern IntPtr GetHelp(IntPtr pObj);

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl, EntryPoint = "CR_GetLastError")]
		public static extern IntPtr GetLastError(IntPtr pObj);

		/**********************************************************************
		*
		*  SetResolveCallbackFn: Set the callback for replying to a resolve A 
		*		callback.
		*
		*    pServer: Pointer to the P4BridgeServer 
		*
		*    pNew: The new callback function pointer
		*    
		*  Return: None
		**********************************************************************/

		[DllImport(bridgeDll, CallingConvention = CallingConvention.Cdecl)]
		public static extern void SetResolveACallbackFn(IntPtr pServer, 
														IntPtr pNew);
    }
}
