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
 * Name		: Repository.Changelist.cs
 *
 * Author	: dbb
 *
 * Description	:Changelist operations for the repository object
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
		internal Changelist SaveChangelist(Changelist change, Options options = null)
		{
			if (change == null)
			{
				throw new ArgumentNullException("change");
			}
			
            P4Command cmd = null;
			cmd = new P4Command(this, "change", true);
			cmd.DataSet = change.ToString();

		    if (options == null)
			{
				options = new Options();
			}
			options["-i"] = null;

			P4CommandResult results = cmd.Run(options);
			if (results.Success)
			{
				// If this a new change that was saved, we need  to parse out the new changelist Id
				if (change.Id == -1)
				{
					string[] words = results.InfoOutput[0].Message.Split(' ');

					int newId = -1;
					if (int.TryParse(words[1], out newId))
					{
						Changelist newChange = GetChangelist(newId);
						return newChange;
					}
				}
				return GetChangelist(change.Id);
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}
			return null;
		}

		/// <summary>
		/// Create a new empty changelist object using a blank spec returned
		///   by the server
		/// </summary>
		/// <returns>New changelist</returns>
		/// <remarks>Guarantees that any custom values are set
        /// <br/>
		/// <br/><b>p4 help change</b>
		/// <br/> 
		/// <br/>     change -- Create or edit a changelist description
		/// <br/>     changelist -- synonym for 'change'
		/// <br/> 
		/// <br/>     p4 change [-s] [-f | -u] [[-O] changelist#]
		/// <br/>     p4 change -d [-f -s -O] changelist#
		/// <br/>     p4 change -o [-s] [-f] [[-O] changelist#]
		/// <br/>     p4 change -i [-s] [-f | -u] 
		/// <br/>     p4 change -t restricted | public [-U user] [-f | -u | -O] changelist#
		/// <br/>     p4 change -U user [-t restricted | public] [-f] changelist#
		/// <br/> 
		/// <br/> 	'p4 change' creates and edits changelists and their descriptions.
		/// <br/> 	With no argument, 'p4 change' creates a new changelist.  If a
		/// <br/> 	changelist number is specified, 'p4 change' edits an existing
		/// <br/> 	pending changelist.  In both cases, the changelist specification
		/// <br/> 	is placed into a form and the user's editor is invoked.
		/// <br/> 
		/// <br/> 	The -d flag deletes a pending changelist, if it has no opened files
		/// <br/> 	and no pending fixes associated with it.  Use 'p4 opened -a' to
		/// <br/> 	report on opened files and 'p4 reopen' to move them to another
		/// <br/> 	changelist.  Use 'p4 fixes -c changelist#' to report on pending
		/// <br/> 	fixes and 'p4 fix -d -c changelist# jobs...' to delete pending
		/// <br/> 	fixes. The changelist can be deleted only by the user and client
		/// <br/> 	who created it, or by a user with 'admin' privilege using the -f
		/// <br/> 	flag.
		/// <br/> 
		/// <br/> 	The -o flag writes the changelist specification to the standard
		/// <br/> 	output.  The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a changelist specification from the standard
		/// <br/> 	input.  The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -f flag forces the update or deletion of other users' pending
		/// <br/> 	changelists.  -f can also force the deletion of submitted changelists
		/// <br/> 	after they have been emptied of files using 'p4 obliterate'.  By
		/// <br/> 	default, submitted changelists cannot be changed.  The -f flag can
		/// <br/> 	also force display of the 'Description' field in a restricted
		/// <br/> 	changelist. Finally the -f flag can force changing the 'User' of
		/// <br/> 	an empty pending change via -U. The -f flag requires 'admin'
		/// <br/> 	access granted by 'p4 protect'.  The -f and -u flags are mutually
		/// <br/> 	exclusive.
		/// <br/> 
		/// <br/> 	The -u flag can force the update of a submitted change by the owner
		/// <br/> 	of the change. Only the Jobs, Type, and Description fields can be
		/// <br/> 	changed	using the -u flag. The -f and -u flags cannot be used in
		/// <br/> 	the same change command.
		/// <br/> 
		/// <br/> 	The -s flag extends the list of jobs to include the fix status
		/// <br/> 	for each job.  On new changelists, the fix status begins as the
		/// <br/> 	special status 'ignore', which, if left unchanged simply excludes
		/// <br/> 	the job from those being fixed.  Otherwise, the fix status, like
		/// <br/> 	that applied with 'p4 fix -s', becomes the job's status when
		/// <br/> 	the changelist is committed.  Note that this option exists
		/// <br/> 	to support integration with external defect trackers.
		/// <br/> 
		/// <br/> 	The -O flag specifies that the changelist number is the original
		/// <br/> 	number of a changelist which was renamed on submit.
		/// <br/> 
		/// <br/> 	The -U flag changes the 'User' of an empty pending change to the
		/// <br/> 	specified user. The user field can only be changed using this flag
		/// <br/> 	by the user who created the change, or by a user with 'admin'
		/// <br/> 	privilege using the -f flag. This option is useful for running
		/// <br/> 	in a trigger or script.
		/// <br/> 
		/// <br/> 	The -t flag changes the 'Type' of the change to 'restricted'
		/// <br/> 	or 'public' without displaying the change form. This option is
		/// <br/> 	useful for running in a trigger or script.
		/// <br/> 
		/// <br/> 	The 'Type' field can be used to hide the change or its description
		/// <br/> 	from users. Valid values for this field are 'public' (default), and
		/// <br/> 	'restricted'. A shelved or committed change that is 'restricted' is
		/// <br/> 	accessible only to users who own the change or have 'list' permission
		/// <br/> 	to at least one file in the change.  A pending (not shelved)
		/// <br/> 	restricted change is only accessible to its owner.  Public changes
		/// <br/> 	are accessible to all users. This setting affects the output of the
		/// <br/> 	'p4 change', 'p4 changes', and 'p4 describe' commands. Note that
		/// <br/> 	the '-S' flag is required with 'p4 describe' for the command to
		/// <br/> 	enforce shelved	rather than pending restricted changelist rules.
		/// <br/> 
		/// <br/> 	If a user is not permitted to have access to a restricted change,
		/// <br/> 	The 'Description' text is replaced with a 'no permission' message
		/// <br/> 	(see 'Type' field). Users with admin permission can override the
		/// <br/> 	restriction using the -f flag.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To create a new empty changelist object:
        ///		<code> 
        ///		    Changelist c = Repository.NewChangelist();
        ///		</code>
        /// </example>
        public Changelist NewChangelist()
		{
			Changelist c = new Changelist();

			P4Command cmd = null;

			cmd = new P4Command(this, "change", true);

			Options	options = new Options();
			options["-o"] = null;

			P4CommandResult results = cmd.Run(options);

            bool dst_mismatch = false;
            string offset = string.Empty;

            if (Server != null && Server.Metadata != null)
            {
                offset = Server.Metadata.DateTimeOffset;
                dst_mismatch = FormBase.DSTMismatch(Server.Metadata);
            }

			if (results.Success)
			{
				c.FromChangeCmdTaggedOutput(results.TaggedOutput[0],offset,dst_mismatch);
				return c;
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}
			return null;
		}

		/// <summary>
		/// Create a new changelist in the repository.
		/// </summary>
		/// <param name="change">Changelist specification for the new changelist</param>
		/// <param name="options">'-s', '-f', -u flags are valid when creating a new changelist</param>
		/// <returns>The Changelist object if new user was created, null if creation failed</returns>
		/// <remarks> The '-i' flag is added if not specified by the caller		
		/// <br/>
		/// <br/><b>p4 help change</b>
		/// <br/> 
		/// <br/>     change -- Create or edit a changelist description
		/// <br/>     changelist -- synonym for 'change'
		/// <br/> 
		/// <br/>     p4 change [-s] [-f | -u] [[-O] changelist#]
		/// <br/>     p4 change -d [-f -s -O] changelist#
		/// <br/>     p4 change -o [-s] [-f] [[-O] changelist#]
		/// <br/>     p4 change -i [-s] [-f | -u] 
		/// <br/>     p4 change -t restricted | public [-U user] [-f | -u | -O] changelist#
		/// <br/>     p4 change -U user [-t restricted | public] [-f] changelist#
		/// <br/> 
		/// <br/> 	'p4 change' creates and edits changelists and their descriptions.
		/// <br/> 	With no argument, 'p4 change' creates a new changelist.  If a
		/// <br/> 	changelist number is specified, 'p4 change' edits an existing
		/// <br/> 	pending changelist.  In both cases, the changelist specification
		/// <br/> 	is placed into a form and the user's editor is invoked.
		/// <br/> 
		/// <br/> 	The -d flag deletes a pending changelist, if it has no opened files
		/// <br/> 	and no pending fixes associated with it.  Use 'p4 opened -a' to
		/// <br/> 	report on opened files and 'p4 reopen' to move them to another
		/// <br/> 	changelist.  Use 'p4 fixes -c changelist#' to report on pending
		/// <br/> 	fixes and 'p4 fix -d -c changelist# jobs...' to delete pending
		/// <br/> 	fixes. The changelist can be deleted only by the user and client
		/// <br/> 	who created it, or by a user with 'admin' privilege using the -f
		/// <br/> 	flag.
		/// <br/> 
		/// <br/> 	The -o flag writes the changelist specification to the standard
		/// <br/> 	output.  The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a changelist specification from the standard
		/// <br/> 	input.  The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -f flag forces the update or deletion of other users' pending
		/// <br/> 	changelists.  -f can also force the deletion of submitted changelists
		/// <br/> 	after they have been emptied of files using 'p4 obliterate'.  By
		/// <br/> 	default, submitted changelists cannot be changed.  The -f flag can
		/// <br/> 	also force display of the 'Description' field in a restricted
		/// <br/> 	changelist. Finally the -f flag can force changing the 'User' of
		/// <br/> 	an empty pending change via -U. The -f flag requires 'admin'
		/// <br/> 	access granted by 'p4 protect'.  The -f and -u flags are mutually
		/// <br/> 	exclusive.
		/// <br/> 
		/// <br/> 	The -u flag can force the update of a submitted change by the owner
		/// <br/> 	of the change. Only the Jobs, Type, and Description fields can be
		/// <br/> 	changed	using the -u flag. The -f and -u flags cannot be used in
		/// <br/> 	the same change command.
		/// <br/> 
		/// <br/> 	The -s flag extends the list of jobs to include the fix status
		/// <br/> 	for each job.  On new changelists, the fix status begins as the
		/// <br/> 	special status 'ignore', which, if left unchanged simply excludes
		/// <br/> 	the job from those being fixed.  Otherwise, the fix status, like
		/// <br/> 	that applied with 'p4 fix -s', becomes the job's status when
		/// <br/> 	the changelist is committed.  Note that this option exists
		/// <br/> 	to support integration with external defect trackers.
		/// <br/> 
		/// <br/> 	The -O flag specifies that the changelist number is the original
		/// <br/> 	number of a changelist which was renamed on submit.
		/// <br/> 
		/// <br/> 	The -U flag changes the 'User' of an empty pending change to the
		/// <br/> 	specified user. The user field can only be changed using this flag
		/// <br/> 	by the user who created the change, or by a user with 'admin'
		/// <br/> 	privilege using the -f flag. This option is useful for running
		/// <br/> 	in a trigger or script.
		/// <br/> 
		/// <br/> 	The -t flag changes the 'Type' of the change to 'restricted'
		/// <br/> 	or 'public' without displaying the change form. This option is
		/// <br/> 	useful for running in a trigger or script.
		/// <br/> 
		/// <br/> 	The 'Type' field can be used to hide the change or its description
		/// <br/> 	from users. Valid values for this field are 'public' (default), and
		/// <br/> 	'restricted'. A shelved or committed change that is 'restricted' is
		/// <br/> 	accessible only to users who own the change or have 'list' permission
		/// <br/> 	to at least one file in the change.  A pending (not shelved)
		/// <br/> 	restricted change is only accessible to its owner.  Public changes
		/// <br/> 	are accessible to all users. This setting affects the output of the
		/// <br/> 	'p4 change', 'p4 changes', and 'p4 describe' commands. Note that
		/// <br/> 	the '-S' flag is required with 'p4 describe' for the command to
		/// <br/> 	enforce shelved	rather than pending restricted changelist rules.
		/// <br/> 
		/// <br/> 	If a user is not permitted to have access to a restricted change,
		/// <br/> 	The 'Description' text is replaced with a 'no permission' message
		/// <br/> 	(see 'Type' field). Users with admin permission can override the
		/// <br/> 	restriction using the -f flag.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To create a new changelist:
        ///		<code> 
        ///		    Changelist c = new Changelist();
        ///		    c = Repository.CreateChangelist(c, null);
        ///		</code>
        ///		To create a new restricted changelist:
        ///		<code> 
        ///		    Changelist c = new Changelist();
        ///		    c.Type = ChangeListType.Restricted;
        ///		    c = Repository.CreateChangelist(c, null);
        ///		</code>
        /// </example>
        /// <seealso cref="ChangeCmdFlags"/> 
        public Changelist CreateChangelist(Changelist change, Options options)
		{
			if (change.Id != -1)
			{
				throw new ArgumentOutOfRangeException("change", "Can only create a new changelist");
			}
			return SaveChangelist(change, options);
		}

		/// <summary>
		/// Create a new change in the repository.
		/// </summary>
		/// <param name="change">Changelist specification for the new change</param>
		/// <returns>The Changelist object if new change was created, null if creation failed</returns>
        /// <remarks>
        /// <br/>
		/// <br/><b>p4 help change</b>
		/// <br/> 
		/// <br/>     change -- Create or edit a changelist description
		/// <br/>     changelist -- synonym for 'change'
		/// <br/> 
		/// <br/>     p4 change [-s] [-f | -u] [[-O] changelist#]
		/// <br/>     p4 change -d [-f -s -O] changelist#
		/// <br/>     p4 change -o [-s] [-f] [[-O] changelist#]
		/// <br/>     p4 change -i [-s] [-f | -u] 
		/// <br/>     p4 change -t restricted | public [-U user] [-f | -u | -O] changelist#
		/// <br/>     p4 change -U user [-t restricted | public] [-f] changelist#
		/// <br/> 
		/// <br/> 	'p4 change' creates and edits changelists and their descriptions.
		/// <br/> 	With no argument, 'p4 change' creates a new changelist.  If a
		/// <br/> 	changelist number is specified, 'p4 change' edits an existing
		/// <br/> 	pending changelist.  In both cases, the changelist specification
		/// <br/> 	is placed into a form and the user's editor is invoked.
		/// <br/> 
		/// <br/> 	The -d flag deletes a pending changelist, if it has no opened files
		/// <br/> 	and no pending fixes associated with it.  Use 'p4 opened -a' to
		/// <br/> 	report on opened files and 'p4 reopen' to move them to another
		/// <br/> 	changelist.  Use 'p4 fixes -c changelist#' to report on pending
		/// <br/> 	fixes and 'p4 fix -d -c changelist# jobs...' to delete pending
		/// <br/> 	fixes. The changelist can be deleted only by the user and client
		/// <br/> 	who created it, or by a user with 'admin' privilege using the -f
		/// <br/> 	flag.
		/// <br/> 
		/// <br/> 	The -o flag writes the changelist specification to the standard
		/// <br/> 	output.  The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a changelist specification from the standard
		/// <br/> 	input.  The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -f flag forces the update or deletion of other users' pending
		/// <br/> 	changelists.  -f can also force the deletion of submitted changelists
		/// <br/> 	after they have been emptied of files using 'p4 obliterate'.  By
		/// <br/> 	default, submitted changelists cannot be changed.  The -f flag can
		/// <br/> 	also force display of the 'Description' field in a restricted
		/// <br/> 	changelist. Finally the -f flag can force changing the 'User' of
		/// <br/> 	an empty pending change via -U. The -f flag requires 'admin'
		/// <br/> 	access granted by 'p4 protect'.  The -f and -u flags are mutually
		/// <br/> 	exclusive.
		/// <br/> 
		/// <br/> 	The -u flag can force the update of a submitted change by the owner
		/// <br/> 	of the change. Only the Jobs, Type, and Description fields can be
		/// <br/> 	changed	using the -u flag. The -f and -u flags cannot be used in
		/// <br/> 	the same change command.
		/// <br/> 
		/// <br/> 	The -s flag extends the list of jobs to include the fix status
		/// <br/> 	for each job.  On new changelists, the fix status begins as the
		/// <br/> 	special status 'ignore', which, if left unchanged simply excludes
		/// <br/> 	the job from those being fixed.  Otherwise, the fix status, like
		/// <br/> 	that applied with 'p4 fix -s', becomes the job's status when
		/// <br/> 	the changelist is committed.  Note that this option exists
		/// <br/> 	to support integration with external defect trackers.
		/// <br/> 
		/// <br/> 	The -O flag specifies that the changelist number is the original
		/// <br/> 	number of a changelist which was renamed on submit.
		/// <br/> 
		/// <br/> 	The -U flag changes the 'User' of an empty pending change to the
		/// <br/> 	specified user. The user field can only be changed using this flag
		/// <br/> 	by the user who created the change, or by a user with 'admin'
		/// <br/> 	privilege using the -f flag. This option is useful for running
		/// <br/> 	in a trigger or script.
		/// <br/> 
		/// <br/> 	The -t flag changes the 'Type' of the change to 'restricted'
		/// <br/> 	or 'public' without displaying the change form. This option is
		/// <br/> 	useful for running in a trigger or script.
		/// <br/> 
		/// <br/> 	The 'Type' field can be used to hide the change or its description
		/// <br/> 	from users. Valid values for this field are 'public' (default), and
		/// <br/> 	'restricted'. A shelved or committed change that is 'restricted' is
		/// <br/> 	accessible only to users who own the change or have 'list' permission
		/// <br/> 	to at least one file in the change.  A pending (not shelved)
		/// <br/> 	restricted change is only accessible to its owner.  Public changes
		/// <br/> 	are accessible to all users. This setting affects the output of the
		/// <br/> 	'p4 change', 'p4 changes', and 'p4 describe' commands. Note that
		/// <br/> 	the '-S' flag is required with 'p4 describe' for the command to
		/// <br/> 	enforce shelved	rather than pending restricted changelist rules.
		/// <br/> 
		/// <br/> 	If a user is not permitted to have access to a restricted change,
		/// <br/> 	The 'Description' text is replaced with a 'no permission' message
		/// <br/> 	(see 'Type' field). Users with admin permission can override the
		/// <br/> 	restriction using the -f flag.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To create a new changelist:
        ///		<code> 
        ///		    Changelist c = new Changelist();
        ///		    c = Repository.CreateChangelist(c, null);
        ///		</code>
        /// </example>
		public Changelist CreateChangelist(Changelist change)
		{
			return CreateChangelist(change, null);
		}

		/// <summary>
		/// Update the record for a change in the repository
		/// </summary>
		/// <param name="change">Changelist specification for the change being updated</param>
		/// <returns>The Changelist object if new change was saved, null if creation failed</returns>
        /// <remarks>
        /// <br/>
		/// <br/><b>p4 help change</b>
		/// <br/> 
		/// <br/>     change -- Create or edit a changelist description
		/// <br/>     changelist -- synonym for 'change'
		/// <br/> 
		/// <br/>     p4 change [-s] [-f | -u] [[-O] changelist#]
		/// <br/>     p4 change -d [-f -s -O] changelist#
		/// <br/>     p4 change -o [-s] [-f] [[-O] changelist#]
		/// <br/>     p4 change -i [-s] [-f | -u] 
		/// <br/>     p4 change -t restricted | public [-U user] [-f | -u | -O] changelist#
		/// <br/>     p4 change -U user [-t restricted | public] [-f] changelist#
		/// <br/> 
		/// <br/> 	'p4 change' creates and edits changelists and their descriptions.
		/// <br/> 	With no argument, 'p4 change' creates a new changelist.  If a
		/// <br/> 	changelist number is specified, 'p4 change' edits an existing
		/// <br/> 	pending changelist.  In both cases, the changelist specification
		/// <br/> 	is placed into a form and the user's editor is invoked.
		/// <br/> 
		/// <br/> 	The -d flag deletes a pending changelist, if it has no opened files
		/// <br/> 	and no pending fixes associated with it.  Use 'p4 opened -a' to
		/// <br/> 	report on opened files and 'p4 reopen' to move them to another
		/// <br/> 	changelist.  Use 'p4 fixes -c changelist#' to report on pending
		/// <br/> 	fixes and 'p4 fix -d -c changelist# jobs...' to delete pending
		/// <br/> 	fixes. The changelist can be deleted only by the user and client
		/// <br/> 	who created it, or by a user with 'admin' privilege using the -f
		/// <br/> 	flag.
		/// <br/> 
		/// <br/> 	The -o flag writes the changelist specification to the standard
		/// <br/> 	output.  The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a changelist specification from the standard
		/// <br/> 	input.  The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -f flag forces the update or deletion of other users' pending
		/// <br/> 	changelists.  -f can also force the deletion of submitted changelists
		/// <br/> 	after they have been emptied of files using 'p4 obliterate'.  By
		/// <br/> 	default, submitted changelists cannot be changed.  The -f flag can
		/// <br/> 	also force display of the 'Description' field in a restricted
		/// <br/> 	changelist. Finally the -f flag can force changing the 'User' of
		/// <br/> 	an empty pending change via -U. The -f flag requires 'admin'
		/// <br/> 	access granted by 'p4 protect'.  The -f and -u flags are mutually
		/// <br/> 	exclusive.
		/// <br/> 
		/// <br/> 	The -u flag can force the update of a submitted change by the owner
		/// <br/> 	of the change. Only the Jobs, Type, and Description fields can be
		/// <br/> 	changed	using the -u flag. The -f and -u flags cannot be used in
		/// <br/> 	the same change command.
		/// <br/> 
		/// <br/> 	The -s flag extends the list of jobs to include the fix status
		/// <br/> 	for each job.  On new changelists, the fix status begins as the
		/// <br/> 	special status 'ignore', which, if left unchanged simply excludes
		/// <br/> 	the job from those being fixed.  Otherwise, the fix status, like
		/// <br/> 	that applied with 'p4 fix -s', becomes the job's status when
		/// <br/> 	the changelist is committed.  Note that this option exists
		/// <br/> 	to support integration with external defect trackers.
		/// <br/> 
		/// <br/> 	The -O flag specifies that the changelist number is the original
		/// <br/> 	number of a changelist which was renamed on submit.
		/// <br/> 
		/// <br/> 	The -U flag changes the 'User' of an empty pending change to the
		/// <br/> 	specified user. The user field can only be changed using this flag
		/// <br/> 	by the user who created the change, or by a user with 'admin'
		/// <br/> 	privilege using the -f flag. This option is useful for running
		/// <br/> 	in a trigger or script.
		/// <br/> 
		/// <br/> 	The -t flag changes the 'Type' of the change to 'restricted'
		/// <br/> 	or 'public' without displaying the change form. This option is
		/// <br/> 	useful for running in a trigger or script.
		/// <br/> 
		/// <br/> 	The 'Type' field can be used to hide the change or its description
		/// <br/> 	from users. Valid values for this field are 'public' (default), and
		/// <br/> 	'restricted'. A shelved or committed change that is 'restricted' is
		/// <br/> 	accessible only to users who own the change or have 'list' permission
		/// <br/> 	to at least one file in the change.  A pending (not shelved)
		/// <br/> 	restricted change is only accessible to its owner.  Public changes
		/// <br/> 	are accessible to all users. This setting affects the output of the
		/// <br/> 	'p4 change', 'p4 changes', and 'p4 describe' commands. Note that
		/// <br/> 	the '-S' flag is required with 'p4 describe' for the command to
		/// <br/> 	enforce shelved	rather than pending restricted changelist rules.
		/// <br/> 
		/// <br/> 	If a user is not permitted to have access to a restricted change,
		/// <br/> 	The 'Description' text is replaced with a 'no permission' message
		/// <br/> 	(see 'Type' field). Users with admin permission can override the
		/// <br/> 	restriction using the -f flag.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To update changelist 15:
        ///		<code> 
        ///		    Changelist c = Repository.GetChangelist(15);
        ///		    
        ///         // change the description
        ///         c.Description = "fixes for localization";
        ///		    c = Repository.UpdateChangelist(c);
        ///		</code>
        /// </example>
        public Changelist UpdateChangelist(Changelist change)
		{
			if (change.Id <= 0)
			{
				throw new ArgumentOutOfRangeException("change", "Can only update a numbered changelist");
			}
			return SaveChangelist(change, null);
		}

        /// <summary>
        /// Update the record for a change in the repository
        /// </summary>
        /// <param name="change">Changelist specification for the change being updated</param>
        /// <param name="options">options/flags</param>
        /// <returns>The Changelist object if new change was saved, null if creation failed</returns>
        /// <remarks>
        /// <br/>
		/// <br/><b>p4 help change</b>
		/// <br/> 
		/// <br/>     change -- Create or edit a changelist description
		/// <br/>     changelist -- synonym for 'change'
		/// <br/> 
		/// <br/>     p4 change [-s] [-f | -u] [[-O] changelist#]
		/// <br/>     p4 change -d [-f -s -O] changelist#
		/// <br/>     p4 change -o [-s] [-f] [[-O] changelist#]
		/// <br/>     p4 change -i [-s] [-f | -u] 
		/// <br/>     p4 change -t restricted | public [-U user] [-f | -u | -O] changelist#
		/// <br/>     p4 change -U user [-t restricted | public] [-f] changelist#
		/// <br/> 
		/// <br/> 	'p4 change' creates and edits changelists and their descriptions.
		/// <br/> 	With no argument, 'p4 change' creates a new changelist.  If a
		/// <br/> 	changelist number is specified, 'p4 change' edits an existing
		/// <br/> 	pending changelist.  In both cases, the changelist specification
		/// <br/> 	is placed into a form and the user's editor is invoked.
		/// <br/> 
		/// <br/> 	The -d flag deletes a pending changelist, if it has no opened files
		/// <br/> 	and no pending fixes associated with it.  Use 'p4 opened -a' to
		/// <br/> 	report on opened files and 'p4 reopen' to move them to another
		/// <br/> 	changelist.  Use 'p4 fixes -c changelist#' to report on pending
		/// <br/> 	fixes and 'p4 fix -d -c changelist# jobs...' to delete pending
		/// <br/> 	fixes. The changelist can be deleted only by the user and client
		/// <br/> 	who created it, or by a user with 'admin' privilege using the -f
		/// <br/> 	flag.
		/// <br/> 
		/// <br/> 	The -o flag writes the changelist specification to the standard
		/// <br/> 	output.  The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a changelist specification from the standard
		/// <br/> 	input.  The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -f flag forces the update or deletion of other users' pending
		/// <br/> 	changelists.  -f can also force the deletion of submitted changelists
		/// <br/> 	after they have been emptied of files using 'p4 obliterate'.  By
		/// <br/> 	default, submitted changelists cannot be changed.  The -f flag can
		/// <br/> 	also force display of the 'Description' field in a restricted
		/// <br/> 	changelist. Finally the -f flag can force changing the 'User' of
		/// <br/> 	an empty pending change via -U. The -f flag requires 'admin'
		/// <br/> 	access granted by 'p4 protect'.  The -f and -u flags are mutually
		/// <br/> 	exclusive.
		/// <br/> 
		/// <br/> 	The -u flag can force the update of a submitted change by the owner
		/// <br/> 	of the change. Only the Jobs, Type, and Description fields can be
		/// <br/> 	changed	using the -u flag. The -f and -u flags cannot be used in
		/// <br/> 	the same change command.
		/// <br/> 
		/// <br/> 	The -s flag extends the list of jobs to include the fix status
		/// <br/> 	for each job.  On new changelists, the fix status begins as the
		/// <br/> 	special status 'ignore', which, if left unchanged simply excludes
		/// <br/> 	the job from those being fixed.  Otherwise, the fix status, like
		/// <br/> 	that applied with 'p4 fix -s', becomes the job's status when
		/// <br/> 	the changelist is committed.  Note that this option exists
		/// <br/> 	to support integration with external defect trackers.
		/// <br/> 
		/// <br/> 	The -O flag specifies that the changelist number is the original
		/// <br/> 	number of a changelist which was renamed on submit.
		/// <br/> 
		/// <br/> 	The -U flag changes the 'User' of an empty pending change to the
		/// <br/> 	specified user. The user field can only be changed using this flag
		/// <br/> 	by the user who created the change, or by a user with 'admin'
		/// <br/> 	privilege using the -f flag. This option is useful for running
		/// <br/> 	in a trigger or script.
		/// <br/> 
		/// <br/> 	The -t flag changes the 'Type' of the change to 'restricted'
		/// <br/> 	or 'public' without displaying the change form. This option is
		/// <br/> 	useful for running in a trigger or script.
		/// <br/> 
		/// <br/> 	The 'Type' field can be used to hide the change or its description
		/// <br/> 	from users. Valid values for this field are 'public' (default), and
		/// <br/> 	'restricted'. A shelved or committed change that is 'restricted' is
		/// <br/> 	accessible only to users who own the change or have 'list' permission
		/// <br/> 	to at least one file in the change.  A pending (not shelved)
		/// <br/> 	restricted change is only accessible to its owner.  Public changes
		/// <br/> 	are accessible to all users. This setting affects the output of the
		/// <br/> 	'p4 change', 'p4 changes', and 'p4 describe' commands. Note that
		/// <br/> 	the '-S' flag is required with 'p4 describe' for the command to
		/// <br/> 	enforce shelved	rather than pending restricted changelist rules.
		/// <br/> 
		/// <br/> 	If a user is not permitted to have access to a restricted change,
		/// <br/> 	The 'Description' text is replaced with a 'no permission' message
		/// <br/> 	(see 'Type' field). Users with admin permission can override the
		/// <br/> 	restriction using the -f flag.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To update changelist 15:
        ///		<code> 
        ///		    Changelist c = Repository.GetChangelist(15);
        ///		    
        ///         // change the description
        ///         c.Description = "fixes for localization";
        ///		    c = Repository.UpdateChangelist(c);
        ///		</code>
        ///		To update changelist 15 as an admin user, changing type to Public:
        ///		<code> 
        ///		    Changelist c = Repository.GetChangelist(15);
        ///		    
        ///         ChangeCmdOptions opts = 
        ///         new ChangeCmdOptions(ChangeCmdFlags.Force);
        ///         c.Type = ChangeListType.Public;
        ///         c = Repository.UpdateChangelist(c, opts);
        ///		</code>
        /// </example>
        /// <seealso cref="ChangeCmdFlags"/> 
        public Changelist UpdateChangelist(Changelist change, Options options)
        {
            if (change.Id <= 0)
            {
                throw new ArgumentOutOfRangeException("change", "Can only update a numbered changelist");
            }
            return SaveChangelist(change, options);
        }

        /// <summary>
        /// Update the record for a submitted change in the repository
        /// </summary>
        /// <param name="change">Changelist specification for the change being updated</param>
        /// <returns>The Changelist object if new change was saved, null if creation failed</returns>
        /// <remarks>
        /// <br/>
		/// <br/><b>p4 help change</b>
		/// <br/> 
		/// <br/>     change -- Create or edit a changelist description
		/// <br/>     changelist -- synonym for 'change'
		/// <br/> 
		/// <br/>     p4 change [-s] [-f | -u] [[-O] changelist#]
		/// <br/>     p4 change -d [-f -s -O] changelist#
		/// <br/>     p4 change -o [-s] [-f] [[-O] changelist#]
		/// <br/>     p4 change -i [-s] [-f | -u] 
		/// <br/>     p4 change -t restricted | public [-U user] [-f | -u | -O] changelist#
		/// <br/>     p4 change -U user [-t restricted | public] [-f] changelist#
		/// <br/> 
		/// <br/> 	'p4 change' creates and edits changelists and their descriptions.
		/// <br/> 	With no argument, 'p4 change' creates a new changelist.  If a
		/// <br/> 	changelist number is specified, 'p4 change' edits an existing
		/// <br/> 	pending changelist.  In both cases, the changelist specification
		/// <br/> 	is placed into a form and the user's editor is invoked.
		/// <br/> 
		/// <br/> 	The -d flag deletes a pending changelist, if it has no opened files
		/// <br/> 	and no pending fixes associated with it.  Use 'p4 opened -a' to
		/// <br/> 	report on opened files and 'p4 reopen' to move them to another
		/// <br/> 	changelist.  Use 'p4 fixes -c changelist#' to report on pending
		/// <br/> 	fixes and 'p4 fix -d -c changelist# jobs...' to delete pending
		/// <br/> 	fixes. The changelist can be deleted only by the user and client
		/// <br/> 	who created it, or by a user with 'admin' privilege using the -f
		/// <br/> 	flag.
		/// <br/> 
		/// <br/> 	The -o flag writes the changelist specification to the standard
		/// <br/> 	output.  The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a changelist specification from the standard
		/// <br/> 	input.  The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -f flag forces the update or deletion of other users' pending
		/// <br/> 	changelists.  -f can also force the deletion of submitted changelists
		/// <br/> 	after they have been emptied of files using 'p4 obliterate'.  By
		/// <br/> 	default, submitted changelists cannot be changed.  The -f flag can
		/// <br/> 	also force display of the 'Description' field in a restricted
		/// <br/> 	changelist. Finally the -f flag can force changing the 'User' of
		/// <br/> 	an empty pending change via -U. The -f flag requires 'admin'
		/// <br/> 	access granted by 'p4 protect'.  The -f and -u flags are mutually
		/// <br/> 	exclusive.
		/// <br/> 
		/// <br/> 	The -u flag can force the update of a submitted change by the owner
		/// <br/> 	of the change. Only the Jobs, Type, and Description fields can be
		/// <br/> 	changed	using the -u flag. The -f and -u flags cannot be used in
		/// <br/> 	the same change command.
		/// <br/> 
		/// <br/> 	The -s flag extends the list of jobs to include the fix status
		/// <br/> 	for each job.  On new changelists, the fix status begins as the
		/// <br/> 	special status 'ignore', which, if left unchanged simply excludes
		/// <br/> 	the job from those being fixed.  Otherwise, the fix status, like
		/// <br/> 	that applied with 'p4 fix -s', becomes the job's status when
		/// <br/> 	the changelist is committed.  Note that this option exists
		/// <br/> 	to support integration with external defect trackers.
		/// <br/> 
		/// <br/> 	The -O flag specifies that the changelist number is the original
		/// <br/> 	number of a changelist which was renamed on submit.
		/// <br/> 
		/// <br/> 	The -U flag changes the 'User' of an empty pending change to the
		/// <br/> 	specified user. The user field can only be changed using this flag
		/// <br/> 	by the user who created the change, or by a user with 'admin'
		/// <br/> 	privilege using the -f flag. This option is useful for running
		/// <br/> 	in a trigger or script.
		/// <br/> 
		/// <br/> 	The -t flag changes the 'Type' of the change to 'restricted'
		/// <br/> 	or 'public' without displaying the change form. This option is
		/// <br/> 	useful for running in a trigger or script.
		/// <br/> 
		/// <br/> 	The 'Type' field can be used to hide the change or its description
		/// <br/> 	from users. Valid values for this field are 'public' (default), and
		/// <br/> 	'restricted'. A shelved or committed change that is 'restricted' is
		/// <br/> 	accessible only to users who own the change or have 'list' permission
		/// <br/> 	to at least one file in the change.  A pending (not shelved)
		/// <br/> 	restricted change is only accessible to its owner.  Public changes
		/// <br/> 	are accessible to all users. This setting affects the output of the
		/// <br/> 	'p4 change', 'p4 changes', and 'p4 describe' commands. Note that
		/// <br/> 	the '-S' flag is required with 'p4 describe' for the command to
		/// <br/> 	enforce shelved	rather than pending restricted changelist rules.
		/// <br/> 
		/// <br/> 	If a user is not permitted to have access to a restricted change,
		/// <br/> 	The 'Description' text is replaced with a 'no permission' message
		/// <br/> 	(see 'Type' field). Users with admin permission can override the
		/// <br/> 	restriction using the -f flag.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To update changelist 25:
        ///		<code> 
        ///		    Changelist c = Repository.GetChangelist(25);
        ///		    
        ///         // change the description
        ///         c.Description = "fixes for localization";
        ///		    c = Repository.UpdateSubmittedChangelist(c);
        ///		</code>
        /// </example>
        public Changelist UpdateSubmittedChangelist(Changelist change)
        {
            return UpdateSubmittedChangelist(change, null);
        }

        /// <summary>
		/// Update the record for a submitted change in the repository
		/// </summary>
		/// <param name="change">Changelist specification for the change being updated</param>
        /// <param name="options">options for the submitted change being updated</param>
		/// <returns>The Changelist object if new change was saved, null if creation failed</returns>
        ///  <remarks>
        /// <br/>
		/// <br/><b>p4 help change</b>
		/// <br/> 
		/// <br/>     change -- Create or edit a changelist description
		/// <br/>     changelist -- synonym for 'change'
		/// <br/> 
		/// <br/>     p4 change [-s] [-f | -u] [[-O] changelist#]
		/// <br/>     p4 change -d [-f -s -O] changelist#
		/// <br/>     p4 change -o [-s] [-f] [[-O] changelist#]
		/// <br/>     p4 change -i [-s] [-f | -u] 
		/// <br/>     p4 change -t restricted | public [-U user] [-f | -u | -O] changelist#
		/// <br/>     p4 change -U user [-t restricted | public] [-f] changelist#
		/// <br/> 
		/// <br/> 	'p4 change' creates and edits changelists and their descriptions.
		/// <br/> 	With no argument, 'p4 change' creates a new changelist.  If a
		/// <br/> 	changelist number is specified, 'p4 change' edits an existing
		/// <br/> 	pending changelist.  In both cases, the changelist specification
		/// <br/> 	is placed into a form and the user's editor is invoked.
		/// <br/> 
		/// <br/> 	The -d flag deletes a pending changelist, if it has no opened files
		/// <br/> 	and no pending fixes associated with it.  Use 'p4 opened -a' to
		/// <br/> 	report on opened files and 'p4 reopen' to move them to another
		/// <br/> 	changelist.  Use 'p4 fixes -c changelist#' to report on pending
		/// <br/> 	fixes and 'p4 fix -d -c changelist# jobs...' to delete pending
		/// <br/> 	fixes. The changelist can be deleted only by the user and client
		/// <br/> 	who created it, or by a user with 'admin' privilege using the -f
		/// <br/> 	flag.
		/// <br/> 
		/// <br/> 	The -o flag writes the changelist specification to the standard
		/// <br/> 	output.  The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a changelist specification from the standard
		/// <br/> 	input.  The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -f flag forces the update or deletion of other users' pending
		/// <br/> 	changelists.  -f can also force the deletion of submitted changelists
		/// <br/> 	after they have been emptied of files using 'p4 obliterate'.  By
		/// <br/> 	default, submitted changelists cannot be changed.  The -f flag can
		/// <br/> 	also force display of the 'Description' field in a restricted
		/// <br/> 	changelist. Finally the -f flag can force changing the 'User' of
		/// <br/> 	an empty pending change via -U. The -f flag requires 'admin'
		/// <br/> 	access granted by 'p4 protect'.  The -f and -u flags are mutually
		/// <br/> 	exclusive.
		/// <br/> 
		/// <br/> 	The -u flag can force the update of a submitted change by the owner
		/// <br/> 	of the change. Only the Jobs, Type, and Description fields can be
		/// <br/> 	changed	using the -u flag. The -f and -u flags cannot be used in
		/// <br/> 	the same change command.
		/// <br/> 
		/// <br/> 	The -s flag extends the list of jobs to include the fix status
		/// <br/> 	for each job.  On new changelists, the fix status begins as the
		/// <br/> 	special status 'ignore', which, if left unchanged simply excludes
		/// <br/> 	the job from those being fixed.  Otherwise, the fix status, like
		/// <br/> 	that applied with 'p4 fix -s', becomes the job's status when
		/// <br/> 	the changelist is committed.  Note that this option exists
		/// <br/> 	to support integration with external defect trackers.
		/// <br/> 
		/// <br/> 	The -O flag specifies that the changelist number is the original
		/// <br/> 	number of a changelist which was renamed on submit.
		/// <br/> 
		/// <br/> 	The -U flag changes the 'User' of an empty pending change to the
		/// <br/> 	specified user. The user field can only be changed using this flag
		/// <br/> 	by the user who created the change, or by a user with 'admin'
		/// <br/> 	privilege using the -f flag. This option is useful for running
		/// <br/> 	in a trigger or script.
		/// <br/> 
		/// <br/> 	The -t flag changes the 'Type' of the change to 'restricted'
		/// <br/> 	or 'public' without displaying the change form. This option is
		/// <br/> 	useful for running in a trigger or script.
		/// <br/> 
		/// <br/> 	The 'Type' field can be used to hide the change or its description
		/// <br/> 	from users. Valid values for this field are 'public' (default), and
		/// <br/> 	'restricted'. A shelved or committed change that is 'restricted' is
		/// <br/> 	accessible only to users who own the change or have 'list' permission
		/// <br/> 	to at least one file in the change.  A pending (not shelved)
		/// <br/> 	restricted change is only accessible to its owner.  Public changes
		/// <br/> 	are accessible to all users. This setting affects the output of the
		/// <br/> 	'p4 change', 'p4 changes', and 'p4 describe' commands. Note that
		/// <br/> 	the '-S' flag is required with 'p4 describe' for the command to
		/// <br/> 	enforce shelved	rather than pending restricted changelist rules.
		/// <br/> 
		/// <br/> 	If a user is not permitted to have access to a restricted change,
		/// <br/> 	The 'Description' text is replaced with a 'no permission' message
		/// <br/> 	(see 'Type' field). Users with admin permission can override the
		/// <br/> 	restriction using the -f flag.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To update changelist 25:
        ///		<code> 
        ///		    Changelist c = Repository.GetChangelist(25);
        ///		    
        ///         // change the description
        ///         c.Description = "fixes for localization";
        ///		    c = Repository.UpdateChangelist(c);
        ///		</code>
        ///		To update changelist 15 as an admin user, changing type to Public:
        ///		<code> 
        ///		    Changelist c = Repository.GetChangelist(25);
        ///		    
        ///         ChangeCmdOptions opts = 
        ///         new ChangeCmdOptions(ChangeCmdFlags.Force);
        ///         c.Type = ChangeListType.Public;
        ///         c = Repository.UpdateChangelist(c, opts);
        ///		</code>
        /// </example>
        /// <seealso cref="ChangeCmdFlags"/> 
        public Changelist UpdateSubmittedChangelist(Changelist change, Options options)
		{
			if (change.Id <= 0)
			{
				throw new ArgumentOutOfRangeException("change", "Can only update a numbered changelist");
			}
            if (options == null)
            {
                options=new Options();
                options["-u"] = null;
            }
			return SaveChangelist(change, options);
		}

		/// <summary>
		/// Get the record for an existing change from the repository.
		/// </summary>
		/// <param name="changeId">Changelist id</param>
		/// <param name="options">'-f' or '-s' are valid flags to use when fetching an existing change</param>
		/// <returns>The Changelist object if new change was found, null if creation failed</returns>
        /// <remarks>
        /// <br/>
		/// <br/><b>p4 help change</b>
		/// <br/> 
		/// <br/>     change -- Create or edit a changelist description
		/// <br/>     changelist -- synonym for 'change'
		/// <br/> 
		/// <br/>     p4 change [-s] [-f | -u] [[-O] changelist#]
		/// <br/>     p4 change -d [-f -s -O] changelist#
		/// <br/>     p4 change -o [-s] [-f] [[-O] changelist#]
		/// <br/>     p4 change -i [-s] [-f | -u] 
		/// <br/>     p4 change -t restricted | public [-U user] [-f | -u | -O] changelist#
		/// <br/>     p4 change -U user [-t restricted | public] [-f] changelist#
		/// <br/> 
		/// <br/> 	'p4 change' creates and edits changelists and their descriptions.
		/// <br/> 	With no argument, 'p4 change' creates a new changelist.  If a
		/// <br/> 	changelist number is specified, 'p4 change' edits an existing
		/// <br/> 	pending changelist.  In both cases, the changelist specification
		/// <br/> 	is placed into a form and the user's editor is invoked.
		/// <br/> 
		/// <br/> 	The -d flag deletes a pending changelist, if it has no opened files
		/// <br/> 	and no pending fixes associated with it.  Use 'p4 opened -a' to
		/// <br/> 	report on opened files and 'p4 reopen' to move them to another
		/// <br/> 	changelist.  Use 'p4 fixes -c changelist#' to report on pending
		/// <br/> 	fixes and 'p4 fix -d -c changelist# jobs...' to delete pending
		/// <br/> 	fixes. The changelist can be deleted only by the user and client
		/// <br/> 	who created it, or by a user with 'admin' privilege using the -f
		/// <br/> 	flag.
		/// <br/> 
		/// <br/> 	The -o flag writes the changelist specification to the standard
		/// <br/> 	output.  The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a changelist specification from the standard
		/// <br/> 	input.  The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -f flag forces the update or deletion of other users' pending
		/// <br/> 	changelists.  -f can also force the deletion of submitted changelists
		/// <br/> 	after they have been emptied of files using 'p4 obliterate'.  By
		/// <br/> 	default, submitted changelists cannot be changed.  The -f flag can
		/// <br/> 	also force display of the 'Description' field in a restricted
		/// <br/> 	changelist. Finally the -f flag can force changing the 'User' of
		/// <br/> 	an empty pending change via -U. The -f flag requires 'admin'
		/// <br/> 	access granted by 'p4 protect'.  The -f and -u flags are mutually
		/// <br/> 	exclusive.
		/// <br/> 
		/// <br/> 	The -u flag can force the update of a submitted change by the owner
		/// <br/> 	of the change. Only the Jobs, Type, and Description fields can be
		/// <br/> 	changed	using the -u flag. The -f and -u flags cannot be used in
		/// <br/> 	the same change command.
		/// <br/> 
		/// <br/> 	The -s flag extends the list of jobs to include the fix status
		/// <br/> 	for each job.  On new changelists, the fix status begins as the
		/// <br/> 	special status 'ignore', which, if left unchanged simply excludes
		/// <br/> 	the job from those being fixed.  Otherwise, the fix status, like
		/// <br/> 	that applied with 'p4 fix -s', becomes the job's status when
		/// <br/> 	the changelist is committed.  Note that this option exists
		/// <br/> 	to support integration with external defect trackers.
		/// <br/> 
		/// <br/> 	The -O flag specifies that the changelist number is the original
		/// <br/> 	number of a changelist which was renamed on submit.
		/// <br/> 
		/// <br/> 	The -U flag changes the 'User' of an empty pending change to the
		/// <br/> 	specified user. The user field can only be changed using this flag
		/// <br/> 	by the user who created the change, or by a user with 'admin'
		/// <br/> 	privilege using the -f flag. This option is useful for running
		/// <br/> 	in a trigger or script.
		/// <br/> 
		/// <br/> 	The -t flag changes the 'Type' of the change to 'restricted'
		/// <br/> 	or 'public' without displaying the change form. This option is
		/// <br/> 	useful for running in a trigger or script.
		/// <br/> 
		/// <br/> 	The 'Type' field can be used to hide the change or its description
		/// <br/> 	from users. Valid values for this field are 'public' (default), and
		/// <br/> 	'restricted'. A shelved or committed change that is 'restricted' is
		/// <br/> 	accessible only to users who own the change or have 'list' permission
		/// <br/> 	to at least one file in the change.  A pending (not shelved)
		/// <br/> 	restricted change is only accessible to its owner.  Public changes
		/// <br/> 	are accessible to all users. This setting affects the output of the
		/// <br/> 	'p4 change', 'p4 changes', and 'p4 describe' commands. Note that
		/// <br/> 	the '-S' flag is required with 'p4 describe' for the command to
		/// <br/> 	enforce shelved	rather than pending restricted changelist rules.
		/// <br/> 
		/// <br/> 	If a user is not permitted to have access to a restricted change,
		/// <br/> 	The 'Description' text is replaced with a 'no permission' message
		/// <br/> 	(see 'Type' field). Users with admin permission can override the
		/// <br/> 	restriction using the -f flag.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To get changelist 25:
        ///		<code> 
        ///		    Changelist c = Repository.GetChangelist(25, null);
        ///		</code>
        /// </example>
        /// <seealso cref="ChangeCmdFlags"/> 
		public Changelist GetChangelist(int changeId, Options options)
		{
			P4Command cmd = null;
			if (options == null)
			{
				options = new Options();
			}

			if (changeId > 0)
			{
				cmd = new P4Command(this, "describe", true, changeId.ToString());
			}
			else
			{
				cmd = new P4Command(this, "change", true);
				options["-o"] = null;
			}
	
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

				Changelist value = new Changelist();
				value.initialize(Connection);
				if (options.ContainsKey("-S"))
				{
					value.FromChangeCmdTaggedOutput((results.TaggedOutput[0]),true,offset,dst_mismatch);
				}
				else
				{
					value.FromChangeCmdTaggedOutput((results.TaggedOutput[0]),offset,dst_mismatch);
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
        /// Get the record for an existing change from the repository.
        /// </summary>
        /// <param name="changeId">Changelist name</param>
        /// <returns>The Changelist object if new change was found, null if creation failed</returns>
        /// <remarks>
        /// <br/>
		/// <br/><b>p4 help change</b>
		/// <br/> 
		/// <br/>     change -- Create or edit a changelist description
		/// <br/>     changelist -- synonym for 'change'
		/// <br/> 
		/// <br/>     p4 change [-s] [-f | -u] [[-O] changelist#]
		/// <br/>     p4 change -d [-f -s -O] changelist#
		/// <br/>     p4 change -o [-s] [-f] [[-O] changelist#]
		/// <br/>     p4 change -i [-s] [-f | -u] 
		/// <br/>     p4 change -t restricted | public [-U user] [-f | -u | -O] changelist#
		/// <br/>     p4 change -U user [-t restricted | public] [-f] changelist#
		/// <br/> 
		/// <br/> 	'p4 change' creates and edits changelists and their descriptions.
		/// <br/> 	With no argument, 'p4 change' creates a new changelist.  If a
		/// <br/> 	changelist number is specified, 'p4 change' edits an existing
		/// <br/> 	pending changelist.  In both cases, the changelist specification
		/// <br/> 	is placed into a form and the user's editor is invoked.
		/// <br/> 
		/// <br/> 	The -d flag deletes a pending changelist, if it has no opened files
		/// <br/> 	and no pending fixes associated with it.  Use 'p4 opened -a' to
		/// <br/> 	report on opened files and 'p4 reopen' to move them to another
		/// <br/> 	changelist.  Use 'p4 fixes -c changelist#' to report on pending
		/// <br/> 	fixes and 'p4 fix -d -c changelist# jobs...' to delete pending
		/// <br/> 	fixes. The changelist can be deleted only by the user and client
		/// <br/> 	who created it, or by a user with 'admin' privilege using the -f
		/// <br/> 	flag.
		/// <br/> 
		/// <br/> 	The -o flag writes the changelist specification to the standard
		/// <br/> 	output.  The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a changelist specification from the standard
		/// <br/> 	input.  The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -f flag forces the update or deletion of other users' pending
		/// <br/> 	changelists.  -f can also force the deletion of submitted changelists
		/// <br/> 	after they have been emptied of files using 'p4 obliterate'.  By
		/// <br/> 	default, submitted changelists cannot be changed.  The -f flag can
		/// <br/> 	also force display of the 'Description' field in a restricted
		/// <br/> 	changelist. Finally the -f flag can force changing the 'User' of
		/// <br/> 	an empty pending change via -U. The -f flag requires 'admin'
		/// <br/> 	access granted by 'p4 protect'.  The -f and -u flags are mutually
		/// <br/> 	exclusive.
		/// <br/> 
		/// <br/> 	The -u flag can force the update of a submitted change by the owner
		/// <br/> 	of the change. Only the Jobs, Type, and Description fields can be
		/// <br/> 	changed	using the -u flag. The -f and -u flags cannot be used in
		/// <br/> 	the same change command.
		/// <br/> 
		/// <br/> 	The -s flag extends the list of jobs to include the fix status
		/// <br/> 	for each job.  On new changelists, the fix status begins as the
		/// <br/> 	special status 'ignore', which, if left unchanged simply excludes
		/// <br/> 	the job from those being fixed.  Otherwise, the fix status, like
		/// <br/> 	that applied with 'p4 fix -s', becomes the job's status when
		/// <br/> 	the changelist is committed.  Note that this option exists
		/// <br/> 	to support integration with external defect trackers.
		/// <br/> 
		/// <br/> 	The -O flag specifies that the changelist number is the original
		/// <br/> 	number of a changelist which was renamed on submit.
		/// <br/> 
		/// <br/> 	The -U flag changes the 'User' of an empty pending change to the
		/// <br/> 	specified user. The user field can only be changed using this flag
		/// <br/> 	by the user who created the change, or by a user with 'admin'
		/// <br/> 	privilege using the -f flag. This option is useful for running
		/// <br/> 	in a trigger or script.
		/// <br/> 
		/// <br/> 	The -t flag changes the 'Type' of the change to 'restricted'
		/// <br/> 	or 'public' without displaying the change form. This option is
		/// <br/> 	useful for running in a trigger or script.
		/// <br/> 
		/// <br/> 	The 'Type' field can be used to hide the change or its description
		/// <br/> 	from users. Valid values for this field are 'public' (default), and
		/// <br/> 	'restricted'. A shelved or committed change that is 'restricted' is
		/// <br/> 	accessible only to users who own the change or have 'list' permission
		/// <br/> 	to at least one file in the change.  A pending (not shelved)
		/// <br/> 	restricted change is only accessible to its owner.  Public changes
		/// <br/> 	are accessible to all users. This setting affects the output of the
		/// <br/> 	'p4 change', 'p4 changes', and 'p4 describe' commands. Note that
		/// <br/> 	the '-S' flag is required with 'p4 describe' for the command to
		/// <br/> 	enforce shelved	rather than pending restricted changelist rules.
		/// <br/> 
		/// <br/> 	If a user is not permitted to have access to a restricted change,
		/// <br/> 	The 'Description' text is replaced with a 'no permission' message
		/// <br/> 	(see 'Type' field). Users with admin permission can override the
		/// <br/> 	restriction using the -f flag.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To get changelist 25:
        ///		<code> 
        ///		    Changelist c = Repository.GetChangelist(25);
        ///		</code>
        /// </example>
		public Changelist GetChangelist(int changeId)
		{
			return GetChangelist(changeId, null);
		}

        /// <summary>
        /// Get a list of changes from the repository
        /// </summary>
        /// <param name="options">options for the changes command<see cref="ChangesCmdOptions"/></param>
        /// <param name="files">array of FileSpecs for the changes command</param>
        /// <returns>A list containing the matching changes</returns>
        /// <remarks>
        /// <br/><b>p4 help changes</b>
        /// <br/> 
        /// <br/>     changes -- Display list of pending and submitted changelists
        /// <br/>     changelists -- synonym for 'changes'
        /// <br/> 
        /// <br/>     p4 changes [options] [file[revRange] ...]
        /// <br/> 
        /// <br/> 	options: -i -t -l -L -f -c client -m max -s status -u user
        /// <br/> 
        /// <br/> 	Returns a list of all pending and submitted changelists currently
        /// <br/> 	stored in the server.
        /// <br/> 
        /// <br/> 	If files are specified, 'p4 changes' lists only changelists that
        /// <br/> 	affect those files.  If the file specification includes a revision
        /// <br/> 	range, 'p4 changes' lists only submitted changelists that affect
        /// <br/> 	the specified revisions.  See 'p4 help revisions' for details.
        /// <br/> 
        /// <br/> 	If files are not specified, 'p4 changes' limits its report
        /// <br/> 	according to each change's type ('public' or 'restricted').
        /// <br/> 	If a submitted or shelved change is restricted, the change is
        /// <br/> 	not reported unless the user owns the change or has list
        /// <br/> 	permission for at least one file in the change. Only the owner
        /// <br/> 	of a restricted and pending (not shelved) change is permitted
        /// <br/> 	to see it.
        /// <br/> 
        /// <br/> 	The -i flag also includes any changelists integrated into the
        /// <br/> 	specified files.
        /// <br/> 
        /// <br/> 	The -t flag displays the time as well as the date.
        /// <br/> 
        /// <br/> 	The -l flag displays the full text of the changelist
        /// <br/> 	descriptions.
        /// <br/> 
        /// <br/> 	The -L flag displays the changelist descriptions, truncated to 250
        /// <br/> 	characters if longer.
        /// <br/> 
        /// <br/> 	The -f flag enables admin users to view restricted changes.
        /// <br/> 
        /// <br/> 	The -c client flag displays only submitted by the specified client.
        /// <br/> 
        /// <br/> 	The -m max flag limits changes to the 'max' most recent.
        /// <br/> 
        /// <br/> 	The -s status flag limits the output to changelists with the specified
        /// <br/> 	status. Specify '-s pending', '-s shelved', or '-s submitted'.
        /// <br/> 
        /// <br/> 	The -u user flag displays only changes owned by the specified user.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        /// <example>
        ///		To get all changelists owned by user bsmith:
        ///		<code> 
        ///		    ChangesCmdOptions opts = new ChangesCmdOptions(ChangesCmdFlags.None,
        ///		        null, 0, ChangeListStatus.None, "bsmith");
        ///
        ///		    IList&lt;Changelist&gt; changes =
        ///		    Repository.GetChangelists(opts, null);
        ///		</code>
        ///		To get all shelved changelists in file path //depot/main/...:
        ///		<code> 
        ///		    ChangesCmdOptions opts =new ChangesCmdOptions(ChangesCmdFlags.None,
        ///		        null, 0, ChangeListStatus.Shelved, null);
        ///
        ///         FileSpec file = 
        ///         new FileSpec(new DepotPath("//depot/main/..."), null, null, null);
        ///
        ///		    IList&lt;Changelist&gt; changes =
        ///		    Repository.GetChangelists(opts, file);
        ///		</code>
        ///		To the 20 latest submitted changelists from client "build_workspace"
        ///		with their full description:
        ///		<code> 
        ///		    ChangesCmdOptions opts =new ChangesCmdOptions(ChangesCmdFlags.FullDescription,
        ///		        "build_workspace", 20, ChangeListStatus.Submitted, null);
        ///
        ///		    IList&lt;Changelist&gt; changes =
        ///		    Repository.GetChangelists(opts, null);
        ///		</code>
        ///		To get all pending changelists as an admin user in file path
        ///		//depot/finance/...	including restricted changelists:
        ///		<code> 
        ///		    ChangesCmdOptions opts =new ChangesCmdOptions(ChangesCmdFlags.ViewRestricted,
        ///		        null, 0, ChangeListStatus.Pending, null);
        ///
        ///         FileSpec file = 
        ///         new FileSpec(new DepotPath("//depot/finance/..."), null, null, null);
        ///
        ///		    IList&lt;Changelist&gt; changes =
        ///		    Repository.GetChangelists(opts, file);
        ///		</code>
        /// </example>
        /// <seealso cref="ChangesCmdFlags"/> 
        public IList<Changelist> GetChangelists(Options options, params FileSpec[] files)
		{
			P4Command cmd = null;
			if ((files != null) && (files.Length > 0))
			{
				cmd = new P4Command(this, "changes", true, FileSpec.ToStrings(files));
			}
			else
			{
				cmd = new P4Command(this, "changes", true);
			}
			P4CommandResult results = cmd.Run(options);
			if (results.Success)
			{
				if ((results.TaggedOutput == null) || (results.TaggedOutput.Count <= 0))
				{
					return null;
				}
				List<Changelist> value = new List<Changelist>();

                bool dst_mismatch = false;
                string offset = string.Empty;

                if (Server != null && Server.Metadata != null)
                {
                    offset = Server.Metadata.DateTimeOffset;
                    dst_mismatch = FormBase.DSTMismatch(Server.Metadata);
                }

				foreach (TaggedObject obj in results.TaggedOutput)
				{
					Changelist change = new Changelist();
					change.initialize(Connection);
					change.FromChangeCmdTaggedOutput(obj,offset,dst_mismatch);
					value.Add(change);
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
		/// Delete a change from the repository
		/// </summary>
		/// <param name="change">The change to be deleted</param>
		/// <param name="options">The '-f' and '-s' flags are valid when deleting an existing change</param>
        /// <returns>The Changelist object if new change was found, null if creation failed</returns>
        /// <remarks>
        /// <br/>
		/// <br/><b>p4 help change</b>
		/// <br/> 
		/// <br/>     change -- Create or edit a changelist description
		/// <br/>     changelist -- synonym for 'change'
		/// <br/> 
		/// <br/>     p4 change [-s] [-f | -u] [[-O] changelist#]
		/// <br/>     p4 change -d [-f -s -O] changelist#
		/// <br/>     p4 change -o [-s] [-f] [[-O] changelist#]
		/// <br/>     p4 change -i [-s] [-f | -u] 
		/// <br/>     p4 change -t restricted | public [-U user] [-f | -u | -O] changelist#
		/// <br/>     p4 change -U user [-t restricted | public] [-f] changelist#
		/// <br/> 
		/// <br/> 	'p4 change' creates and edits changelists and their descriptions.
		/// <br/> 	With no argument, 'p4 change' creates a new changelist.  If a
		/// <br/> 	changelist number is specified, 'p4 change' edits an existing
		/// <br/> 	pending changelist.  In both cases, the changelist specification
		/// <br/> 	is placed into a form and the user's editor is invoked.
		/// <br/> 
		/// <br/> 	The -d flag deletes a pending changelist, if it has no opened files
		/// <br/> 	and no pending fixes associated with it.  Use 'p4 opened -a' to
		/// <br/> 	report on opened files and 'p4 reopen' to move them to another
		/// <br/> 	changelist.  Use 'p4 fixes -c changelist#' to report on pending
		/// <br/> 	fixes and 'p4 fix -d -c changelist# jobs...' to delete pending
		/// <br/> 	fixes. The changelist can be deleted only by the user and client
		/// <br/> 	who created it, or by a user with 'admin' privilege using the -f
		/// <br/> 	flag.
		/// <br/> 
		/// <br/> 	The -o flag writes the changelist specification to the standard
		/// <br/> 	output.  The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a changelist specification from the standard
		/// <br/> 	input.  The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -f flag forces the update or deletion of other users' pending
		/// <br/> 	changelists.  -f can also force the deletion of submitted changelists
		/// <br/> 	after they have been emptied of files using 'p4 obliterate'.  By
		/// <br/> 	default, submitted changelists cannot be changed.  The -f flag can
		/// <br/> 	also force display of the 'Description' field in a restricted
		/// <br/> 	changelist. Finally the -f flag can force changing the 'User' of
		/// <br/> 	an empty pending change via -U. The -f flag requires 'admin'
		/// <br/> 	access granted by 'p4 protect'.  The -f and -u flags are mutually
		/// <br/> 	exclusive.
		/// <br/> 
		/// <br/> 	The -u flag can force the update of a submitted change by the owner
		/// <br/> 	of the change. Only the Jobs, Type, and Description fields can be
		/// <br/> 	changed	using the -u flag. The -f and -u flags cannot be used in
		/// <br/> 	the same change command.
		/// <br/> 
		/// <br/> 	The -s flag extends the list of jobs to include the fix status
		/// <br/> 	for each job.  On new changelists, the fix status begins as the
		/// <br/> 	special status 'ignore', which, if left unchanged simply excludes
		/// <br/> 	the job from those being fixed.  Otherwise, the fix status, like
		/// <br/> 	that applied with 'p4 fix -s', becomes the job's status when
		/// <br/> 	the changelist is committed.  Note that this option exists
		/// <br/> 	to support integration with external defect trackers.
		/// <br/> 
		/// <br/> 	The -O flag specifies that the changelist number is the original
		/// <br/> 	number of a changelist which was renamed on submit.
		/// <br/> 
		/// <br/> 	The -U flag changes the 'User' of an empty pending change to the
		/// <br/> 	specified user. The user field can only be changed using this flag
		/// <br/> 	by the user who created the change, or by a user with 'admin'
		/// <br/> 	privilege using the -f flag. This option is useful for running
		/// <br/> 	in a trigger or script.
		/// <br/> 
		/// <br/> 	The -t flag changes the 'Type' of the change to 'restricted'
		/// <br/> 	or 'public' without displaying the change form. This option is
		/// <br/> 	useful for running in a trigger or script.
		/// <br/> 
		/// <br/> 	The 'Type' field can be used to hide the change or its description
		/// <br/> 	from users. Valid values for this field are 'public' (default), and
		/// <br/> 	'restricted'. A shelved or committed change that is 'restricted' is
		/// <br/> 	accessible only to users who own the change or have 'list' permission
		/// <br/> 	to at least one file in the change.  A pending (not shelved)
		/// <br/> 	restricted change is only accessible to its owner.  Public changes
		/// <br/> 	are accessible to all users. This setting affects the output of the
		/// <br/> 	'p4 change', 'p4 changes', and 'p4 describe' commands. Note that
		/// <br/> 	the '-S' flag is required with 'p4 describe' for the command to
		/// <br/> 	enforce shelved	rather than pending restricted changelist rules.
		/// <br/> 
		/// <br/> 	If a user is not permitted to have access to a restricted change,
		/// <br/> 	The 'Description' text is replaced with a 'no permission' message
		/// <br/> 	(see 'Type' field). Users with admin permission can override the
		/// <br/> 	restriction using the -f flag.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To delete pending changelist 34:
        ///		<code> 
        ///		    Repository.DeleteChangelist(34,null);
        ///		</code>
        ///		To delete pending changelist 34 as an admin user
        ///		who is not owner of the change:
        ///		<code> 
        ///		    ChangeCmdOptions opts = new ChangeCmdOptions(ChangeCmdFlags.Force);
        ///		    Repository.DeleteChangelist(34, opts);
        ///		</code>
        /// </example>
        /// <seealso cref="ChangeCmdFlags"/> 
        public void DeleteChangelist(Changelist change, Options options)
		{
			if (change == null)
			{
				throw new ArgumentNullException("change");
			}
			P4Command cmd = new P4Command(this, "change", true, change.Id.ToString());

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
