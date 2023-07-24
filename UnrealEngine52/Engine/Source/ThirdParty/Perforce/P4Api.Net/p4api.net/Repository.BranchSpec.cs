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
 * Name		: Repository.BranchSpec.cs
 *
 * Author	: wjb
 *
 * Description	: BranchSpec operations for the Repository.
 *
 ******************************************************************************/
using System;
using System.Collections.Generic;

namespace Perforce.P4
{
	public partial class Repository
	{
		/// <summary>
		/// Create a new branch in the repository.
		/// </summary>
		/// <param name="branch">Branch specification for the new branch</param>
		/// <param name="options">The '-i' flag is required when creating a new branch </param>
        /// <returns>The Branch object if new branch was created, null if creation failed</returns>
        /// <remarks>
		/// <br/>
		/// <br/><b>p4 help branch</b>
		/// <br/> 
		/// <br/>     branch -- Create, modify, or delete a branch view specification
		/// <br/> 
		/// <br/>     p4 branch [-f] name
		/// <br/>     p4 branch -d [-f] name
		/// <br/>     p4 branch [ -S stream ] [ -P parent ] -o name
		/// <br/>     p4 branch -i [-f]
		/// <br/> 
		/// <br/> 	A branch specification ('spec') is a named, user-defined mapping of
		/// <br/> 	depot files to depot files. It can be used with most of the commands
		/// <br/> 	that operate on two sets of files ('copy', 'merge', 'integrate',
		/// <br/> 	'diff2', etc.)
		/// <br/> 
		/// <br/> 	Creating a branch spec does not branch files.  To branch files, use
		/// <br/> 	'p4 copy', with or without a branch spec.
		/// <br/> 	
		/// <br/> 	The 'branch' command puts the branch spec into a temporary file and
		/// <br/> 	invokes the editor configured by the environment variable $P4EDITOR.
		/// <br/> 	Saving the file creates or modifies the branch spec.
		/// <br/> 
		/// <br/> 	The branch spec contains the following fields:
		/// <br/> 
		/// <br/> 	Branch:      The branch spec name (read only).
		/// <br/> 
		/// <br/> 	Owner:       The user who created this branch spec. Can be changed.
		/// <br/> 
		/// <br/> 	Update:      The date this branch spec was last modified.
		/// <br/> 
		/// <br/> 	Access:      The date of the last command used with this spec.
		/// <br/> 
		/// <br/> 	Description: A description of the branch spec (optional).
		/// <br/> 
		/// <br/> 	Options:     Flags to change the branch spec behavior. The defaults
		/// <br/> 		     are marked with *.
		/// <br/> 
		/// <br/> 		locked   	Permits only the owner to change the spec.
		/// <br/> 		unlocked *	Prevents the branch spec from being deleted.
		/// <br/> 
		/// <br/> 	View:        Lines mapping of one view of depot files to another.
		/// <br/> 		     Both the left and right-hand sides of the mappings refer
		/// <br/> 		     to the depot namespace.  See 'p4 help views' for more on
		/// <br/> 		     view syntax.
		/// <br/> 
		/// <br/> 	New branch specs are created with a default view that maps all depot
		/// <br/> 	files to themselves.  This view must be changed before the branch
		/// <br/> 	spec can be saved.
		/// <br/> 
		/// <br/> 	The -d flag deletes the named branch spec.
		/// <br/> 
		/// <br/> 	The -o flag writes the branch spec to standard output. The user's
		/// <br/> 	editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag causes a branch spec to be read from the standard input.
		/// <br/> 	The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -f flag enables a user with 'admin' privilege to delete the spec
		/// <br/> 	or set the 'last modified' date.  By default, specs can be deleted
		/// <br/> 	only by their owner.
		/// <br/> 
		/// <br/> 	A branch spec can also be used to expose the internally generated
		/// <br/> 	mapping of a stream to its parent. (See 'p4 help stream' and 'p4
		/// <br/> 	help streamintro'.)
		/// <br/> 
		/// <br/> 	The -S stream flag will expose the internally generated mapping.
		/// <br/> 	The -P flag may be used with -S to treat the stream as if it were a
		/// <br/> 	child of a different parent. The -o flag is required with -S.
		/// <br/> 
		/// <br/> 
		/// </remarks>
		/// <example>
		///		To create a branch spec [-i]:
		///		<code>
		///	
		///			BranchSpec newBranchSpec = new BranchSpec();
		///			newBranchSpec.Id = "newBranchSpec";
		///			newBranchSpec.Owner = "admin";
		///			newBranchSpec.Description = " created by perforce";
		///			newBranchSpec.ViewMap = new ViewMap();
		///			string v0 = "//depot/main/... //depot/rel1/...";
		///			string v1 = "//depot/main/... //depot/rel2/...";
		///			string v2 = "//depot/dev/... //depot/main/...";
		///			newBranchSpec.ViewMap.Add(v0);
		///			newBranchSpec.ViewMap.Add(v1);
		///			newBranchSpec.ViewMap.Add(v2);
		///			Options opts = new Options(BranchSpecCmdFlags.Input);
		///			_repository.CreateBranchSpec(newBranchSpec, opts);
		///	
		///		</code>
		/// </example>
		/// <seealso cref="BranchSpecCmdFlags"/>
        public BranchSpec CreateBranchSpec(BranchSpec branch, Options options)
		{
			if (branch == null)
			{
				throw new ArgumentNullException("branch");

			}
			P4Command cmd = new P4Command(this, "branch", true);

			cmd.DataSet = branch.ToString();

			if (options == null)
			{
				options = new Options((BranchSpecCmdFlags.Input), null, null);
			}
			if (options.ContainsKey("-i") == false)
			{
				options["-i"] = null;
			}

			P4CommandResult results = cmd.Run(options);
			if (results.Success)
			{
				return branch;
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}
			return null;
		}
		/// <summary>
		/// Create a new branch in the repository.
		/// </summary>
		/// <param name="branch">Branch specification for the new branch</param>
		/// <returns>The Branch object if new branch was created, null if creation failed</returns>
		/// <example>
		///		To create a basic branch spec:
		///		<code> 
		///	
		///		BranchSpec newBranchSpec = new BranchSpec();
		///		newBranchSpec.Id = "newBranchSpec";
		///		newBranchSpec.ViewMap = new ViewMap();
		///		string v0 = "//depot/main/... //depot/rel1/...";
		///		newBranchSpec.ViewMap.Add(v0);
		///		_repository.CreateBranchSpec(newBranchSpec);
		///	
		///		</code>
		/// </example>
		public BranchSpec CreateBranchSpec(BranchSpec branch)
		{
			return CreateBranchSpec(branch, null);
		}
		/// <summary>
		/// Update the record for a branch in the repository
		/// </summary>
		/// <param name="branch">Branch specification for the branch being updated</param>
		/// <returns>The Branch object if new depot was saved, null if creation failed</returns>
		/// <example>
		///		To append a view to an existing branch spec:
		///		<code> 
		///	
		///			BranchSpec updateBranchSpec = _repository.GetBranchSpec("newBranchSpec");
		///			string v0 = "\"//depot/main/a file with spaces.txt\" \"//depot/rel1/a file with spaces.txt\"";
		///			updateBranchSpec.ViewMap.Add(v0);        
		///			_repository.UpdateBranchSpec(updateBranchSpec);
		///	
		///		</code>
		///		To lock a branch spec:
		///		<code> 
		///	
		///			BranchSpec updateBranchSpec = _repository.GetBranchSpec("newBranchSpec");
		///			updateBranchSpec.Locked = true;
		///			_repository.UpdateBranchSpec(updateBranchSpec);
		///	
		///		</code>    
		/// </example>
		public BranchSpec UpdateBranchSpec(BranchSpec branch)
		{
			return CreateBranchSpec(branch, null);
		}
        /// <summary>
        /// Get the record for an existing branch from the repository.
        /// </summary>
        /// <param name="branch">Branch name</param>
		/// <param name="stream">Stream name</param>
		/// <param name="parent">Parent stream</param>
        /// <param name="options">Options</param>
        /// <returns>The Branch object if branch was found, null if creation failed</returns>
        /// <example>
        ///		To get a branch spec:
        ///		<code> 
        ///	
        ///		BranchSpec getBranchSpec = _repository.GetBranchSpec("newBranchSpec");
        ///	
        ///		</code>
        /// </example>
        public BranchSpec GetBranchSpec(string branch, string stream, string parent, Options options)
		{
			if (branch == null)
			{
				throw new ArgumentNullException("branch");

			}
			P4Command cmd = new P4Command(this, "branch", true, branch);

			if (options == null)
			{
				options = new Options((BranchSpecCmdFlags.Output), stream, parent);
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

				BranchSpec value = new BranchSpec();
				value.FromBranchSpecCmdTaggedOutput(results.TaggedOutput[0]);

				return value;
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}
			return null;
		}

        /// <summary>
        /// Get a BranchSpec from the branch name
        /// </summary>
        /// <param name="branch">branch name</param>
        /// <returns>BranchSpec</returns>
		public BranchSpec GetBranchSpec(string branch)
		{
			return GetBranchSpec(branch, null, null, null);
		}

		/// <summary>
		/// Get a list of branches from the repository
		/// </summary>
		/// <returns>A list containing the matching branches</returns>
		/// <remarks>
		/// <br/><b>p4 help branches</b>
		/// <br/> 
		/// <br/>     branches -- Display list of branch specifications
		/// <br/> 
		/// <br/>     p4 branches [-t] [-u user] [[-e|-E] nameFilter -m max]
		/// <br/> 
		/// <br/> 	Lists branch specifications. (See 'p4 help branch'.) 
		/// <br/> 
		/// <br/> 	The -t flag displays the time as well as the date.
		/// <br/> 
		/// <br/> 	The -u user flag lists branch specs owned by the specified user.
		/// <br/> 
		/// <br/> 	The -e nameFilter flag lists branch specs with a name that matches
		/// <br/> 	the nameFilter pattern, for example: -e 'svr-dev-rel*'. The -e flag
		/// <br/> 	uses the server's normal case-sensitivity rules. The -E flag makes
		/// <br/> 	the matching case-insensitive, even on a case-sensitive server.
		/// <br/> 
		/// <br/> 	The -m max flag limits output to the specified number of branch specs.
		/// <br/> 
		/// <br/> 
		/// </remarks>
		/// <example>
		///		To get all branches and include timestamps [-t] (WARNING, will fetch all branches from the repository):
		///		<code> 
		///	
		///			Options opts = new Options(BranchSpecsCmdFlags.Time, "", "", -1);
		/// 		IList&#60;Branch&#62; branches = _repository.GetBranchSpecs(opts);
		///			
		///		</code>
		///		To get branches owned by 'Bob' [-u]:
		///		<code> 
		///			
		///			Options opts = new Options(BranchSpecsCmdFlags.None, "Bob", "", -1);
		///			IList&#60;Branch&#62; branches = _repository.GetBranchSpecs(opts);
		///			
		///		</code>
		///		To get the first 10 branches that start with the capital letter 'A' [-m] [-e]:
		///		<code> 
		///			
		///			Options opts = new Options(BranchSpecsCmdFlags.None, "", "A*", 10);
		///			IList&#60;Branch&#62; branches = _repository.GetBranchSpecs(opts);
		///			
		///		</code>
		///		To get the first 10 branches that start with the letter 'A' case insensitive [-m] [-E]:
		///		<code> 
		///			
		///			Options opts = new Options(BranchSpecsCmdFlags.IgnoreCase, "", "A*", 10);
		///			IList&#60;Branch&#62; branches = _repository.GetBranchSpecs(opts);
		///			
		///		</code>
		/// </example>
		/// <seealso cref="BranchSpecsCmdFlags"/>
		public IList<BranchSpec> GetBranchSpecs(Options options)
		{
			P4Command cmd = new P4Command(this, "branches", true);


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

				List<BranchSpec> value = new List<BranchSpec>();
				foreach (TaggedObject obj in results.TaggedOutput)
				{
					BranchSpec branch = new BranchSpec();
					branch.FromBranchSpecsCmdTaggedOutput(obj,offset,dst_mismatch);
					value.Add(branch);
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
		/// Delete a branch from the repository
		/// </summary>
		/// <param name="branch">The branch to be deleted</param>
		/// <param name="options">The '-f' and '-d' flags are valid when deleting an
		/// existing branch</param>
		/// <example>
		///		To delete a branch spec owned by you [-d implied]:
		///		<code> 
		///						
		///         BranchSpec deleteBranchSpec = new BranchSpec();
		///         deleteBranchSpec.Id = "newBranchSpec";
		/// 		_repository.DeleteBranchSpec(deleteBranchSpec, null);
		///			
		///		</code>
		///		To delete a branch owned by someone other than you [-d implied] [-f requires admin privileges]:
		///		<code> 
		///						
		///         BranchSpec deleteBranchSpec = new BranchSpec();
		///         deleteBranchSpec.Id = "newBranchSpec";
		///			Options opts = new Options(BranchSpecsCmdFlags.Force);
		/// 		_repository.DeleteBranchSpec(deleteBranchSpec, opts);
		///			
		///		</code>
		/// </example>
		/// <seealso cref="BranchSpecCmdFlags"/>
		public void DeleteBranchSpec(BranchSpec branch, Options options)
		{
			if (branch == null)
			{
				throw new ArgumentNullException("branch");

			}
			P4Command cmd = new P4Command(this, "branch", true, branch.Id);

			if (options == null)
			{
				options = new Options(BranchSpecCmdFlags.Delete, null, null);
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
