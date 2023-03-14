using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Text;

namespace Perforce.P4
{
	public partial class P4ClientError
	{
		public static int MsgLbr_BadType1 = ErrorOf(ErrorSubsystem.ES_LBR, 1, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 1); //"Unsupported librarian file type %lbrType%!"
		public static int MsgLbr_Purged = ErrorOf(ErrorSubsystem.ES_LBR, 2, ErrorSeverity.E_FAILED, ErrorGeneric.EV_EMPTY, 2); //"Old revision %lbrRev% of tempobj %lbrFile% purged; try using head revision."
		public static int MsgLbr_ScriptFailed = ErrorOf(ErrorSubsystem.ES_LBR, 3, ErrorSeverity.E_FAILED, ErrorGeneric.EV_ADMIN, 3); //"%trigger% %op%: %result%"
		public static int MsgLbr_After = ErrorOf(ErrorSubsystem.ES_LBR, 101, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 1); //"RCS no revision after %name%!"
		public static int MsgLbr_Checkin = ErrorOf(ErrorSubsystem.ES_LBR, 102, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 2); //"RCS checkin %file%#%rev% failed!"
		public static int MsgLbr_Checkout = ErrorOf(ErrorSubsystem.ES_LBR, 103, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 1); //"RCS checkout %file% failed!"
		public static int MsgLbr_Commit = ErrorOf(ErrorSubsystem.ES_LBR, 104, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 1); //"RCS can't commit changes to %file%!"
		public static int MsgLbr_Diff = ErrorOf(ErrorSubsystem.ES_LBR, 105, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 1); //"RCS diff %file% failed!"
		public static int MsgLbr_Edit0 = ErrorOf(ErrorSubsystem.ES_LBR, 106, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 0); //"RCS editLineNumber past currLineNumber!"
		public static int MsgLbr_Edit1 = ErrorOf(ErrorSubsystem.ES_LBR, 107, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 0); //"RCS editLineCount bogus in RcsPieceDelete!"
		public static int MsgLbr_Edit2 = ErrorOf(ErrorSubsystem.ES_LBR, 108, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 1); //"RCS editLine '%line%' bogus!"
		public static int MsgLbr_Empty = ErrorOf(ErrorSubsystem.ES_LBR, 109, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 0); //"RCS checkin author/state empty!"
		public static int MsgLbr_EofAt = ErrorOf(ErrorSubsystem.ES_LBR, 110, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 0); //"RCS EOF in @ block!"
		public static int MsgLbr_ExpDesc = ErrorOf(ErrorSubsystem.ES_LBR, 111, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 0); //"RCS expected desc!"
		public static int MsgLbr_Expect = ErrorOf(ErrorSubsystem.ES_LBR, 111, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 2); //"RCS expected %token%, got %token2%!"
		public static int MsgLbr_ExpEof = ErrorOf(ErrorSubsystem.ES_LBR, 112, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 0); //"RCS expected EOF!"
		public static int MsgLbr_ExpRev = ErrorOf(ErrorSubsystem.ES_LBR, 113, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 0); //"RCS expected optional revision!"
		public static int MsgLbr_ExpSemi = ErrorOf(ErrorSubsystem.ES_LBR, 114, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 0); //"RCS expected ;!"
		public static int MsgLbr_Lock = ErrorOf(ErrorSubsystem.ES_LBR, 115, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 1); //"RCS lock on %file% failed!"
		public static int MsgLbr_Loop = ErrorOf(ErrorSubsystem.ES_LBR, 116, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 1); //"RCS loop in revision tree at %name%!"
		public static int MsgLbr_Mangled = ErrorOf(ErrorSubsystem.ES_LBR, 117, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 1); //"RCS delta mangled: %text%!"
		public static int MsgLbr_MkDir = ErrorOf(ErrorSubsystem.ES_LBR, 118, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 1); //"RCS can't make directory for %file%!"
		public static int MsgLbr_NoBrRev = ErrorOf(ErrorSubsystem.ES_LBR, 119, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 1); //"RCS no branch to revision %rev%!"
		public static int MsgLbr_NoBranch = ErrorOf(ErrorSubsystem.ES_LBR, 120, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 1); //"RCS no such branch %branch%!"
		public static int MsgLbr_NoRev = ErrorOf(ErrorSubsystem.ES_LBR, 121, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 1); //"RCS no such revision %rev%!"
		public static int MsgLbr_NoRev3 = ErrorOf(ErrorSubsystem.ES_LBR, 123, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 1); //"RCS expected revision %rev% missing!"
		public static int MsgLbr_NoRevDel = ErrorOf(ErrorSubsystem.ES_LBR, 124, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 1); //"RCS non-existant revision %rev% to delete!"
		public static int MsgLbr_Parse = ErrorOf(ErrorSubsystem.ES_LBR, 125, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 1); //"RCS parse error at line %line%!"
		public static int MsgLbr_RevLess = ErrorOf(ErrorSubsystem.ES_LBR, 126, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 0); //"RCS log without matching revision!"
		public static int MsgLbr_TooBig = ErrorOf(ErrorSubsystem.ES_LBR, 127, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 0); //"RCS token too big!"
		public static int MsgLbr_RcsTooBig = ErrorOf(ErrorSubsystem.ES_LBR, 128, ErrorSeverity.E_FAILED, ErrorGeneric.EV_TOOBIG, 1); //"Result RCS file '%file%' is too big; change type to compressed text."
		public static int MsgLbr_LbrOpenFail = ErrorOf(ErrorSubsystem.ES_LBR, 131, ErrorSeverity.E_FAILED, ErrorGeneric.EV_FAULT, 2); //"Error opening librarian file %lbrFile% revision %lbrRev%."
		public static int MsgLbr_AlreadyOpen = ErrorOf(ErrorSubsystem.ES_LBR, 132, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 1); //"Librarian for %path% is already open!"
		public static int MsgLbr_FmtLbrStat3 = ErrorOf(ErrorSubsystem.ES_LBR, 133, ErrorSeverity.E_INFO, ErrorGeneric.EV_NONE, 15); //"%file% %rev% %type% %state% %action% %digest% %size% %change% %revDate% %modTime% %process% %timestamp% %origin% %retries% %lastError%"
		public static int MsgLbr_BadKeyword = ErrorOf(ErrorSubsystem.ES_LBR, 134, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 1); //"RCS keyword for %file% is malformed!"
		public static int MsgLbr_KeywordUnterminated = ErrorOf(ErrorSubsystem.ES_LBR, 135, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 2); //"While processing keywords in file %file%, a line longer than %length% was encountered which contained an initial keyword '$' sign but no matching terminating '$' sign. The maximum line length value can be configured by setting lbr.rcs.maxlen; alternately, if keyword expansion is not necessary for this file, change the file's type to remove the +k option (see 'p4 help filetypes')."
		public static int MsgLbr_ObjectReadError = ErrorOf(ErrorSubsystem.ES_LBR, 136, ErrorSeverity.E_FAILED, ErrorGeneric.EV_FAULT, 5); //"Error reading object type %objType% with sha %objSha% of length %expectedLength% at offset %offset% from pack % pack%."
		public static int MsgLbr_FmtLbrStat = ErrorOf(ErrorSubsystem.ES_LBR, 129, ErrorSeverity.E_INFO, ErrorGeneric.EV_NONE, 11); //"%file% %rev% %type% %state% %action% %digest% %size% %process% %timestamp% %retries% %lastError%"
		public static int MsgLbr_FmtLbrStat2 = ErrorOf(ErrorSubsystem.ES_LBR, 130, ErrorSeverity.E_INFO, ErrorGeneric.EV_NONE, 14); //"%file% %rev% %type% %state% %action% %digest% %size% %change% %revDate% %modTime% %process% %timestamp% %retries% %lastError%"
	}
}
