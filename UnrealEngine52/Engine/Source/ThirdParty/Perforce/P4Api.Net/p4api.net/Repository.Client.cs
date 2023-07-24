/*******************************************************************************

Copyright (c) 2011, Perforce Software, Inc.  All rights reserved.

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
 * Name		: Repository.Client.cs
 *
 * Author	: dbb
 *
 * Description	: Class used to abstract a client in Perforce.
 *
 ******************************************************************************/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	public partial class Repository
	{
		/// <summary>
		/// Create a new client in the repository.
		/// </summary>
		/// <param name="client">Client specification for the new user</param>
		/// <param name="options">The '-i' flags is required when creating a new user</param>
		/// <returns>The Client object if new user was created, null if creation failed</returns>
		/// <remarks> The '-i' flag is added if not specified by the caller	
		/// <br/> 
		/// <br/><b>p4 help client</b>
		/// <br/> 
		/// <br/>     client -- Create or edit a client workspace specification and its view
		/// <br/>     workspace -- Synonym for 'client'
		/// <br/> 
		/// <br/>     p4 client [-f] [-t template] [name]
		/// <br/>     p4 client -d [-f [-Fs]] name
		/// <br/>     p4 client -o [-t template] [name]
		/// <br/>     p4 client -S stream [[-c change] -o] [name]
		/// <br/>     p4 client -s [-f] -S stream [name]
		/// <br/>     p4 client -s [-f] -t template [name]
		/// <br/>     p4 client -i [-f]
		/// <br/> 
		/// <br/> 	Creates a new client specification ('spec') or edits an existing
		/// <br/> 	spec.  A client spec is a named mapping of depot files to workspace
		/// <br/> 	files.
		/// <br/> 
		/// <br/> 	The 'p4 client' command puts the client spec into a temporary file
		/// <br/> 	and invokes the editor configured by the environment variable
		/// <br/> 	$P4EDITOR.  For new workspaces, the client name defaults to the
		/// <br/> 	$P4CLIENT environment variable, if set, or to the current host name.
		/// <br/> 	Saving the file creates or modifies the client spec.
		/// <br/> 
		/// <br/> 	The client spec contains the following fields:
		/// <br/> 
		/// <br/> 	Client:      The client name.
		/// <br/> 
		/// <br/> 	Host:        If set, restricts access to the named host.
		/// <br/> 		     If unset, access is allowed from any host.
		/// <br/> 
		/// <br/> 	Owner:       The user who created this client.
		/// <br/> 
		/// <br/> 	Update:      The date that this spec was last modified.
		/// <br/> 
		/// <br/> 	Access:      The date that this client was last used in any way.
		/// <br/> 
		/// <br/> 	Description: A short description of the workspace.
		/// <br/> 
		/// <br/> 	Root:        The root directory of the workspace (specified in local
		/// <br/> 		     file system syntax), under which all versioned files
		/// <br/> 		     will be placed. If you change this setting, you must
		/// <br/> 		     physically relocate any files that currently reside
		/// <br/> 		     there.  On Windows client machines, you can specify the
		/// <br/> 		     root as "null" to enable you to map files to multiple
		/// <br/> 		     drives.
		/// <br/> 
		/// <br/> 	AltRoots:    Up to two optional alternate client workspace roots.
		/// <br/> 		     The first of the main and alternate roots to match the
		/// <br/> 		     client program's current working directory is used. If
		/// <br/> 		     none match, the main root is used. 'p4 info' displays
		/// <br/> 		     the root that is being used.
		/// <br/> 
		/// <br/> 	Options:     Flags to configure the client behavior. Defaults
		/// <br/> 		     are marked with *.
		/// <br/> 
		/// <br/> 		allwrite	Leaves all files writable on the client;
		/// <br/> 		noallwrite *	by default, only files opened by 'p4 edit'
		/// <br/> 				are writable. If set, files might be clobbered
		/// <br/> 				as a result of ignoring the clobber option
		/// <br/> 				(see below).
		/// <br/> 
		/// <br/> 		clobber		Permits 'p4 sync' to overwrite writable
		/// <br/> 		noclobber *	files on the client.  noclobber is ignored if
		/// <br/> 				allwrite is set.
		/// <br/> 
		/// <br/> 		compress 	Compresses data sent between the client
		/// <br/> 		nocompress *	and server to speed up slow connections.
		/// <br/> 
		/// <br/> 		locked   	Allows only the client owner to use or change
		/// <br/> 		unlocked *	the client spec.  Prevents the client spec from
		/// <br/> 				being deleted or unloaded.
		/// <br/> 
		/// <br/> 		modtime  	Causes 'p4 sync' and 'p4 submit' to preserve
		/// <br/> 		nomodtime *	file modification time, as with files with the
		/// <br/> 				+m type modifier. (See 'p4 help filetypes'.)
		/// <br/> 				With nomodtime, file timestamps are updated by
		/// <br/> 				sync and submit operations.
		/// <br/> 
		/// <br/> 		rmdir		Makes 'p4 sync' attempt to delete a workspace
		/// <br/> 		normdir *	directory when all files in it are removed.
		/// <br/> 
		/// <br/> 	SubmitOptions:  Flags to change submit behavior.
		/// <br/> 
		/// <br/> 		submitunchanged     All open files are submitted (default).
		/// <br/> 
		/// <br/> 		revertunchanged     Files that have content, type, or resolved
		/// <br/> 		                    changes are submitted. Unchanged files are
		/// <br/> 		                    reverted.
		/// <br/> 
		/// <br/> 		leaveunchanged      Files that have content, type, or resolved
		/// <br/> 		                    changes are submitted. Unchanged files are
		/// <br/> 		                    moved to the default changelist.
		/// <br/> 
		/// <br/> 		        +reopen     Can be appended to the submit option flag
		/// <br/> 		                    to cause submitted files to be reopened in
		/// <br/> 		                    the default changelist.
		/// <br/> 		                    Example: submitunchanged+reopen
		/// <br/> 
		/// <br/> 	LineEnd:    Set line-ending character(s) for client text files.
		/// <br/> 
		/// <br/> 		local		mode that is native to the client (default).
		/// <br/> 		unix		linefeed: UNIX style.
		/// <br/> 		mac		carriage return: Macintosh style.
		/// <br/> 		win		carriage return-linefeed: Windows style.
		/// <br/> 		share		hybrid: writes UNIX style but reads UNIX,
		/// <br/> 				Mac or Windows style.
		/// <br/> 
		/// <br/> 	View:        Maps files in the depot to files in your client
		/// <br/> 		     workspace.  Defines the files that you want in your
		/// <br/> 		     client workspace and specifies where you want them
		/// <br/> 		     to reside.  The default view maps all depot files
		/// <br/> 		     onto the client.  See 'p4 help views' for view syntax.
		/// <br/> 		     A new view takes effect on the next 'p4 sync'.
		/// <br/> 
		/// <br/> 	ChangeView:  Restricts depot paths to a particular point in time.
		/// <br/> 	             Example: //depot/path/...@1000
		/// <br/> 	             Revisions submitted to the path after the specified
		/// <br/> 	             changelist will not be visible.  Files matching a
		/// <br/> 	             ChangeView path may not be submitted.
		/// <br/> 
		/// <br/> 	Stream:      The stream to which this client's view will be dedicated.
		/// <br/> 		     (Files in stream paths can be submitted only by dedicated
		/// <br/> 		     stream clients.) When this optional field is set, the
		/// <br/> 		     View field will be automatically replaced by a stream
		/// <br/> 		     view as the client spec is saved.
		/// <br/> 
		/// <br/> 	StreamAtChange:
		/// <br/> 		     Stream meta-data (for view creation) is versioned.
		/// <br/> 		     When StreamAtChange is set, the stream hierarchy is
		/// <br/> 		     constructed back-in-time to reproduce the view that was
		/// <br/> 		     available to clients in the past. With this field set
		/// <br/> 		     changes cannot be be submitted from the client.
		/// <br/> 		     StreamAtChange is optional; Stream must be set as well.
		/// <br/> 
		/// <br/> 	ServerID:    If set, restricts usage to the named server.
		/// <br/> 		     If unset, usage is allowed on the master server and on
		/// <br/> 		     any replicas of the master other than Edge Servers.
		/// <br/> 
		/// <br/> 	Note: changing the client root does not actually move the client
		/// <br/> 	files; you must relocate them manually.  Similarly, changing
		/// <br/> 	the 'LineEnd' option does not actually update the client files;
		/// <br/> 	you can refresh them with 'p4 sync -f'.
		/// <br/> 
		/// <br/> 	The -d flag deletes the specified spec, as long as the client
		/// <br/> 	workspace has no opened files or pending changes.  (See 'p4 help
		/// <br/> 	opened'.) The -f flag forces the delete. The -Fs option used with -f
		/// <br/> 	forces client deletion even when the user has shelved changes. In
		/// <br/> 	this case, the shelved changes are left intact.
		/// <br/> 
		/// <br/> 	The -o flag writes the named client spec to the standard output.
		/// <br/> 	The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a client spec from the standard input.  The
		/// <br/> 	user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -t template flag, where 'template' is the name of another client
		/// <br/> 	spec, causes the View and Options fields to be replaced by those of
		/// <br/> 	the template.
		/// <br/> 
		/// <br/> 	The -f flag can force the updating of locked clients; normally
		/// <br/> 	locked clients can only be modified by their owner.  -f also allows
		/// <br/> 	the last modified date to be set.  The -f flag requires 'admin'
		/// <br/> 	access granted by 'p4 protect'.
		/// <br/> 
		/// <br/> 	The -s flag is used to switch an existing client spec's view without
		/// <br/> 	invoking the editor.  It can be used with -S to switch to a stream
		/// <br/> 	view, or with -t to switch to a view defined in another client spec.
		/// <br/> 	Switching views is not allowed in a client that has opened files.
		/// <br/> 	The -f flag can be used with -s to force switching with opened files.
		/// <br/> 	View switching has no effect on files in a client workspace until
		/// <br/> 	'p4 sync' is run.
		/// <br/> 
		/// <br/> 	Without -s, the '-S stream' flag can be used to create a new client
		/// <br/> 	spec dedicated to a stream. If the client spec already exists, and
		/// <br/> 	-S is used without -s, an error occurs when the client is bound to
		/// <br/> 	a different, or no stream.  Using -S sets the new client's
		/// <br/> 	Stream field.  The special syntax '-S //a/stream@changelist' can
		/// <br/> 	be used to set both Stream and StreamAtChange at the same time.
		/// <br/> 
		/// <br/> 	The '-S stream' flag can be used with '-o -c change' to inspect an
		/// <br/> 	old stream client view. It yields the client spec that would have
		/// <br/> 	been created for the stream at the moment the change was recorded.
		/// <br/> 
		/// <br/> 	A server of type 'build-server' (see 'p4 help server') is a replica
		/// <br/> 	which supports build farm integration, and the 'p4 client' command
		/// <br/> 	may be used to create or edit client workspaces on a build-server.
		/// <br/> 	Such workspaces may issue the 'p4 sync' command, in addition to any
		/// <br/> 	read-only command supported by the replica. For more information, run
		/// <br/> 	'p4 help buildserver'.
		/// <br/> 
		/// <br/> 	When creating or editing a client workspace for a build-server,
		/// <br/> 	the client specified by the optional 'name' argument, as well as the
		/// <br/> 	client specified by the $P4CLIENT environment variable or via the
		/// <br/> 	global '-c client' argument must not exist, or must be restricted
		/// <br/> 	to this server; this command may not be used to create or edit a
		/// <br/> 	workspace that is not restricted to this build-server.
		/// <br/> 
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To create a client named main_ws:
        ///		<code> 
        ///		    Client c = Repository.GetClient("main_ws");
        ///		    c.OwnerName= "bsmith";
        ///		    c.Description = "client for main dev";
        ///		    Repository.CreateClient(c, null);
        ///		</code>
        ///		To create a client named main_ws using existing admin_main_ws
        ///		as a template:
        ///		<code> 
        ///		    ClientCmdOptions opts = 
        ///		    new ClientCmdOptions(ClientCmdFlags.None, "admin_main_ws", null, 0);
        ///		    Client c = Repository.GetClient("main_ws",opts);
        ///		    Repository.CreateClient(c, null);
        ///		</code>
        ///		To create a streams client named main_ws for the mainline stream
        ///		//MobileApp/Main:
        ///		<code> 
        ///		    ClientCmdOptions opts = 
        ///		    new ClientCmdOptions(ClientCmdFlags.None, null, "//MobileApp/Main", 0);
        ///		    Client c = Repository.GetClient("main_ws",opts);
        ///		    Repository.CreateClient(c, null);
        ///		</code>
        /// </example>
        /// <seealso cref="ClientCmdFlags"/> 
        public Client CreateClient(Client client, Options options)
		{
			if (client == null)
			{
				throw new ArgumentNullException("client");

			}
			P4Command cmd = new P4Command(this, "client", true);

			cmd.DataSet = client.ToString();

			if (options == null)
			{
				options = new Options();
			}
			
				options["-i"] = null;

			P4CommandResult results = cmd.Run(options);
			if (results.Success)
			{
				return client;
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}
			return null;
		}

        /// <summary>
        /// Create a new client in the repository.
        /// </summary>
        /// <param name="client">Client specification for the new user</param>
        /// <returns>The Client object if new user was created, null if creation failed</returns>
        /// <remarks> The '-i' flag is added if not specified by the caller	
        /// <br/> 
		/// <br/><b>p4 help client</b>
		/// <br/> 
		/// <br/>     client -- Create or edit a client workspace specification and its view
		/// <br/>     workspace -- Synonym for 'client'
		/// <br/> 
		/// <br/>     p4 client [-f] [-t template] [name]
		/// <br/>     p4 client -d [-f [-Fs]] name
		/// <br/>     p4 client -o [-t template] [name]
		/// <br/>     p4 client -S stream [[-c change] -o] [name]
		/// <br/>     p4 client -s [-f] -S stream [name]
		/// <br/>     p4 client -s [-f] -t template [name]
		/// <br/>     p4 client -i [-f]
		/// <br/> 
		/// <br/> 	Creates a new client specification ('spec') or edits an existing
		/// <br/> 	spec.  A client spec is a named mapping of depot files to workspace
		/// <br/> 	files.
		/// <br/> 
		/// <br/> 	The 'p4 client' command puts the client spec into a temporary file
		/// <br/> 	and invokes the editor configured by the environment variable
		/// <br/> 	$P4EDITOR.  For new workspaces, the client name defaults to the
		/// <br/> 	$P4CLIENT environment variable, if set, or to the current host name.
		/// <br/> 	Saving the file creates or modifies the client spec.
		/// <br/> 
		/// <br/> 	The client spec contains the following fields:
		/// <br/> 
		/// <br/> 	Client:      The client name.
		/// <br/> 
		/// <br/> 	Host:        If set, restricts access to the named host.
		/// <br/> 		     If unset, access is allowed from any host.
		/// <br/> 
		/// <br/> 	Owner:       The user who created this client.
		/// <br/> 
		/// <br/> 	Update:      The date that this spec was last modified.
		/// <br/> 
		/// <br/> 	Access:      The date that this client was last used in any way.
		/// <br/> 
		/// <br/> 	Description: A short description of the workspace.
		/// <br/> 
		/// <br/> 	Root:        The root directory of the workspace (specified in local
		/// <br/> 		     file system syntax), under which all versioned files
		/// <br/> 		     will be placed. If you change this setting, you must
		/// <br/> 		     physically relocate any files that currently reside
		/// <br/> 		     there.  On Windows client machines, you can specify the
		/// <br/> 		     root as "null" to enable you to map files to multiple
		/// <br/> 		     drives.
		/// <br/> 
		/// <br/> 	AltRoots:    Up to two optional alternate client workspace roots.
		/// <br/> 		     The first of the main and alternate roots to match the
		/// <br/> 		     client program's current working directory is used. If
		/// <br/> 		     none match, the main root is used. 'p4 info' displays
		/// <br/> 		     the root that is being used.
		/// <br/> 
		/// <br/> 	Options:     Flags to configure the client behavior. Defaults
		/// <br/> 		     are marked with *.
		/// <br/> 
		/// <br/> 		allwrite	Leaves all files writable on the client;
		/// <br/> 		noallwrite *	by default, only files opened by 'p4 edit'
		/// <br/> 				are writable. If set, files might be clobbered
		/// <br/> 				as a result of ignoring the clobber option
		/// <br/> 				(see below).
		/// <br/> 
		/// <br/> 		clobber		Permits 'p4 sync' to overwrite writable
		/// <br/> 		noclobber *	files on the client.  noclobber is ignored if
		/// <br/> 				allwrite is set.
		/// <br/> 
		/// <br/> 		compress 	Compresses data sent between the client
		/// <br/> 		nocompress *	and server to speed up slow connections.
		/// <br/> 
		/// <br/> 		locked   	Allows only the client owner to use or change
		/// <br/> 		unlocked *	the client spec.  Prevents the client spec from
		/// <br/> 				being deleted or unloaded.
		/// <br/> 
		/// <br/> 		modtime  	Causes 'p4 sync' and 'p4 submit' to preserve
		/// <br/> 		nomodtime *	file modification time, as with files with the
		/// <br/> 				+m type modifier. (See 'p4 help filetypes'.)
		/// <br/> 				With nomodtime, file timestamps are updated by
		/// <br/> 				sync and submit operations.
		/// <br/> 
		/// <br/> 		rmdir		Makes 'p4 sync' attempt to delete a workspace
		/// <br/> 		normdir *	directory when all files in it are removed.
		/// <br/> 
		/// <br/> 	SubmitOptions:  Flags to change submit behavior.
		/// <br/> 
		/// <br/> 		submitunchanged     All open files are submitted (default).
		/// <br/> 
		/// <br/> 		revertunchanged     Files that have content, type, or resolved
		/// <br/> 		                    changes are submitted. Unchanged files are
		/// <br/> 		                    reverted.
		/// <br/> 
		/// <br/> 		leaveunchanged      Files that have content, type, or resolved
		/// <br/> 		                    changes are submitted. Unchanged files are
		/// <br/> 		                    moved to the default changelist.
		/// <br/> 
		/// <br/> 		        +reopen     Can be appended to the submit option flag
		/// <br/> 		                    to cause submitted files to be reopened in
		/// <br/> 		                    the default changelist.
		/// <br/> 		                    Example: submitunchanged+reopen
		/// <br/> 
		/// <br/> 	LineEnd:    Set line-ending character(s) for client text files.
		/// <br/> 
		/// <br/> 		local		mode that is native to the client (default).
		/// <br/> 		unix		linefeed: UNIX style.
		/// <br/> 		mac		carriage return: Macintosh style.
		/// <br/> 		win		carriage return-linefeed: Windows style.
		/// <br/> 		share		hybrid: writes UNIX style but reads UNIX,
		/// <br/> 				Mac or Windows style.
		/// <br/> 
		/// <br/> 	View:        Maps files in the depot to files in your client
		/// <br/> 		     workspace.  Defines the files that you want in your
		/// <br/> 		     client workspace and specifies where you want them
		/// <br/> 		     to reside.  The default view maps all depot files
		/// <br/> 		     onto the client.  See 'p4 help views' for view syntax.
		/// <br/> 		     A new view takes effect on the next 'p4 sync'.
		/// <br/> 
		/// <br/> 	ChangeView:  Restricts depot paths to a particular point in time.
		/// <br/> 	             Example: //depot/path/...@1000
		/// <br/> 	             Revisions submitted to the path after the specified
		/// <br/> 	             changelist will not be visible.  Files matching a
		/// <br/> 	             ChangeView path may not be submitted.
		/// <br/> 
		/// <br/> 	Stream:      The stream to which this client's view will be dedicated.
		/// <br/> 		     (Files in stream paths can be submitted only by dedicated
		/// <br/> 		     stream clients.) When this optional field is set, the
		/// <br/> 		     View field will be automatically replaced by a stream
		/// <br/> 		     view as the client spec is saved.
		/// <br/> 
		/// <br/> 	StreamAtChange:
		/// <br/> 		     Stream meta-data (for view creation) is versioned.
		/// <br/> 		     When StreamAtChange is set, the stream hierarchy is
		/// <br/> 		     constructed back-in-time to reproduce the view that was
		/// <br/> 		     available to clients in the past. With this field set
		/// <br/> 		     changes cannot be be submitted from the client.
		/// <br/> 		     StreamAtChange is optional; Stream must be set as well.
		/// <br/> 
		/// <br/> 	ServerID:    If set, restricts usage to the named server.
		/// <br/> 		     If unset, usage is allowed on the master server and on
		/// <br/> 		     any replicas of the master other than Edge Servers.
		/// <br/> 
		/// <br/> 	Note: changing the client root does not actually move the client
		/// <br/> 	files; you must relocate them manually.  Similarly, changing
		/// <br/> 	the 'LineEnd' option does not actually update the client files;
		/// <br/> 	you can refresh them with 'p4 sync -f'.
		/// <br/> 
		/// <br/> 	The -d flag deletes the specified spec, as long as the client
		/// <br/> 	workspace has no opened files or pending changes.  (See 'p4 help
		/// <br/> 	opened'.) The -f flag forces the delete. The -Fs option used with -f
		/// <br/> 	forces client deletion even when the user has shelved changes. In
		/// <br/> 	this case, the shelved changes are left intact.
		/// <br/> 
		/// <br/> 	The -o flag writes the named client spec to the standard output.
		/// <br/> 	The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a client spec from the standard input.  The
		/// <br/> 	user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -t template flag, where 'template' is the name of another client
		/// <br/> 	spec, causes the View and Options fields to be replaced by those of
		/// <br/> 	the template.
		/// <br/> 
		/// <br/> 	The -f flag can force the updating of locked clients; normally
		/// <br/> 	locked clients can only be modified by their owner.  -f also allows
		/// <br/> 	the last modified date to be set.  The -f flag requires 'admin'
		/// <br/> 	access granted by 'p4 protect'.
		/// <br/> 
		/// <br/> 	The -s flag is used to switch an existing client spec's view without
		/// <br/> 	invoking the editor.  It can be used with -S to switch to a stream
		/// <br/> 	view, or with -t to switch to a view defined in another client spec.
		/// <br/> 	Switching views is not allowed in a client that has opened files.
		/// <br/> 	The -f flag can be used with -s to force switching with opened files.
		/// <br/> 	View switching has no effect on files in a client workspace until
		/// <br/> 	'p4 sync' is run.
		/// <br/> 
		/// <br/> 	Without -s, the '-S stream' flag can be used to create a new client
		/// <br/> 	spec dedicated to a stream. If the client spec already exists, and
		/// <br/> 	-S is used without -s, an error occurs when the client is bound to
		/// <br/> 	a different, or no stream.  Using -S sets the new client's
		/// <br/> 	Stream field.  The special syntax '-S //a/stream@changelist' can
		/// <br/> 	be used to set both Stream and StreamAtChange at the same time.
		/// <br/> 
		/// <br/> 	The '-S stream' flag can be used with '-o -c change' to inspect an
		/// <br/> 	old stream client view. It yields the client spec that would have
		/// <br/> 	been created for the stream at the moment the change was recorded.
		/// <br/> 
		/// <br/> 	A server of type 'build-server' (see 'p4 help server') is a replica
		/// <br/> 	which supports build farm integration, and the 'p4 client' command
		/// <br/> 	may be used to create or edit client workspaces on a build-server.
		/// <br/> 	Such workspaces may issue the 'p4 sync' command, in addition to any
		/// <br/> 	read-only command supported by the replica. For more information, run
		/// <br/> 	'p4 help buildserver'.
		/// <br/> 
		/// <br/> 	When creating or editing a client workspace for a build-server,
		/// <br/> 	the client specified by the optional 'name' argument, as well as the
		/// <br/> 	client specified by the $P4CLIENT environment variable or via the
		/// <br/> 	global '-c client' argument must not exist, or must be restricted
		/// <br/> 	to this server; this command may not be used to create or edit a
		/// <br/> 	workspace that is not restricted to this build-server.
		/// <br/> 
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To create a client named main_ws:
        ///		<code> 
        ///		    Client c = Repository.GetClient("main_ws");
        ///		    c.OwnerName= "bsmith";
        ///		    c.Description = "client for main dev";
        ///		    Repository.CreateClient(c);
        ///		</code>
        ///		To create a client named main_ws using existing admin_main_ws
        ///		as a template:
        ///		<code> 
        ///		    ClientCmdOptions opts = 
        ///		    new ClientCmdOptions(ClientCmdFlags.None, "admin_main_ws", null, 0);
        ///		    Client c = Repository.GetClient("main_ws",opts);
        ///		    Repository.CreateClient(c);
        ///		</code>
        ///		To create a streams client named main_ws for the mainline stream
        ///		//MobileApp/Main:
        ///		<code> 
        ///		    ClientCmdOptions opts = 
        ///		    new ClientCmdOptions(ClientCmdFlags.None, null, "//MobileApp/Main", 0);
        ///		    Client c = Repository.GetClient("main_ws",opts);
        ///		    Repository.CreateClient(c);
        ///		</code>
        ///	</example>
        public Client CreateClient(Client client)
		{
			return CreateClient(client, null);
		}

		/// <summary>
		/// Update the record for a client in the repository
		/// </summary>
		/// <param name="client">Client specification for the client being updated</param>
		/// <returns>The Client object if new client was saved, null if creation failed</returns>
        /// <remarks> The '-i' flag is added if not specified by the caller	
        /// <br/> 
		/// <br/><b>p4 help client</b>
		/// <br/> 
		/// <br/>     client -- Create or edit a client workspace specification and its view
		/// <br/>     workspace -- Synonym for 'client'
		/// <br/> 
		/// <br/>     p4 client [-f] [-t template] [name]
		/// <br/>     p4 client -d [-f [-Fs]] name
		/// <br/>     p4 client -o [-t template] [name]
		/// <br/>     p4 client -S stream [[-c change] -o] [name]
		/// <br/>     p4 client -s [-f] -S stream [name]
		/// <br/>     p4 client -s [-f] -t template [name]
		/// <br/>     p4 client -i [-f]
		/// <br/> 
		/// <br/> 	Creates a new client specification ('spec') or edits an existing
		/// <br/> 	spec.  A client spec is a named mapping of depot files to workspace
		/// <br/> 	files.
		/// <br/> 
		/// <br/> 	The 'p4 client' command puts the client spec into a temporary file
		/// <br/> 	and invokes the editor configured by the environment variable
		/// <br/> 	$P4EDITOR.  For new workspaces, the client name defaults to the
		/// <br/> 	$P4CLIENT environment variable, if set, or to the current host name.
		/// <br/> 	Saving the file creates or modifies the client spec.
		/// <br/> 
		/// <br/> 	The client spec contains the following fields:
		/// <br/> 
		/// <br/> 	Client:      The client name.
		/// <br/> 
		/// <br/> 	Host:        If set, restricts access to the named host.
		/// <br/> 		     If unset, access is allowed from any host.
		/// <br/> 
		/// <br/> 	Owner:       The user who created this client.
		/// <br/> 
		/// <br/> 	Update:      The date that this spec was last modified.
		/// <br/> 
		/// <br/> 	Access:      The date that this client was last used in any way.
		/// <br/> 
		/// <br/> 	Description: A short description of the workspace.
		/// <br/> 
		/// <br/> 	Root:        The root directory of the workspace (specified in local
		/// <br/> 		     file system syntax), under which all versioned files
		/// <br/> 		     will be placed. If you change this setting, you must
		/// <br/> 		     physically relocate any files that currently reside
		/// <br/> 		     there.  On Windows client machines, you can specify the
		/// <br/> 		     root as "null" to enable you to map files to multiple
		/// <br/> 		     drives.
		/// <br/> 
		/// <br/> 	AltRoots:    Up to two optional alternate client workspace roots.
		/// <br/> 		     The first of the main and alternate roots to match the
		/// <br/> 		     client program's current working directory is used. If
		/// <br/> 		     none match, the main root is used. 'p4 info' displays
		/// <br/> 		     the root that is being used.
		/// <br/> 
		/// <br/> 	Options:     Flags to configure the client behavior. Defaults
		/// <br/> 		     are marked with *.
		/// <br/> 
		/// <br/> 		allwrite	Leaves all files writable on the client;
		/// <br/> 		noallwrite *	by default, only files opened by 'p4 edit'
		/// <br/> 				are writable. If set, files might be clobbered
		/// <br/> 				as a result of ignoring the clobber option
		/// <br/> 				(see below).
		/// <br/> 
		/// <br/> 		clobber		Permits 'p4 sync' to overwrite writable
		/// <br/> 		noclobber *	files on the client.  noclobber is ignored if
		/// <br/> 				allwrite is set.
		/// <br/> 
		/// <br/> 		compress 	Compresses data sent between the client
		/// <br/> 		nocompress *	and server to speed up slow connections.
		/// <br/> 
		/// <br/> 		locked   	Allows only the client owner to use or change
		/// <br/> 		unlocked *	the client spec.  Prevents the client spec from
		/// <br/> 				being deleted or unloaded.
		/// <br/> 
		/// <br/> 		modtime  	Causes 'p4 sync' and 'p4 submit' to preserve
		/// <br/> 		nomodtime *	file modification time, as with files with the
		/// <br/> 				+m type modifier. (See 'p4 help filetypes'.)
		/// <br/> 				With nomodtime, file timestamps are updated by
		/// <br/> 				sync and submit operations.
		/// <br/> 
		/// <br/> 		rmdir		Makes 'p4 sync' attempt to delete a workspace
		/// <br/> 		normdir *	directory when all files in it are removed.
		/// <br/> 
		/// <br/> 	SubmitOptions:  Flags to change submit behavior.
		/// <br/> 
		/// <br/> 		submitunchanged     All open files are submitted (default).
		/// <br/> 
		/// <br/> 		revertunchanged     Files that have content, type, or resolved
		/// <br/> 		                    changes are submitted. Unchanged files are
		/// <br/> 		                    reverted.
		/// <br/> 
		/// <br/> 		leaveunchanged      Files that have content, type, or resolved
		/// <br/> 		                    changes are submitted. Unchanged files are
		/// <br/> 		                    moved to the default changelist.
		/// <br/> 
		/// <br/> 		        +reopen     Can be appended to the submit option flag
		/// <br/> 		                    to cause submitted files to be reopened in
		/// <br/> 		                    the default changelist.
		/// <br/> 		                    Example: submitunchanged+reopen
		/// <br/> 
		/// <br/> 	LineEnd:    Set line-ending character(s) for client text files.
		/// <br/> 
		/// <br/> 		local		mode that is native to the client (default).
		/// <br/> 		unix		linefeed: UNIX style.
		/// <br/> 		mac		carriage return: Macintosh style.
		/// <br/> 		win		carriage return-linefeed: Windows style.
		/// <br/> 		share		hybrid: writes UNIX style but reads UNIX,
		/// <br/> 				Mac or Windows style.
		/// <br/> 
		/// <br/> 	View:        Maps files in the depot to files in your client
		/// <br/> 		     workspace.  Defines the files that you want in your
		/// <br/> 		     client workspace and specifies where you want them
		/// <br/> 		     to reside.  The default view maps all depot files
		/// <br/> 		     onto the client.  See 'p4 help views' for view syntax.
		/// <br/> 		     A new view takes effect on the next 'p4 sync'.
		/// <br/> 
		/// <br/> 	ChangeView:  Restricts depot paths to a particular point in time.
		/// <br/> 	             Example: //depot/path/...@1000
		/// <br/> 	             Revisions submitted to the path after the specified
		/// <br/> 	             changelist will not be visible.  Files matching a
		/// <br/> 	             ChangeView path may not be submitted.
		/// <br/> 
		/// <br/> 	Stream:      The stream to which this client's view will be dedicated.
		/// <br/> 		     (Files in stream paths can be submitted only by dedicated
		/// <br/> 		     stream clients.) When this optional field is set, the
		/// <br/> 		     View field will be automatically replaced by a stream
		/// <br/> 		     view as the client spec is saved.
		/// <br/> 
		/// <br/> 	StreamAtChange:
		/// <br/> 		     Stream meta-data (for view creation) is versioned.
		/// <br/> 		     When StreamAtChange is set, the stream hierarchy is
		/// <br/> 		     constructed back-in-time to reproduce the view that was
		/// <br/> 		     available to clients in the past. With this field set
		/// <br/> 		     changes cannot be be submitted from the client.
		/// <br/> 		     StreamAtChange is optional; Stream must be set as well.
		/// <br/> 
		/// <br/> 	ServerID:    If set, restricts usage to the named server.
		/// <br/> 		     If unset, usage is allowed on the master server and on
		/// <br/> 		     any replicas of the master other than Edge Servers.
		/// <br/> 
		/// <br/> 	Note: changing the client root does not actually move the client
		/// <br/> 	files; you must relocate them manually.  Similarly, changing
		/// <br/> 	the 'LineEnd' option does not actually update the client files;
		/// <br/> 	you can refresh them with 'p4 sync -f'.
		/// <br/> 
		/// <br/> 	The -d flag deletes the specified spec, as long as the client
		/// <br/> 	workspace has no opened files or pending changes.  (See 'p4 help
		/// <br/> 	opened'.) The -f flag forces the delete. The -Fs option used with -f
		/// <br/> 	forces client deletion even when the user has shelved changes. In
		/// <br/> 	this case, the shelved changes are left intact.
		/// <br/> 
		/// <br/> 	The -o flag writes the named client spec to the standard output.
		/// <br/> 	The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a client spec from the standard input.  The
		/// <br/> 	user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -t template flag, where 'template' is the name of another client
		/// <br/> 	spec, causes the View and Options fields to be replaced by those of
		/// <br/> 	the template.
		/// <br/> 
		/// <br/> 	The -f flag can force the updating of locked clients; normally
		/// <br/> 	locked clients can only be modified by their owner.  -f also allows
		/// <br/> 	the last modified date to be set.  The -f flag requires 'admin'
		/// <br/> 	access granted by 'p4 protect'.
		/// <br/> 
		/// <br/> 	The -s flag is used to switch an existing client spec's view without
		/// <br/> 	invoking the editor.  It can be used with -S to switch to a stream
		/// <br/> 	view, or with -t to switch to a view defined in another client spec.
		/// <br/> 	Switching views is not allowed in a client that has opened files.
		/// <br/> 	The -f flag can be used with -s to force switching with opened files.
		/// <br/> 	View switching has no effect on files in a client workspace until
		/// <br/> 	'p4 sync' is run.
		/// <br/> 
		/// <br/> 	Without -s, the '-S stream' flag can be used to create a new client
		/// <br/> 	spec dedicated to a stream. If the client spec already exists, and
		/// <br/> 	-S is used without -s, an error occurs when the client is bound to
		/// <br/> 	a different, or no stream.  Using -S sets the new client's
		/// <br/> 	Stream field.  The special syntax '-S //a/stream@changelist' can
		/// <br/> 	be used to set both Stream and StreamAtChange at the same time.
		/// <br/> 
		/// <br/> 	The '-S stream' flag can be used with '-o -c change' to inspect an
		/// <br/> 	old stream client view. It yields the client spec that would have
		/// <br/> 	been created for the stream at the moment the change was recorded.
		/// <br/> 
		/// <br/> 	A server of type 'build-server' (see 'p4 help server') is a replica
		/// <br/> 	which supports build farm integration, and the 'p4 client' command
		/// <br/> 	may be used to create or edit client workspaces on a build-server.
		/// <br/> 	Such workspaces may issue the 'p4 sync' command, in addition to any
		/// <br/> 	read-only command supported by the replica. For more information, run
		/// <br/> 	'p4 help buildserver'.
		/// <br/> 
		/// <br/> 	When creating or editing a client workspace for a build-server,
		/// <br/> 	the client specified by the optional 'name' argument, as well as the
		/// <br/> 	client specified by the $P4CLIENT environment variable or via the
		/// <br/> 	global '-c client' argument must not exist, or must be restricted
		/// <br/> 	to this server; this command may not be used to create or edit a
		/// <br/> 	workspace that is not restricted to this build-server.
		/// <br/> 
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To update a client named main_ws:
        ///		<code> 
        ///		    Client c = Repository.GetClient("main_ws");
        ///
        ///         // change the description add add a MapEntry to the ViewMap
        ///		    c.Description = "client for main dev. Adding tools path";
        ///		    
        ///		    DepotPath left = new DepotPath("//depot/tools/...");
        ///		    ClientPath right = new ClientPath("//main_ws/tools/...");
        ///         
        ///		    MapEntry line = new MapEntry(MapType.Include, left, right);
        ///		    c.ViewMap.Add(line);
        ///         
        ///		    Repository.UpdateClient(c);
        ///		</code>
        /// </example>
        public Client UpdateClient(Client client)
		{
			return CreateClient(client, null);
		}

		/// <summary>
		/// Get the record for an existing client from the repository.
		/// </summary>
		/// <param name="client">Client name</param>
		/// <param name="options">There are no valid flags to use when fetching an existing client</param>
		/// <returns>The Client object if new client was found, null if creation failed</returns>
        /// <remarks> The '-i' flag is added if not specified by the caller	
        /// <br/> 
		/// <br/><b>p4 help client</b>
		/// <br/> 
		/// <br/>     client -- Create or edit a client workspace specification and its view
		/// <br/>     workspace -- Synonym for 'client'
		/// <br/> 
		/// <br/>     p4 client [-f] [-t template] [name]
		/// <br/>     p4 client -d [-f [-Fs]] name
		/// <br/>     p4 client -o [-t template] [name]
		/// <br/>     p4 client -S stream [[-c change] -o] [name]
		/// <br/>     p4 client -s [-f] -S stream [name]
		/// <br/>     p4 client -s [-f] -t template [name]
		/// <br/>     p4 client -i [-f]
		/// <br/> 
		/// <br/> 	Creates a new client specification ('spec') or edits an existing
		/// <br/> 	spec.  A client spec is a named mapping of depot files to workspace
		/// <br/> 	files.
		/// <br/> 
		/// <br/> 	The 'p4 client' command puts the client spec into a temporary file
		/// <br/> 	and invokes the editor configured by the environment variable
		/// <br/> 	$P4EDITOR.  For new workspaces, the client name defaults to the
		/// <br/> 	$P4CLIENT environment variable, if set, or to the current host name.
		/// <br/> 	Saving the file creates or modifies the client spec.
		/// <br/> 
		/// <br/> 	The client spec contains the following fields:
		/// <br/> 
		/// <br/> 	Client:      The client name.
		/// <br/> 
		/// <br/> 	Host:        If set, restricts access to the named host.
		/// <br/> 		     If unset, access is allowed from any host.
		/// <br/> 
		/// <br/> 	Owner:       The user who created this client.
		/// <br/> 
		/// <br/> 	Update:      The date that this spec was last modified.
		/// <br/> 
		/// <br/> 	Access:      The date that this client was last used in any way.
		/// <br/> 
		/// <br/> 	Description: A short description of the workspace.
		/// <br/> 
		/// <br/> 	Root:        The root directory of the workspace (specified in local
		/// <br/> 		     file system syntax), under which all versioned files
		/// <br/> 		     will be placed. If you change this setting, you must
		/// <br/> 		     physically relocate any files that currently reside
		/// <br/> 		     there.  On Windows client machines, you can specify the
		/// <br/> 		     root as "null" to enable you to map files to multiple
		/// <br/> 		     drives.
		/// <br/> 
		/// <br/> 	AltRoots:    Up to two optional alternate client workspace roots.
		/// <br/> 		     The first of the main and alternate roots to match the
		/// <br/> 		     client program's current working directory is used. If
		/// <br/> 		     none match, the main root is used. 'p4 info' displays
		/// <br/> 		     the root that is being used.
		/// <br/> 
		/// <br/> 	Options:     Flags to configure the client behavior. Defaults
		/// <br/> 		     are marked with *.
		/// <br/> 
		/// <br/> 		allwrite	Leaves all files writable on the client;
		/// <br/> 		noallwrite *	by default, only files opened by 'p4 edit'
		/// <br/> 				are writable. If set, files might be clobbered
		/// <br/> 				as a result of ignoring the clobber option
		/// <br/> 				(see below).
		/// <br/> 
		/// <br/> 		clobber		Permits 'p4 sync' to overwrite writable
		/// <br/> 		noclobber *	files on the client.  noclobber is ignored if
		/// <br/> 				allwrite is set.
		/// <br/> 
		/// <br/> 		compress 	Compresses data sent between the client
		/// <br/> 		nocompress *	and server to speed up slow connections.
		/// <br/> 
		/// <br/> 		locked   	Allows only the client owner to use or change
		/// <br/> 		unlocked *	the client spec.  Prevents the client spec from
		/// <br/> 				being deleted or unloaded.
		/// <br/> 
		/// <br/> 		modtime  	Causes 'p4 sync' and 'p4 submit' to preserve
		/// <br/> 		nomodtime *	file modification time, as with files with the
		/// <br/> 				+m type modifier. (See 'p4 help filetypes'.)
		/// <br/> 				With nomodtime, file timestamps are updated by
		/// <br/> 				sync and submit operations.
		/// <br/> 
		/// <br/> 		rmdir		Makes 'p4 sync' attempt to delete a workspace
		/// <br/> 		normdir *	directory when all files in it are removed.
		/// <br/> 
		/// <br/> 	SubmitOptions:  Flags to change submit behavior.
		/// <br/> 
		/// <br/> 		submitunchanged     All open files are submitted (default).
		/// <br/> 
		/// <br/> 		revertunchanged     Files that have content, type, or resolved
		/// <br/> 		                    changes are submitted. Unchanged files are
		/// <br/> 		                    reverted.
		/// <br/> 
		/// <br/> 		leaveunchanged      Files that have content, type, or resolved
		/// <br/> 		                    changes are submitted. Unchanged files are
		/// <br/> 		                    moved to the default changelist.
		/// <br/> 
		/// <br/> 		        +reopen     Can be appended to the submit option flag
		/// <br/> 		                    to cause submitted files to be reopened in
		/// <br/> 		                    the default changelist.
		/// <br/> 		                    Example: submitunchanged+reopen
		/// <br/> 
		/// <br/> 	LineEnd:    Set line-ending character(s) for client text files.
		/// <br/> 
		/// <br/> 		local		mode that is native to the client (default).
		/// <br/> 		unix		linefeed: UNIX style.
		/// <br/> 		mac		carriage return: Macintosh style.
		/// <br/> 		win		carriage return-linefeed: Windows style.
		/// <br/> 		share		hybrid: writes UNIX style but reads UNIX,
		/// <br/> 				Mac or Windows style.
		/// <br/> 
		/// <br/> 	View:        Maps files in the depot to files in your client
		/// <br/> 		     workspace.  Defines the files that you want in your
		/// <br/> 		     client workspace and specifies where you want them
		/// <br/> 		     to reside.  The default view maps all depot files
		/// <br/> 		     onto the client.  See 'p4 help views' for view syntax.
		/// <br/> 		     A new view takes effect on the next 'p4 sync'.
		/// <br/> 
		/// <br/> 	ChangeView:  Restricts depot paths to a particular point in time.
		/// <br/> 	             Example: //depot/path/...@1000
		/// <br/> 	             Revisions submitted to the path after the specified
		/// <br/> 	             changelist will not be visible.  Files matching a
		/// <br/> 	             ChangeView path may not be submitted.
		/// <br/> 
		/// <br/> 	Stream:      The stream to which this client's view will be dedicated.
		/// <br/> 		     (Files in stream paths can be submitted only by dedicated
		/// <br/> 		     stream clients.) When this optional field is set, the
		/// <br/> 		     View field will be automatically replaced by a stream
		/// <br/> 		     view as the client spec is saved.
		/// <br/> 
		/// <br/> 	StreamAtChange:
		/// <br/> 		     Stream meta-data (for view creation) is versioned.
		/// <br/> 		     When StreamAtChange is set, the stream hierarchy is
		/// <br/> 		     constructed back-in-time to reproduce the view that was
		/// <br/> 		     available to clients in the past. With this field set
		/// <br/> 		     changes cannot be be submitted from the client.
		/// <br/> 		     StreamAtChange is optional; Stream must be set as well.
		/// <br/> 
		/// <br/> 	ServerID:    If set, restricts usage to the named server.
		/// <br/> 		     If unset, usage is allowed on the master server and on
		/// <br/> 		     any replicas of the master other than Edge Servers.
		/// <br/> 
		/// <br/> 	Note: changing the client root does not actually move the client
		/// <br/> 	files; you must relocate them manually.  Similarly, changing
		/// <br/> 	the 'LineEnd' option does not actually update the client files;
		/// <br/> 	you can refresh them with 'p4 sync -f'.
		/// <br/> 
		/// <br/> 	The -d flag deletes the specified spec, as long as the client
		/// <br/> 	workspace has no opened files or pending changes.  (See 'p4 help
		/// <br/> 	opened'.) The -f flag forces the delete. The -Fs option used with -f
		/// <br/> 	forces client deletion even when the user has shelved changes. In
		/// <br/> 	this case, the shelved changes are left intact.
		/// <br/> 
		/// <br/> 	The -o flag writes the named client spec to the standard output.
		/// <br/> 	The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a client spec from the standard input.  The
		/// <br/> 	user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -t template flag, where 'template' is the name of another client
		/// <br/> 	spec, causes the View and Options fields to be replaced by those of
		/// <br/> 	the template.
		/// <br/> 
		/// <br/> 	The -f flag can force the updating of locked clients; normally
		/// <br/> 	locked clients can only be modified by their owner.  -f also allows
		/// <br/> 	the last modified date to be set.  The -f flag requires 'admin'
		/// <br/> 	access granted by 'p4 protect'.
		/// <br/> 
		/// <br/> 	The -s flag is used to switch an existing client spec's view without
		/// <br/> 	invoking the editor.  It can be used with -S to switch to a stream
		/// <br/> 	view, or with -t to switch to a view defined in another client spec.
		/// <br/> 	Switching views is not allowed in a client that has opened files.
		/// <br/> 	The -f flag can be used with -s to force switching with opened files.
		/// <br/> 	View switching has no effect on files in a client workspace until
		/// <br/> 	'p4 sync' is run.
		/// <br/> 
		/// <br/> 	Without -s, the '-S stream' flag can be used to create a new client
		/// <br/> 	spec dedicated to a stream. If the client spec already exists, and
		/// <br/> 	-S is used without -s, an error occurs when the client is bound to
		/// <br/> 	a different, or no stream.  Using -S sets the new client's
		/// <br/> 	Stream field.  The special syntax '-S //a/stream@changelist' can
		/// <br/> 	be used to set both Stream and StreamAtChange at the same time.
		/// <br/> 
		/// <br/> 	The '-S stream' flag can be used with '-o -c change' to inspect an
		/// <br/> 	old stream client view. It yields the client spec that would have
		/// <br/> 	been created for the stream at the moment the change was recorded.
		/// <br/> 
		/// <br/> 	A server of type 'build-server' (see 'p4 help server') is a replica
		/// <br/> 	which supports build farm integration, and the 'p4 client' command
		/// <br/> 	may be used to create or edit client workspaces on a build-server.
		/// <br/> 	Such workspaces may issue the 'p4 sync' command, in addition to any
		/// <br/> 	read-only command supported by the replica. For more information, run
		/// <br/> 	'p4 help buildserver'.
		/// <br/> 
		/// <br/> 	When creating or editing a client workspace for a build-server,
		/// <br/> 	the client specified by the optional 'name' argument, as well as the
		/// <br/> 	client specified by the $P4CLIENT environment variable or via the
		/// <br/> 	global '-c client' argument must not exist, or must be restricted
		/// <br/> 	to this server; this command may not be used to create or edit a
		/// <br/> 	workspace that is not restricted to this build-server.
		/// <br/> 
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To get the spec for a client named main_ws:
        ///		<code> 
        ///		    Client c = Repository.GetClient("main_ws", null);
        ///		</code>
        /// </example>
        /// <seealso cref="ClientCmdFlags"/> 
        public Client GetClient(string client, Options options)
		{
			if (client == null)
			{
				throw new ArgumentNullException("client");

			}
			P4Command cmd = new P4Command(this, "client", true, client);

			if (options == null)
			{
				options = new Options();
			}

			options["-o"] = null;

			P4CommandResult results = cmd.Run(options);
			if (results.Success)
			{
				if ((results.TaggedOutput == null) || (results.TaggedOutput.Count <= 0))
				{
					return null;
				}
				Client value = new Client();

				value.FromClientCmdTaggedOutput(results.TaggedOutput[0]);

				return value;
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}
			return null;
		}

        /// <summary>
        /// Get the record for an existing client from the repository.
        /// </summary>
        /// <param name="client">Client name</param>
        /// <returns>The Client object if new client was found, null if creation failed</returns>
        /// <remarks> The '-i' flag is added if not specified by the caller	
        /// <br/> 
		/// <br/><b>p4 help client</b>
		/// <br/> 
		/// <br/>     client -- Create or edit a client workspace specification and its view
		/// <br/>     workspace -- Synonym for 'client'
		/// <br/> 
		/// <br/>     p4 client [-f] [-t template] [name]
		/// <br/>     p4 client -d [-f [-Fs]] name
		/// <br/>     p4 client -o [-t template] [name]
		/// <br/>     p4 client -S stream [[-c change] -o] [name]
		/// <br/>     p4 client -s [-f] -S stream [name]
		/// <br/>     p4 client -s [-f] -t template [name]
		/// <br/>     p4 client -i [-f]
		/// <br/> 
		/// <br/> 	Creates a new client specification ('spec') or edits an existing
		/// <br/> 	spec.  A client spec is a named mapping of depot files to workspace
		/// <br/> 	files.
		/// <br/> 
		/// <br/> 	The 'p4 client' command puts the client spec into a temporary file
		/// <br/> 	and invokes the editor configured by the environment variable
		/// <br/> 	$P4EDITOR.  For new workspaces, the client name defaults to the
		/// <br/> 	$P4CLIENT environment variable, if set, or to the current host name.
		/// <br/> 	Saving the file creates or modifies the client spec.
		/// <br/> 
		/// <br/> 	The client spec contains the following fields:
		/// <br/> 
		/// <br/> 	Client:      The client name.
		/// <br/> 
		/// <br/> 	Host:        If set, restricts access to the named host.
		/// <br/> 		     If unset, access is allowed from any host.
		/// <br/> 
		/// <br/> 	Owner:       The user who created this client.
		/// <br/> 
		/// <br/> 	Update:      The date that this spec was last modified.
		/// <br/> 
		/// <br/> 	Access:      The date that this client was last used in any way.
		/// <br/> 
		/// <br/> 	Description: A short description of the workspace.
		/// <br/> 
		/// <br/> 	Root:        The root directory of the workspace (specified in local
		/// <br/> 		     file system syntax), under which all versioned files
		/// <br/> 		     will be placed. If you change this setting, you must
		/// <br/> 		     physically relocate any files that currently reside
		/// <br/> 		     there.  On Windows client machines, you can specify the
		/// <br/> 		     root as "null" to enable you to map files to multiple
		/// <br/> 		     drives.
		/// <br/> 
		/// <br/> 	AltRoots:    Up to two optional alternate client workspace roots.
		/// <br/> 		     The first of the main and alternate roots to match the
		/// <br/> 		     client program's current working directory is used. If
		/// <br/> 		     none match, the main root is used. 'p4 info' displays
		/// <br/> 		     the root that is being used.
		/// <br/> 
		/// <br/> 	Options:     Flags to configure the client behavior. Defaults
		/// <br/> 		     are marked with *.
		/// <br/> 
		/// <br/> 		allwrite	Leaves all files writable on the client;
		/// <br/> 		noallwrite *	by default, only files opened by 'p4 edit'
		/// <br/> 				are writable. If set, files might be clobbered
		/// <br/> 				as a result of ignoring the clobber option
		/// <br/> 				(see below).
		/// <br/> 
		/// <br/> 		clobber		Permits 'p4 sync' to overwrite writable
		/// <br/> 		noclobber *	files on the client.  noclobber is ignored if
		/// <br/> 				allwrite is set.
		/// <br/> 
		/// <br/> 		compress 	Compresses data sent between the client
		/// <br/> 		nocompress *	and server to speed up slow connections.
		/// <br/> 
		/// <br/> 		locked   	Allows only the client owner to use or change
		/// <br/> 		unlocked *	the client spec.  Prevents the client spec from
		/// <br/> 				being deleted or unloaded.
		/// <br/> 
		/// <br/> 		modtime  	Causes 'p4 sync' and 'p4 submit' to preserve
		/// <br/> 		nomodtime *	file modification time, as with files with the
		/// <br/> 				+m type modifier. (See 'p4 help filetypes'.)
		/// <br/> 				With nomodtime, file timestamps are updated by
		/// <br/> 				sync and submit operations.
		/// <br/> 
		/// <br/> 		rmdir		Makes 'p4 sync' attempt to delete a workspace
		/// <br/> 		normdir *	directory when all files in it are removed.
		/// <br/> 
		/// <br/> 	SubmitOptions:  Flags to change submit behavior.
		/// <br/> 
		/// <br/> 		submitunchanged     All open files are submitted (default).
		/// <br/> 
		/// <br/> 		revertunchanged     Files that have content, type, or resolved
		/// <br/> 		                    changes are submitted. Unchanged files are
		/// <br/> 		                    reverted.
		/// <br/> 
		/// <br/> 		leaveunchanged      Files that have content, type, or resolved
		/// <br/> 		                    changes are submitted. Unchanged files are
		/// <br/> 		                    moved to the default changelist.
		/// <br/> 
		/// <br/> 		        +reopen     Can be appended to the submit option flag
		/// <br/> 		                    to cause submitted files to be reopened in
		/// <br/> 		                    the default changelist.
		/// <br/> 		                    Example: submitunchanged+reopen
		/// <br/> 
		/// <br/> 	LineEnd:    Set line-ending character(s) for client text files.
		/// <br/> 
		/// <br/> 		local		mode that is native to the client (default).
		/// <br/> 		unix		linefeed: UNIX style.
		/// <br/> 		mac		carriage return: Macintosh style.
		/// <br/> 		win		carriage return-linefeed: Windows style.
		/// <br/> 		share		hybrid: writes UNIX style but reads UNIX,
		/// <br/> 				Mac or Windows style.
		/// <br/> 
		/// <br/> 	View:        Maps files in the depot to files in your client
		/// <br/> 		     workspace.  Defines the files that you want in your
		/// <br/> 		     client workspace and specifies where you want them
		/// <br/> 		     to reside.  The default view maps all depot files
		/// <br/> 		     onto the client.  See 'p4 help views' for view syntax.
		/// <br/> 		     A new view takes effect on the next 'p4 sync'.
		/// <br/> 
		/// <br/> 	ChangeView:  Restricts depot paths to a particular point in time.
		/// <br/> 	             Example: //depot/path/...@1000
		/// <br/> 	             Revisions submitted to the path after the specified
		/// <br/> 	             changelist will not be visible.  Files matching a
		/// <br/> 	             ChangeView path may not be submitted.
		/// <br/> 
		/// <br/> 	Stream:      The stream to which this client's view will be dedicated.
		/// <br/> 		     (Files in stream paths can be submitted only by dedicated
		/// <br/> 		     stream clients.) When this optional field is set, the
		/// <br/> 		     View field will be automatically replaced by a stream
		/// <br/> 		     view as the client spec is saved.
		/// <br/> 
		/// <br/> 	StreamAtChange:
		/// <br/> 		     Stream meta-data (for view creation) is versioned.
		/// <br/> 		     When StreamAtChange is set, the stream hierarchy is
		/// <br/> 		     constructed back-in-time to reproduce the view that was
		/// <br/> 		     available to clients in the past. With this field set
		/// <br/> 		     changes cannot be be submitted from the client.
		/// <br/> 		     StreamAtChange is optional; Stream must be set as well.
		/// <br/> 
		/// <br/> 	ServerID:    If set, restricts usage to the named server.
		/// <br/> 		     If unset, usage is allowed on the master server and on
		/// <br/> 		     any replicas of the master other than Edge Servers.
		/// <br/> 
		/// <br/> 	Note: changing the client root does not actually move the client
		/// <br/> 	files; you must relocate them manually.  Similarly, changing
		/// <br/> 	the 'LineEnd' option does not actually update the client files;
		/// <br/> 	you can refresh them with 'p4 sync -f'.
		/// <br/> 
		/// <br/> 	The -d flag deletes the specified spec, as long as the client
		/// <br/> 	workspace has no opened files or pending changes.  (See 'p4 help
		/// <br/> 	opened'.) The -f flag forces the delete. The -Fs option used with -f
		/// <br/> 	forces client deletion even when the user has shelved changes. In
		/// <br/> 	this case, the shelved changes are left intact.
		/// <br/> 
		/// <br/> 	The -o flag writes the named client spec to the standard output.
		/// <br/> 	The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a client spec from the standard input.  The
		/// <br/> 	user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -t template flag, where 'template' is the name of another client
		/// <br/> 	spec, causes the View and Options fields to be replaced by those of
		/// <br/> 	the template.
		/// <br/> 
		/// <br/> 	The -f flag can force the updating of locked clients; normally
		/// <br/> 	locked clients can only be modified by their owner.  -f also allows
		/// <br/> 	the last modified date to be set.  The -f flag requires 'admin'
		/// <br/> 	access granted by 'p4 protect'.
		/// <br/> 
		/// <br/> 	The -s flag is used to switch an existing client spec's view without
		/// <br/> 	invoking the editor.  It can be used with -S to switch to a stream
		/// <br/> 	view, or with -t to switch to a view defined in another client spec.
		/// <br/> 	Switching views is not allowed in a client that has opened files.
		/// <br/> 	The -f flag can be used with -s to force switching with opened files.
		/// <br/> 	View switching has no effect on files in a client workspace until
		/// <br/> 	'p4 sync' is run.
		/// <br/> 
		/// <br/> 	Without -s, the '-S stream' flag can be used to create a new client
		/// <br/> 	spec dedicated to a stream. If the client spec already exists, and
		/// <br/> 	-S is used without -s, an error occurs when the client is bound to
		/// <br/> 	a different, or no stream.  Using -S sets the new client's
		/// <br/> 	Stream field.  The special syntax '-S //a/stream@changelist' can
		/// <br/> 	be used to set both Stream and StreamAtChange at the same time.
		/// <br/> 
		/// <br/> 	The '-S stream' flag can be used with '-o -c change' to inspect an
		/// <br/> 	old stream client view. It yields the client spec that would have
		/// <br/> 	been created for the stream at the moment the change was recorded.
		/// <br/> 
		/// <br/> 	A server of type 'build-server' (see 'p4 help server') is a replica
		/// <br/> 	which supports build farm integration, and the 'p4 client' command
		/// <br/> 	may be used to create or edit client workspaces on a build-server.
		/// <br/> 	Such workspaces may issue the 'p4 sync' command, in addition to any
		/// <br/> 	read-only command supported by the replica. For more information, run
		/// <br/> 	'p4 help buildserver'.
		/// <br/> 
		/// <br/> 	When creating or editing a client workspace for a build-server,
		/// <br/> 	the client specified by the optional 'name' argument, as well as the
		/// <br/> 	client specified by the $P4CLIENT environment variable or via the
		/// <br/> 	global '-c client' argument must not exist, or must be restricted
		/// <br/> 	to this server; this command may not be used to create or edit a
		/// <br/> 	workspace that is not restricted to this build-server.
		/// <br/> 
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To get the spec for a client named main_ws:
        ///		<code> 
        ///		    Client c = Repository.GetClient("main_ws");
        ///		</code>
        /// </example>
        public Client GetClient(string client)
		{
			return GetClient(client, null);
		}

		/// <summary>
		/// Get a list of clients from the repository
		/// </summary>
		/// <param name="options">options for the clients command<see cref="ClientsCmdOptions"/></param>
		/// <returns>A list containing the matching clients</returns>
		/// <remarks>
		/// <br/><b>p4 help clients</b>
		/// <br/> 
		/// <br/>     clients -- Display list of clients
		/// <br/>     workspaces -- synonym for 'clients'
		/// <br/> 
		/// <br/>     p4 clients [-t] [-u user] [[-e|-E] nameFilter -m max] [-S stream]
		/// <br/>                [-a | -s serverID]
		/// <br/>     p4 clients -U
		/// <br/> 
		/// <br/> 	Lists all client workspaces currently defined in the server.
		/// <br/> 
		/// <br/> 	The -t flag displays the time as well as the date.
		/// <br/> 
		/// <br/> 	The -u user flag lists client workspaces that are owned by the
		/// <br/> 	specified user.
		/// <br/> 
		/// <br/> 	The -e nameFilter flag lists workspaces with a name that matches
		/// <br/> 	the nameFilter pattern, for example: -e 'svr-dev-rel*'. The -e flag
		/// <br/> 	uses the server's normal case-sensitivity rules. The -E flag makes
		/// <br/> 	the matching case-insensitive, even on a case-sensitive server.
		/// <br/> 
		/// <br/> 	The -m max flag limits output to the specified number of workspaces.
		/// <br/> 
		/// <br/> 	The -S stream flag limits output to the client workspaces dedicated 
		/// <br/> 	to the stream.
		/// <br/> 
		/// <br/> 	The -U flag lists unloaded clients (see 'p4 help unload').
		/// <br/> 
		/// <br/> 	The -a flag specifies that all clients should be displayed, not just
		/// <br/> 	those that are bound to this server.
		/// <br/> 
		/// <br/> 	The -s serverID flag specifies that only those clients bound to the
		/// <br/> 	specified serverID should be displayed. On an Edge Server, the -s
		/// <br/> 	flag defaults to the Edge Server's serverID.
		/// <br/> 
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To get a list of a maximum of 10 clients:
        ///		<code> 
        ///			ClientsCmdOptions opts =
        ///			new ClientsCmdOptions(ClientsCmdFlags.NoneIncludeTime, null, null, 10, null));
        ///			
        ///			IList&lt;Client&gt; clients = Repository.GetClients(opts);
        ///		</code>
        ///		To get a list of clients that have "iOS_app" in their name:
        ///		<code> 
        ///			ClientsCmdOptions opts =
        ///			new ClientsCmdOptions(ClientsCmdFlags.NoneIncludeTime, null, "*iOS_app*, 0, null));
        ///			
        ///			IList&lt;Client&gt; clients = Repository.GetClients(opts);
        ///		</code>
        ///		To get a list of clients that can be used with stream //depot/main:
        ///		<code> 
        ///			ClientsCmdOptions opts =
        ///			new ClientsCmdOptions(ClientsCmdFlags.NoneIncludeTime, null, null, 0, "//depot/main"));
        ///			
        ///			IList&lt;Client&gt; clients = Repository.GetClients(opts);
        ///		</code>
        ///		To get a list clients owned by user bsmith:
        ///		<code> 
        ///			ClientsCmdOptions opts =
        ///			new ClientsCmdOptions(ClientsCmdFlags.NoneIncludeTime, "bsmith", null, 0, null));
        ///			
        ///			IList&lt;Client&gt; clients = Repository.GetClients(opts);
        ///		</code>
        /// </example>
        /// <seealso cref="ClientsCmdOptions"/> 
        public IList<Client> GetClients(Options options)
		{
			P4Command cmd = new P4Command(this, "clients", true);

			P4CommandResult results = cmd.Run(options);
			if (results.Success)
			{
				if ((results.TaggedOutput == null) || (results.TaggedOutput.Count <= 0))
				{
					return null;
				}
				List<Client> value = new List<Client>();
				foreach (TaggedObject obj in results.TaggedOutput)
				{
					Client client = new Client();

                    bool dst_mismatch = false;
                    string offset = string.Empty;

                    if (Server != null && Server.Metadata != null)
                    {
                        offset = Server.Metadata.DateTimeOffset;
                        dst_mismatch = FormBase.DSTMismatch(Server.Metadata);
                    }

					client.FromClientsCmdTaggedOutput(obj, offset,dst_mismatch);
					value.Add(client);
				}
				return value;
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}
			return null;
		}

		/// <summary>
		/// Delete a client from the repository
		/// </summary>
		/// <param name="client">The client to be deleted</param>
		/// <param name="options">Only the '-f' flag is valid when deleting an existing client</param>
        /// <remarks> 
        /// <br/> 
		/// <br/><b>p4 help client</b>
		/// <br/> 
		/// <br/>     client -- Create or edit a client workspace specification and its view
		/// <br/>     workspace -- Synonym for 'client'
		/// <br/> 
		/// <br/>     p4 client [-f] [-t template] [name]
		/// <br/>     p4 client -d [-f [-Fs]] name
		/// <br/>     p4 client -o [-t template] [name]
		/// <br/>     p4 client -S stream [[-c change] -o] [name]
		/// <br/>     p4 client -s [-f] -S stream [name]
		/// <br/>     p4 client -s [-f] -t template [name]
		/// <br/>     p4 client -i [-f]
		/// <br/> 
		/// <br/> 	Creates a new client specification ('spec') or edits an existing
		/// <br/> 	spec.  A client spec is a named mapping of depot files to workspace
		/// <br/> 	files.
		/// <br/> 
		/// <br/> 	The 'p4 client' command puts the client spec into a temporary file
		/// <br/> 	and invokes the editor configured by the environment variable
		/// <br/> 	$P4EDITOR.  For new workspaces, the client name defaults to the
		/// <br/> 	$P4CLIENT environment variable, if set, or to the current host name.
		/// <br/> 	Saving the file creates or modifies the client spec.
		/// <br/> 
		/// <br/> 	The client spec contains the following fields:
		/// <br/> 
		/// <br/> 	Client:      The client name.
		/// <br/> 
		/// <br/> 	Host:        If set, restricts access to the named host.
		/// <br/> 		     If unset, access is allowed from any host.
		/// <br/> 
		/// <br/> 	Owner:       The user who created this client.
		/// <br/> 
		/// <br/> 	Update:      The date that this spec was last modified.
		/// <br/> 
		/// <br/> 	Access:      The date that this client was last used in any way.
		/// <br/> 
		/// <br/> 	Description: A short description of the workspace.
		/// <br/> 
		/// <br/> 	Root:        The root directory of the workspace (specified in local
		/// <br/> 		     file system syntax), under which all versioned files
		/// <br/> 		     will be placed. If you change this setting, you must
		/// <br/> 		     physically relocate any files that currently reside
		/// <br/> 		     there.  On Windows client machines, you can specify the
		/// <br/> 		     root as "null" to enable you to map files to multiple
		/// <br/> 		     drives.
		/// <br/> 
		/// <br/> 	AltRoots:    Up to two optional alternate client workspace roots.
		/// <br/> 		     The first of the main and alternate roots to match the
		/// <br/> 		     client program's current working directory is used. If
		/// <br/> 		     none match, the main root is used. 'p4 info' displays
		/// <br/> 		     the root that is being used.
		/// <br/> 
		/// <br/> 	Options:     Flags to configure the client behavior. Defaults
		/// <br/> 		     are marked with *.
		/// <br/> 
		/// <br/> 		allwrite	Leaves all files writable on the client;
		/// <br/> 		noallwrite *	by default, only files opened by 'p4 edit'
		/// <br/> 				are writable. If set, files might be clobbered
		/// <br/> 				as a result of ignoring the clobber option
		/// <br/> 				(see below).
		/// <br/> 
		/// <br/> 		clobber		Permits 'p4 sync' to overwrite writable
		/// <br/> 		noclobber *	files on the client.  noclobber is ignored if
		/// <br/> 				allwrite is set.
		/// <br/> 
		/// <br/> 		compress 	Compresses data sent between the client
		/// <br/> 		nocompress *	and server to speed up slow connections.
		/// <br/> 
		/// <br/> 		locked   	Allows only the client owner to use or change
		/// <br/> 		unlocked *	the client spec.  Prevents the client spec from
		/// <br/> 				being deleted or unloaded.
		/// <br/> 
		/// <br/> 		modtime  	Causes 'p4 sync' and 'p4 submit' to preserve
		/// <br/> 		nomodtime *	file modification time, as with files with the
		/// <br/> 				+m type modifier. (See 'p4 help filetypes'.)
		/// <br/> 				With nomodtime, file timestamps are updated by
		/// <br/> 				sync and submit operations.
		/// <br/> 
		/// <br/> 		rmdir		Makes 'p4 sync' attempt to delete a workspace
		/// <br/> 		normdir *	directory when all files in it are removed.
		/// <br/> 
		/// <br/> 	SubmitOptions:  Flags to change submit behavior.
		/// <br/> 
		/// <br/> 		submitunchanged     All open files are submitted (default).
		/// <br/> 
		/// <br/> 		revertunchanged     Files that have content, type, or resolved
		/// <br/> 		                    changes are submitted. Unchanged files are
		/// <br/> 		                    reverted.
		/// <br/> 
		/// <br/> 		leaveunchanged      Files that have content, type, or resolved
		/// <br/> 		                    changes are submitted. Unchanged files are
		/// <br/> 		                    moved to the default changelist.
		/// <br/> 
		/// <br/> 		        +reopen     Can be appended to the submit option flag
		/// <br/> 		                    to cause submitted files to be reopened in
		/// <br/> 		                    the default changelist.
		/// <br/> 		                    Example: submitunchanged+reopen
		/// <br/> 
		/// <br/> 	LineEnd:    Set line-ending character(s) for client text files.
		/// <br/> 
		/// <br/> 		local		mode that is native to the client (default).
		/// <br/> 		unix		linefeed: UNIX style.
		/// <br/> 		mac		carriage return: Macintosh style.
		/// <br/> 		win		carriage return-linefeed: Windows style.
		/// <br/> 		share		hybrid: writes UNIX style but reads UNIX,
		/// <br/> 				Mac or Windows style.
		/// <br/> 
		/// <br/> 	View:        Maps files in the depot to files in your client
		/// <br/> 		     workspace.  Defines the files that you want in your
		/// <br/> 		     client workspace and specifies where you want them
		/// <br/> 		     to reside.  The default view maps all depot files
		/// <br/> 		     onto the client.  See 'p4 help views' for view syntax.
		/// <br/> 		     A new view takes effect on the next 'p4 sync'.
		/// <br/> 
		/// <br/> 	ChangeView:  Restricts depot paths to a particular point in time.
		/// <br/> 	             Example: //depot/path/...@1000
		/// <br/> 	             Revisions submitted to the path after the specified
		/// <br/> 	             changelist will not be visible.  Files matching a
		/// <br/> 	             ChangeView path may not be submitted.
		/// <br/> 
		/// <br/> 	Stream:      The stream to which this client's view will be dedicated.
		/// <br/> 		     (Files in stream paths can be submitted only by dedicated
		/// <br/> 		     stream clients.) When this optional field is set, the
		/// <br/> 		     View field will be automatically replaced by a stream
		/// <br/> 		     view as the client spec is saved.
		/// <br/> 
		/// <br/> 	StreamAtChange:
		/// <br/> 		     Stream meta-data (for view creation) is versioned.
		/// <br/> 		     When StreamAtChange is set, the stream hierarchy is
		/// <br/> 		     constructed back-in-time to reproduce the view that was
		/// <br/> 		     available to clients in the past. With this field set
		/// <br/> 		     changes cannot be be submitted from the client.
		/// <br/> 		     StreamAtChange is optional; Stream must be set as well.
		/// <br/> 
		/// <br/> 	ServerID:    If set, restricts usage to the named server.
		/// <br/> 		     If unset, usage is allowed on the master server and on
		/// <br/> 		     any replicas of the master other than Edge Servers.
		/// <br/> 
		/// <br/> 	Note: changing the client root does not actually move the client
		/// <br/> 	files; you must relocate them manually.  Similarly, changing
		/// <br/> 	the 'LineEnd' option does not actually update the client files;
		/// <br/> 	you can refresh them with 'p4 sync -f'.
		/// <br/> 
		/// <br/> 	The -d flag deletes the specified spec, as long as the client
		/// <br/> 	workspace has no opened files or pending changes.  (See 'p4 help
		/// <br/> 	opened'.) The -f flag forces the delete. The -Fs option used with -f
		/// <br/> 	forces client deletion even when the user has shelved changes. In
		/// <br/> 	this case, the shelved changes are left intact.
		/// <br/> 
		/// <br/> 	The -o flag writes the named client spec to the standard output.
		/// <br/> 	The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a client spec from the standard input.  The
		/// <br/> 	user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -t template flag, where 'template' is the name of another client
		/// <br/> 	spec, causes the View and Options fields to be replaced by those of
		/// <br/> 	the template.
		/// <br/> 
		/// <br/> 	The -f flag can force the updating of locked clients; normally
		/// <br/> 	locked clients can only be modified by their owner.  -f also allows
		/// <br/> 	the last modified date to be set.  The -f flag requires 'admin'
		/// <br/> 	access granted by 'p4 protect'.
		/// <br/> 
		/// <br/> 	The -s flag is used to switch an existing client spec's view without
		/// <br/> 	invoking the editor.  It can be used with -S to switch to a stream
		/// <br/> 	view, or with -t to switch to a view defined in another client spec.
		/// <br/> 	Switching views is not allowed in a client that has opened files.
		/// <br/> 	The -f flag can be used with -s to force switching with opened files.
		/// <br/> 	View switching has no effect on files in a client workspace until
		/// <br/> 	'p4 sync' is run.
		/// <br/> 
		/// <br/> 	Without -s, the '-S stream' flag can be used to create a new client
		/// <br/> 	spec dedicated to a stream. If the client spec already exists, and
		/// <br/> 	-S is used without -s, an error occurs when the client is bound to
		/// <br/> 	a different, or no stream.  Using -S sets the new client's
		/// <br/> 	Stream field.  The special syntax '-S //a/stream@changelist' can
		/// <br/> 	be used to set both Stream and StreamAtChange at the same time.
		/// <br/> 
		/// <br/> 	The '-S stream' flag can be used with '-o -c change' to inspect an
		/// <br/> 	old stream client view. It yields the client spec that would have
		/// <br/> 	been created for the stream at the moment the change was recorded.
		/// <br/> 
		/// <br/> 	A server of type 'build-server' (see 'p4 help server') is a replica
		/// <br/> 	which supports build farm integration, and the 'p4 client' command
		/// <br/> 	may be used to create or edit client workspaces on a build-server.
		/// <br/> 	Such workspaces may issue the 'p4 sync' command, in addition to any
		/// <br/> 	read-only command supported by the replica. For more information, run
		/// <br/> 	'p4 help buildserver'.
		/// <br/> 
		/// <br/> 	When creating or editing a client workspace for a build-server,
		/// <br/> 	the client specified by the optional 'name' argument, as well as the
		/// <br/> 	client specified by the $P4CLIENT environment variable or via the
		/// <br/> 	global '-c client' argument must not exist, or must be restricted
		/// <br/> 	to this server; this command may not be used to create or edit a
		/// <br/> 	workspace that is not restricted to this build-server.
		/// <br/> 
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To delete a client named main_ws with the user who owns it:
        ///		<code> 
        ///		    Client c = Repository.GetClient("main_ws");
        ///		    Repository.DeleteClient(c, null);
        ///		</code>
        ///		To delete a locked client named main_ws with an admin user:
        ///		<code> 
        ///		    Client c = Repository.GetClient("main_ws", null);
        ///		    
        ///         ClientCmdOptions opts = new ClientCmdOptions(ClientCmdFlags.Force);
        ///         rep.DeleteClient(c, opts);
        ///		</code>
        /// </example>
        /// <seealso cref="ClientCmdFlags"/> 
		public void DeleteClient(Client client, Options options)
		{
			if (client == null)
			{
				throw new ArgumentNullException("client");

			}
			P4Command cmd = new P4Command(this, "client", true, client.Name);

			if (options == null)
			{
				options = new Options();
			}
			options["-d"] = null;

			P4CommandResult results = cmd.Run(options);
			if (results.Success == false)
			{
				P4Exception.Throw(results.ErrorList);
			}
		}
	}
}
