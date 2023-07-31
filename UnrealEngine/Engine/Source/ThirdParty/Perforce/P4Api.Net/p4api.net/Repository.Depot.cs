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
 * Name		: Repository.Depot.cs
 *
 * Author	: wjb
 *
 * Description	: Depot operations for the Repository.
 *
 ******************************************************************************/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace Perforce.P4
{
	public partial class Repository
	{
		/// <summary>
		/// Create a new depot in the repository.
		/// </summary>
		/// <param name="depot">Depot specification for the new depot</param>
		/// <param name="options">The '-i' flag is required when creating a new depot</param>
		/// <returns>The Depot object if new depot was created, null if creation failed</returns>
		/// <remarks> The '-i' flag is added if not specified by the caller
		/// <br/> 
		/// <br/><b>p4 help depot</b>
		/// <br/> 
		/// <br/>     depot -- Create or edit a depot specification
		/// <br/> 
		/// <br/>     p4 depot name
		/// <br/>     p4 depot -d [-f] name
		/// <br/>     p4 depot -o name
		/// <br/>     p4 depot -i
		/// <br/> 
		/// <br/> 	Create a new depot specification or edit an existing depot
		/// <br/> 	specification. The specification form is put into a temporary file
		/// <br/> 	and the editor (configured by the environment variable $P4EDITOR)
		/// <br/> 	is invoked.
		/// <br/> 
		/// <br/> 	The depot specification contains the following fields:
		/// <br/> 
		/// <br/> 	Depot:       The name of the depot.  This name cannot be the same as
		/// <br/> 		     any branch, client, or label name.
		/// <br/> 
		/// <br/> 	Owner:       The user who created this depot.
		/// <br/> 
		/// <br/> 	Date:        The date that this specification was last modified.
		/// <br/> 
		/// <br/> 	Description: A short description of the depot (optional).
		/// <br/> 
		/// <br/> 	Type:        One of the types: 'local', 'stream', 'remote', 'spec',
		/// <br/> 		     'archive', or 'unload'.
		/// <br/> 
		/// <br/> 		     A 'local' depot (the default) is managed directly by
		/// <br/> 		     the server and its files reside in the server's root
		/// <br/> 		     directory.
		/// <br/> 
		/// <br/> 		     A 'stream' depot is a local depot dedicated to the
		/// <br/> 		     storage of files in a stream.
		/// <br/> 
		/// <br/> 		     A 'remote' depot refers to files in another Perforce
		/// <br/> 		     server.
		/// <br/> 
		/// <br/> 		     A 'spec' depot automatically archives all edited forms
		/// <br/> 		     (branch, change, client, depot, group, job, jobspec,
		/// <br/> 		     protect, triggers, typemap, and user) in special,
		/// <br/> 		     read-only files.  The files are named:
		/// <br/> 		     //depotname/formtype/name[suffix].  Updates to jobs made
		/// <br/> 		     by the 'p4 change', 'p4 fix', and 'p4 submit' commands
		/// <br/> 		     are also saved, but other automatic updates such as
		/// <br/> 		     as access times or opened files (for changes) are not.
		/// <br/> 		     A server can contain only one 'spec' depot.
		/// <br/> 
		/// <br/> 		     A 'archive' depot defines a storage location to which
		/// <br/> 		     obsolete revisions may be relocated.
		/// <br/> 
		/// <br/> 		     An 'unload' depot defines a storage location to which
		/// <br/> 		     database records may be unloaded and from which they
		/// <br/> 		     may be reloaded.
		/// <br/> 
		/// <br/> 	Address:     For remote depots, the $P4PORT (connection address)
		/// <br/> 		     of the remote server.
		/// <br/> 
		/// <br/> 	Suffix:      For spec depots, the optional suffix to be used
		/// <br/> 		     for generated paths. The default is '.p4s'.
		/// <br/> 
		/// <br/> 	Map:         Path translation information, in the form of a file
		/// <br/> 		     pattern with a single ... in it.  For local depots,
		/// <br/> 		     this path is relative to the server's root directory
		/// <br/> 		     or to server.depot.root if it has been configured
		/// <br/> 		     (Example: depot/...).  For remote depots, this path
		/// <br/> 		     refers to the remote server's namespace
		/// <br/> 		     (Example: //depot/...).
		/// <br/> 
		/// <br/> 	SpecMap:     For spec depots, the optional description of which
		/// <br/> 	             specs should be saved, as one or more patterns.
		/// <br/> 
		/// <br/> 	The -d flag deletes the specified depot.  If any files reside in the
		/// <br/> 	depot, they must be removed with 'p4 obliterate' before deleting the
		/// <br/> 	depot. If any archive files remain in the depot directory, they may
		/// <br/> 	be referenced by lazy copies in other depots; use 'p4 snap' to break
		/// <br/> 	those linkages. Snap lazy copies prior to obliterating the old depot
		/// <br/> 	files to allow the obliterate command to remove any unreferenced
		/// <br/> 	archives from the depot directory. If the depot directory is not
		/// <br/> 	empty, you must specify the -f flag to delete the depot.
		/// <br/> 
		/// <br/> 	The -o flag writes the depot specification to standard output. The
		/// <br/> 	user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a depot specification from standard input. The
		/// <br/> 	user's editor is not invoked.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To create a streams depot named MobileApp:
        ///		<code> 
        ///		   	Depot d = new Depot();
        ///		   			   	                  
        ///		   	d.Id = "MobileApp";
        ///		   	d.Description = "Stream depot for mobile app project";
        ///		   	d.Owner = "admin";
        ///		   	d.Type = DepotType.Stream;
        ///		   	d.Map = "MobileApp/...";
        ///		   	
        ///		   	Depot MobileApp = Repository.CreateDepot(d, null);
        ///		</code>
        /// </example>
        /// <seealso cref="DepotCmdFlags"/>
        public Depot CreateDepot(Depot depot, Options options)
		{
			if (depot == null)
			{
				throw new ArgumentNullException("depot");

			}
			P4Command cmd = new P4Command(this, "depot", true);

			cmd.DataSet = depot.ToString();

			if (options == null)
			{
				options = new Options(DepotCmdFlags.Input);
			}
			if (options.ContainsKey("-i") == false)
			{
				options["-i"] = null;
			}

			P4CommandResult results = cmd.Run(options);
			if (results.Success)
			{
				return depot;
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}
			return null;
		}

        /// <summary>
        /// Create a new depot in the repository.
        /// </summary>
        /// <param name="depot">Depot specification for the new depot</param>
        /// <returns>The Depot object if new depot was created, null if creation failed</returns>
        /// <remarks> The '-i' flag is added if not specified by the caller
        /// <br/> 
		/// <br/><b>p4 help depot</b>
		/// <br/> 
		/// <br/>     depot -- Create or edit a depot specification
		/// <br/> 
		/// <br/>     p4 depot name
		/// <br/>     p4 depot -d [-f] name
		/// <br/>     p4 depot -o name
		/// <br/>     p4 depot -i
		/// <br/> 
		/// <br/> 	Create a new depot specification or edit an existing depot
		/// <br/> 	specification. The specification form is put into a temporary file
		/// <br/> 	and the editor (configured by the environment variable $P4EDITOR)
		/// <br/> 	is invoked.
		/// <br/> 
		/// <br/> 	The depot specification contains the following fields:
		/// <br/> 
		/// <br/> 	Depot:       The name of the depot.  This name cannot be the same as
		/// <br/> 		     any branch, client, or label name.
		/// <br/> 
		/// <br/> 	Owner:       The user who created this depot.
		/// <br/> 
		/// <br/> 	Date:        The date that this specification was last modified.
		/// <br/> 
		/// <br/> 	Description: A short description of the depot (optional).
		/// <br/> 
		/// <br/> 	Type:        One of the types: 'local', 'stream', 'remote', 'spec',
		/// <br/> 		     'archive', or 'unload'.
		/// <br/> 
		/// <br/> 		     A 'local' depot (the default) is managed directly by
		/// <br/> 		     the server and its files reside in the server's root
		/// <br/> 		     directory.
		/// <br/> 
		/// <br/> 		     A 'stream' depot is a local depot dedicated to the
		/// <br/> 		     storage of files in a stream.
		/// <br/> 
		/// <br/> 		     A 'remote' depot refers to files in another Perforce
		/// <br/> 		     server.
		/// <br/> 
		/// <br/> 		     A 'spec' depot automatically archives all edited forms
		/// <br/> 		     (branch, change, client, depot, group, job, jobspec,
		/// <br/> 		     protect, triggers, typemap, and user) in special,
		/// <br/> 		     read-only files.  The files are named:
		/// <br/> 		     //depotname/formtype/name[suffix].  Updates to jobs made
		/// <br/> 		     by the 'p4 change', 'p4 fix', and 'p4 submit' commands
		/// <br/> 		     are also saved, but other automatic updates such as
		/// <br/> 		     as access times or opened files (for changes) are not.
		/// <br/> 		     A server can contain only one 'spec' depot.
		/// <br/> 
		/// <br/> 		     A 'archive' depot defines a storage location to which
		/// <br/> 		     obsolete revisions may be relocated.
		/// <br/> 
		/// <br/> 		     An 'unload' depot defines a storage location to which
		/// <br/> 		     database records may be unloaded and from which they
		/// <br/> 		     may be reloaded.
		/// <br/> 
		/// <br/> 	Address:     For remote depots, the $P4PORT (connection address)
		/// <br/> 		     of the remote server.
		/// <br/> 
		/// <br/> 	Suffix:      For spec depots, the optional suffix to be used
		/// <br/> 		     for generated paths. The default is '.p4s'.
		/// <br/> 
		/// <br/> 	Map:         Path translation information, in the form of a file
		/// <br/> 		     pattern with a single ... in it.  For local depots,
		/// <br/> 		     this path is relative to the server's root directory
		/// <br/> 		     or to server.depot.root if it has been configured
		/// <br/> 		     (Example: depot/...).  For remote depots, this path
		/// <br/> 		     refers to the remote server's namespace
		/// <br/> 		     (Example: //depot/...).
		/// <br/> 
		/// <br/> 	SpecMap:     For spec depots, the optional description of which
		/// <br/> 	             specs should be saved, as one or more patterns.
		/// <br/> 
		/// <br/> 	The -d flag deletes the specified depot.  If any files reside in the
		/// <br/> 	depot, they must be removed with 'p4 obliterate' before deleting the
		/// <br/> 	depot. If any archive files remain in the depot directory, they may
		/// <br/> 	be referenced by lazy copies in other depots; use 'p4 snap' to break
		/// <br/> 	those linkages. Snap lazy copies prior to obliterating the old depot
		/// <br/> 	files to allow the obliterate command to remove any unreferenced
		/// <br/> 	archives from the depot directory. If the depot directory is not
		/// <br/> 	empty, you must specify the -f flag to delete the depot.
		/// <br/> 
		/// <br/> 	The -o flag writes the depot specification to standard output. The
		/// <br/> 	user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a depot specification from standard input. The
		/// <br/> 	user's editor is not invoked.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To create a streams depot named MobileApp:
        ///		<code> 
        ///		   	Depot d = new Depot();
        ///		   			   	                  
        ///		   	d.Id = "MobileApp";
        ///		   	d.Description = "Stream depot for mobile app project";
        ///		   	d.Owner = "admin";
        ///		   	d.Type = DepotType.Stream;
        ///		   	d.Map = "MobileApp/...";
        ///		   	
        ///		   	Depot MobileApp = Repository.CreateDepot(d);
        ///		</code>
        /// </example>
        public Depot CreateDepot(Depot depot)
		{
			return CreateDepot(depot, null);
		}

		/// <summary>
		/// Update the record for a depot in the repository
		/// </summary>
		/// <param name="depot">Depot specification for the depot being updated</param>
		/// <returns>The Depot object if new depot was saved, null if creation failed</returns>
        /// <remarks> The '-i' flag is added if not specified by the caller
        /// <br/> 
		/// <br/><b>p4 help depot</b>
		/// <br/> 
		/// <br/>     depot -- Create or edit a depot specification
		/// <br/> 
		/// <br/>     p4 depot name
		/// <br/>     p4 depot -d [-f] name
		/// <br/>     p4 depot -o name
		/// <br/>     p4 depot -i
		/// <br/> 
		/// <br/> 	Create a new depot specification or edit an existing depot
		/// <br/> 	specification. The specification form is put into a temporary file
		/// <br/> 	and the editor (configured by the environment variable $P4EDITOR)
		/// <br/> 	is invoked.
		/// <br/> 
		/// <br/> 	The depot specification contains the following fields:
		/// <br/> 
		/// <br/> 	Depot:       The name of the depot.  This name cannot be the same as
		/// <br/> 		     any branch, client, or label name.
		/// <br/> 
		/// <br/> 	Owner:       The user who created this depot.
		/// <br/> 
		/// <br/> 	Date:        The date that this specification was last modified.
		/// <br/> 
		/// <br/> 	Description: A short description of the depot (optional).
		/// <br/> 
		/// <br/> 	Type:        One of the types: 'local', 'stream', 'remote', 'spec',
		/// <br/> 		     'archive', or 'unload'.
		/// <br/> 
		/// <br/> 		     A 'local' depot (the default) is managed directly by
		/// <br/> 		     the server and its files reside in the server's root
		/// <br/> 		     directory.
		/// <br/> 
		/// <br/> 		     A 'stream' depot is a local depot dedicated to the
		/// <br/> 		     storage of files in a stream.
		/// <br/> 
		/// <br/> 		     A 'remote' depot refers to files in another Perforce
		/// <br/> 		     server.
		/// <br/> 
		/// <br/> 		     A 'spec' depot automatically archives all edited forms
		/// <br/> 		     (branch, change, client, depot, group, job, jobspec,
		/// <br/> 		     protect, triggers, typemap, and user) in special,
		/// <br/> 		     read-only files.  The files are named:
		/// <br/> 		     //depotname/formtype/name[suffix].  Updates to jobs made
		/// <br/> 		     by the 'p4 change', 'p4 fix', and 'p4 submit' commands
		/// <br/> 		     are also saved, but other automatic updates such as
		/// <br/> 		     as access times or opened files (for changes) are not.
		/// <br/> 		     A server can contain only one 'spec' depot.
		/// <br/> 
		/// <br/> 		     A 'archive' depot defines a storage location to which
		/// <br/> 		     obsolete revisions may be relocated.
		/// <br/> 
		/// <br/> 		     An 'unload' depot defines a storage location to which
		/// <br/> 		     database records may be unloaded and from which they
		/// <br/> 		     may be reloaded.
		/// <br/> 
		/// <br/> 	Address:     For remote depots, the $P4PORT (connection address)
		/// <br/> 		     of the remote server.
		/// <br/> 
		/// <br/> 	Suffix:      For spec depots, the optional suffix to be used
		/// <br/> 		     for generated paths. The default is '.p4s'.
		/// <br/> 
		/// <br/> 	Map:         Path translation information, in the form of a file
		/// <br/> 		     pattern with a single ... in it.  For local depots,
		/// <br/> 		     this path is relative to the server's root directory
		/// <br/> 		     or to server.depot.root if it has been configured
		/// <br/> 		     (Example: depot/...).  For remote depots, this path
		/// <br/> 		     refers to the remote server's namespace
		/// <br/> 		     (Example: //depot/...).
		/// <br/> 
		/// <br/> 	SpecMap:     For spec depots, the optional description of which
		/// <br/> 	             specs should be saved, as one or more patterns.
		/// <br/> 
		/// <br/> 	The -d flag deletes the specified depot.  If any files reside in the
		/// <br/> 	depot, they must be removed with 'p4 obliterate' before deleting the
		/// <br/> 	depot. If any archive files remain in the depot directory, they may
		/// <br/> 	be referenced by lazy copies in other depots; use 'p4 snap' to break
		/// <br/> 	those linkages. Snap lazy copies prior to obliterating the old depot
		/// <br/> 	files to allow the obliterate command to remove any unreferenced
		/// <br/> 	archives from the depot directory. If the depot directory is not
		/// <br/> 	empty, you must specify the -f flag to delete the depot.
		/// <br/> 
		/// <br/> 	The -o flag writes the depot specification to standard output. The
		/// <br/> 	user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a depot specification from standard input. The
		/// <br/> 	user's editor is not invoked.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To update the streams depot named MobileApp:
        ///		<code> 
        ///		   	Depot d = Repository.GetDepot("MobileApp");
        ///	
        ///         // change description
        ///		   	d.Description = "Stream depot for Win8 phone apps";
        ///		   	
        ///		   	Depot MobileApp = Repository.UpdateDepot(d);
        ///		</code>
        /// </example>
        public Depot UpdateDepot(Depot depot)
		{
			return CreateDepot(depot, null);
		}

		/// <summary>
		/// Get the record for an existing depot from the repository.
		/// </summary>
		/// <param name="depot">Depot name</param>
		/// <param name="options">There are no valid flags to use when fetching an existing depot</param>
		/// <returns>The Depot object if depot was found, null if not</returns>
        /// <remarks>
        /// <br/> 
		/// <br/><b>p4 help depot</b>
		/// <br/> 
		/// <br/>     depot -- Create or edit a depot specification
		/// <br/> 
		/// <br/>     p4 depot name
		/// <br/>     p4 depot -d [-f] name
		/// <br/>     p4 depot -o name
		/// <br/>     p4 depot -i
		/// <br/> 
		/// <br/> 	Create a new depot specification or edit an existing depot
		/// <br/> 	specification. The specification form is put into a temporary file
		/// <br/> 	and the editor (configured by the environment variable $P4EDITOR)
		/// <br/> 	is invoked.
		/// <br/> 
		/// <br/> 	The depot specification contains the following fields:
		/// <br/> 
		/// <br/> 	Depot:       The name of the depot.  This name cannot be the same as
		/// <br/> 		     any branch, client, or label name.
		/// <br/> 
		/// <br/> 	Owner:       The user who created this depot.
		/// <br/> 
		/// <br/> 	Date:        The date that this specification was last modified.
		/// <br/> 
		/// <br/> 	Description: A short description of the depot (optional).
		/// <br/> 
		/// <br/> 	Type:        One of the types: 'local', 'stream', 'remote', 'spec',
		/// <br/> 		     'archive', or 'unload'.
		/// <br/> 
		/// <br/> 		     A 'local' depot (the default) is managed directly by
		/// <br/> 		     the server and its files reside in the server's root
		/// <br/> 		     directory.
		/// <br/> 
		/// <br/> 		     A 'stream' depot is a local depot dedicated to the
		/// <br/> 		     storage of files in a stream.
		/// <br/> 
		/// <br/> 		     A 'remote' depot refers to files in another Perforce
		/// <br/> 		     server.
		/// <br/> 
		/// <br/> 		     A 'spec' depot automatically archives all edited forms
		/// <br/> 		     (branch, change, client, depot, group, job, jobspec,
		/// <br/> 		     protect, triggers, typemap, and user) in special,
		/// <br/> 		     read-only files.  The files are named:
		/// <br/> 		     //depotname/formtype/name[suffix].  Updates to jobs made
		/// <br/> 		     by the 'p4 change', 'p4 fix', and 'p4 submit' commands
		/// <br/> 		     are also saved, but other automatic updates such as
		/// <br/> 		     as access times or opened files (for changes) are not.
		/// <br/> 		     A server can contain only one 'spec' depot.
		/// <br/> 
		/// <br/> 		     A 'archive' depot defines a storage location to which
		/// <br/> 		     obsolete revisions may be relocated.
		/// <br/> 
		/// <br/> 		     An 'unload' depot defines a storage location to which
		/// <br/> 		     database records may be unloaded and from which they
		/// <br/> 		     may be reloaded.
		/// <br/> 
		/// <br/> 	Address:     For remote depots, the $P4PORT (connection address)
		/// <br/> 		     of the remote server.
		/// <br/> 
		/// <br/> 	Suffix:      For spec depots, the optional suffix to be used
		/// <br/> 		     for generated paths. The default is '.p4s'.
		/// <br/> 
		/// <br/> 	Map:         Path translation information, in the form of a file
		/// <br/> 		     pattern with a single ... in it.  For local depots,
		/// <br/> 		     this path is relative to the server's root directory
		/// <br/> 		     or to server.depot.root if it has been configured
		/// <br/> 		     (Example: depot/...).  For remote depots, this path
		/// <br/> 		     refers to the remote server's namespace
		/// <br/> 		     (Example: //depot/...).
		/// <br/> 
		/// <br/> 	SpecMap:     For spec depots, the optional description of which
		/// <br/> 	             specs should be saved, as one or more patterns.
		/// <br/> 
		/// <br/> 	The -d flag deletes the specified depot.  If any files reside in the
		/// <br/> 	depot, they must be removed with 'p4 obliterate' before deleting the
		/// <br/> 	depot. If any archive files remain in the depot directory, they may
		/// <br/> 	be referenced by lazy copies in other depots; use 'p4 snap' to break
		/// <br/> 	those linkages. Snap lazy copies prior to obliterating the old depot
		/// <br/> 	files to allow the obliterate command to remove any unreferenced
		/// <br/> 	archives from the depot directory. If the depot directory is not
		/// <br/> 	empty, you must specify the -f flag to delete the depot.
		/// <br/> 
		/// <br/> 	The -o flag writes the depot specification to standard output. The
		/// <br/> 	user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a depot specification from standard input. The
		/// <br/> 	user's editor is not invoked.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To get the spec for a streams depot named MobileApp:
        ///		<code> 
        ///		   	Depot MobileApp = Repository.GetDepot("MobileApp", null);
        ///		</code>
        /// </example>
        /// <seealso cref="DepotCmdFlags"/>
        public Depot GetDepot(string depot, Options options)
		{
			if (depot == null)
			{
				throw new ArgumentNullException("depot");

			}
			P4Command cmd = new P4Command(this, "depot", true, depot);

			if (options == null)
			{
				options = new Options(DepotCmdFlags.Output);
			}
			if (options.ContainsKey("-o") == false)
			{
				options["-o"] = null;
			}

			P4CommandResult results = cmd.Run(options);
			if (results.Success)
			{
				if ((results.TaggedOutput == null) || (results.TaggedOutput.Count <= 0))
				{
					return null;
				}
				Depot value = new Depot();

				value.FromDepotCmdTaggedOutput(results.TaggedOutput[0]);

				return value;
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}
			return null;
		}

        /// <summary>
        /// Get the record for an existing depot from the repository.
        /// </summary>
        /// <param name="depot">Depot name</param>
        /// <returns>The Depot object if depot was found, null if not</returns>
        /// <remarks>
        /// <br/> 
		/// <br/><b>p4 help depot</b>
		/// <br/> 
		/// <br/>     depot -- Create or edit a depot specification
		/// <br/> 
		/// <br/>     p4 depot name
		/// <br/>     p4 depot -d [-f] name
		/// <br/>     p4 depot -o name
		/// <br/>     p4 depot -i
		/// <br/> 
		/// <br/> 	Create a new depot specification or edit an existing depot
		/// <br/> 	specification. The specification form is put into a temporary file
		/// <br/> 	and the editor (configured by the environment variable $P4EDITOR)
		/// <br/> 	is invoked.
		/// <br/> 
		/// <br/> 	The depot specification contains the following fields:
		/// <br/> 
		/// <br/> 	Depot:       The name of the depot.  This name cannot be the same as
		/// <br/> 		     any branch, client, or label name.
		/// <br/> 
		/// <br/> 	Owner:       The user who created this depot.
		/// <br/> 
		/// <br/> 	Date:        The date that this specification was last modified.
		/// <br/> 
		/// <br/> 	Description: A short description of the depot (optional).
		/// <br/> 
		/// <br/> 	Type:        One of the types: 'local', 'stream', 'remote', 'spec',
		/// <br/> 		     'archive', or 'unload'.
		/// <br/> 
		/// <br/> 		     A 'local' depot (the default) is managed directly by
		/// <br/> 		     the server and its files reside in the server's root
		/// <br/> 		     directory.
		/// <br/> 
		/// <br/> 		     A 'stream' depot is a local depot dedicated to the
		/// <br/> 		     storage of files in a stream.
		/// <br/> 
		/// <br/> 		     A 'remote' depot refers to files in another Perforce
		/// <br/> 		     server.
		/// <br/> 
		/// <br/> 		     A 'spec' depot automatically archives all edited forms
		/// <br/> 		     (branch, change, client, depot, group, job, jobspec,
		/// <br/> 		     protect, triggers, typemap, and user) in special,
		/// <br/> 		     read-only files.  The files are named:
		/// <br/> 		     //depotname/formtype/name[suffix].  Updates to jobs made
		/// <br/> 		     by the 'p4 change', 'p4 fix', and 'p4 submit' commands
		/// <br/> 		     are also saved, but other automatic updates such as
		/// <br/> 		     as access times or opened files (for changes) are not.
		/// <br/> 		     A server can contain only one 'spec' depot.
		/// <br/> 
		/// <br/> 		     A 'archive' depot defines a storage location to which
		/// <br/> 		     obsolete revisions may be relocated.
		/// <br/> 
		/// <br/> 		     An 'unload' depot defines a storage location to which
		/// <br/> 		     database records may be unloaded and from which they
		/// <br/> 		     may be reloaded.
		/// <br/> 
		/// <br/> 	Address:     For remote depots, the $P4PORT (connection address)
		/// <br/> 		     of the remote server.
		/// <br/> 
		/// <br/> 	Suffix:      For spec depots, the optional suffix to be used
		/// <br/> 		     for generated paths. The default is '.p4s'.
		/// <br/> 
		/// <br/> 	Map:         Path translation information, in the form of a file
		/// <br/> 		     pattern with a single ... in it.  For local depots,
		/// <br/> 		     this path is relative to the server's root directory
		/// <br/> 		     or to server.depot.root if it has been configured
		/// <br/> 		     (Example: depot/...).  For remote depots, this path
		/// <br/> 		     refers to the remote server's namespace
		/// <br/> 		     (Example: //depot/...).
		/// <br/> 
		/// <br/> 	SpecMap:     For spec depots, the optional description of which
		/// <br/> 	             specs should be saved, as one or more patterns.
		/// <br/> 
		/// <br/> 	The -d flag deletes the specified depot.  If any files reside in the
		/// <br/> 	depot, they must be removed with 'p4 obliterate' before deleting the
		/// <br/> 	depot. If any archive files remain in the depot directory, they may
		/// <br/> 	be referenced by lazy copies in other depots; use 'p4 snap' to break
		/// <br/> 	those linkages. Snap lazy copies prior to obliterating the old depot
		/// <br/> 	files to allow the obliterate command to remove any unreferenced
		/// <br/> 	archives from the depot directory. If the depot directory is not
		/// <br/> 	empty, you must specify the -f flag to delete the depot.
		/// <br/> 
		/// <br/> 	The -o flag writes the depot specification to standard output. The
		/// <br/> 	user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a depot specification from standard input. The
		/// <br/> 	user's editor is not invoked.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To get the spec for a streams depot named MobileApp:
        ///		<code> 
        ///		   	Depot MobileApp = Repository.GetDepot("MobileApp");
        ///		</code>
        /// </example>
        /// <seealso cref="DepotCmdFlags"/>
        public Depot GetDepot(string depot)
		{
			return GetDepot(depot, null);
		}

		/// <summary>
		/// Get a list of depots from the repository
		/// </summary>
		/// <returns>A list containing the matching depots</returns>
		/// <remarks>
		/// <br/><b>p4 help depots</b>
		/// <br/> 
		/// <br/>     depots -- Lists defined depots
		/// <br/> 
		/// <br/>     p4 depots
		/// <br/> 
		/// <br/> 	Lists all depots defined in the server.
		/// <br/> 	Depots takes no arguments.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To list the depots on the server:
        ///		<code> 
        ///			IList&lt;Depot&gt; depots = Repository.GetDepots();
        ///		</code>
        /// </example>
		public IList<Depot> GetDepots()
		{
			P4Command cmd = new P4Command(this, "depots", true);
			

			P4CommandResult results = cmd.Run();
			if (results.Success)
			{
				if ((results.TaggedOutput == null) || (results.TaggedOutput.Count <= 0))
				{
					return null;
				}

                bool dst_mismatch = false;
                string offset = string.Empty;

                if (Server != null && Server.Metadata != null)
                {
                    offset = Server.Metadata.DateTimeOffset;
                    dst_mismatch = FormBase.DSTMismatch(Server.Metadata);
                }

				List<Depot> value = new List<Depot>();
				foreach (TaggedObject obj in results.TaggedOutput)
				{
					Depot depot = new Depot();
					depot.FromDepotsCmdTaggedOutput(obj,offset,dst_mismatch);
					value.Add(depot);
				}
				return value;
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}
			return null;
		}

        public IList<Depot> GetDepots(Options options)
        {
            P4Command cmd = new P4Command(this, "depots", true);


            P4CommandResult results = cmd.Run(options);
            if (results.Success)
            {
                if ((results.TaggedOutput == null) || (results.TaggedOutput.Count <= 0))
                {
                    return null;
                }

                bool dst_mismatch = false;
                string offset = string.Empty;

                if (Server != null && Server.Metadata != null)
                {
                    offset = Server.Metadata.DateTimeOffset;
                    dst_mismatch = FormBase.DSTMismatch(Server.Metadata);
                }

                List<Depot> value = new List<Depot>();
                foreach (TaggedObject obj in results.TaggedOutput)
                {
                    Depot depot = new Depot();
                    depot.FromDepotsCmdTaggedOutput(obj, offset, dst_mismatch);
                    value.Add(depot);
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
        /// Delete a depot from the repository
        /// </summary>
        /// <param name="depot">The depot to be deleted</param>
        /// <param name="options">Only the '-d' flag is valid when deleting an existing depot</param>
        /// <remarks>
        /// <br/> 
        /// <br/><b>p4 help depot</b>
        /// <br/> 
        /// <br/>     depot -- Create or edit a depot specification
        /// <br/> 
        /// <br/>     p4 depot name
        /// <br/>     p4 depot -d [-f] name
        /// <br/>     p4 depot -o name
        /// <br/>     p4 depot -i
        /// <br/> 
        /// <br/> 	Create a new depot specification or edit an existing depot
        /// <br/> 	specification. The specification form is put into a temporary file
        /// <br/> 	and the editor (configured by the environment variable $P4EDITOR)
        /// <br/> 	is invoked.
        /// <br/> 
        /// <br/> 	The depot specification contains the following fields:
        /// <br/> 
        /// <br/> 	Depot:       The name of the depot.  This name cannot be the same as
        /// <br/> 		     any branch, client, or label name.
        /// <br/> 
        /// <br/> 	Owner:       The user who created this depot.
        /// <br/> 
        /// <br/> 	Date:        The date that this specification was last modified.
        /// <br/> 
        /// <br/> 	Description: A short description of the depot (optional).
        /// <br/> 
        /// <br/> 	Type:        One of the types: 'local', 'stream', 'remote', 'spec',
        /// <br/> 		     'archive', or 'unload'.
        /// <br/> 
        /// <br/> 		     A 'local' depot (the default) is managed directly by
        /// <br/> 		     the server and its files reside in the server's root
        /// <br/> 		     directory.
        /// <br/> 
        /// <br/> 		     A 'stream' depot is a local depot dedicated to the
        /// <br/> 		     storage of files in a stream.
        /// <br/> 
        /// <br/> 		     A 'remote' depot refers to files in another Perforce
        /// <br/> 		     server.
        /// <br/> 
        /// <br/> 		     A 'spec' depot automatically archives all edited forms
        /// <br/> 		     (branch, change, client, depot, group, job, jobspec,
        /// <br/> 		     protect, triggers, typemap, and user) in special,
        /// <br/> 		     read-only files.  The files are named:
        /// <br/> 		     //depotname/formtype/name[suffix].  Updates to jobs made
        /// <br/> 		     by the 'p4 change', 'p4 fix', and 'p4 submit' commands
        /// <br/> 		     are also saved, but other automatic updates such as
        /// <br/> 		     as access times or opened files (for changes) are not.
        /// <br/> 		     A server can contain only one 'spec' depot.
        /// <br/> 
        /// <br/> 		     A 'archive' depot defines a storage location to which
        /// <br/> 		     obsolete revisions may be relocated.
        /// <br/> 
        /// <br/> 		     An 'unload' depot defines a storage location to which
        /// <br/> 		     database records may be unloaded and from which they
        /// <br/> 		     may be reloaded.
        /// <br/> 
        /// <br/> 	Address:     For remote depots, the $P4PORT (connection address)
        /// <br/> 		     of the remote server.
        /// <br/> 
        /// <br/> 	Suffix:      For spec depots, the optional suffix to be used
        /// <br/> 		     for generated paths. The default is '.p4s'.
        /// <br/> 
        /// <br/> 	Map:         Path translation information, in the form of a file
        /// <br/> 		     pattern with a single ... in it.  For local depots,
        /// <br/> 		     this path is relative to the server's root directory
        /// <br/> 		     or to server.depot.root if it has been configured
        /// <br/> 		     (Example: depot/...).  For remote depots, this path
        /// <br/> 		     refers to the remote server's namespace
        /// <br/> 		     (Example: //depot/...).
        /// <br/> 
        /// <br/> 	SpecMap:     For spec depots, the optional description of which
        /// <br/> 	             specs should be saved, as one or more patterns.
        /// <br/> 
        /// <br/> 	The -d flag deletes the specified depot.  If any files reside in the
        /// <br/> 	depot, they must be removed with 'p4 obliterate' before deleting the
        /// <br/> 	depot. If any archive files remain in the depot directory, they may
        /// <br/> 	be referenced by lazy copies in other depots; use 'p4 snap' to break
        /// <br/> 	those linkages. Snap lazy copies prior to obliterating the old depot
        /// <br/> 	files to allow the obliterate command to remove any unreferenced
        /// <br/> 	archives from the depot directory. If the depot directory is not
        /// <br/> 	empty, you must specify the -f flag to delete the depot.
        /// <br/> 
        /// <br/> 	The -o flag writes the depot specification to standard output. The
        /// <br/> 	user's editor is not invoked.
        /// <br/> 
        /// <br/> 	The -i flag reads a depot specification from standard input. The
        /// <br/> 	user's editor is not invoked.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        /// <example>
        ///		To delete a streams depot named MobileApp:
        ///		<code> 
        ///		   	Depot d = Repository.GetDepot("MobileApp");
        ///		   			   	                  
        ///		   	Repository.DeleteDepot(d, null);
        ///		</code>
        /// </example>
        /// <seealso cref="DepotCmdFlags"/>
        public void DeleteDepot(Depot depot, Options options)
		{
			if (depot == null)
			{
				throw new ArgumentNullException("depot");

			}
			P4Command cmd = new P4Command(this, "depot", true, depot.Id);

			if (options == null)
			{
				options = new Options(DepotCmdFlags.Delete);
			}
			if (options.ContainsKey("-d") == false)
			{
				options["-d"] = null;
			}
			
			P4CommandResult results = cmd.Run(options);
			if (results.Success == false)
			{
				P4Exception.Throw(results.ErrorList);
			}
		}
	}
}
