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
		public static int MsgOs_Sys = ErrorOf(ErrorSubsystem.ES_OS, 1, ErrorSeverity.E_FAILED, ErrorGeneric.EV_FAULT, 3); //"%operation%: %arg%: %errmsg%"
		public static int MsgOs_Sys2 = ErrorOf(ErrorSubsystem.ES_OS, 9, ErrorSeverity.E_FAILED, ErrorGeneric.EV_FAULT, 3); //"%operation2%: %arg2%: %errmsg2%"
		public static int MsgOs_SysUn = ErrorOf(ErrorSubsystem.ES_OS, 2, ErrorSeverity.E_FAILED, ErrorGeneric.EV_FAULT, 3); //"%operation%: %arg%: unknown errno %errno%"
		public static int MsgOs_SysUn2 = ErrorOf(ErrorSubsystem.ES_OS, 10, ErrorSeverity.E_FAILED, ErrorGeneric.EV_FAULT, 3); //"%operation2%: %arg2%: unknown errno %errno2%"
		public static int MsgOs_ChmodBetrayal = ErrorOf(ErrorSubsystem.ES_OS, 11, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 4); //"File mode modification failed! File %oldname% was successfully renamed to %newname% but the file permissions were not correctly changed to read-only. The current permissions are %perms% and the file inode number is %inode%."
		public static int MsgOs_Net = ErrorOf(ErrorSubsystem.ES_OS, 3, ErrorSeverity.E_FAILED, ErrorGeneric.EV_COMM, 3); //"%operation%: %arg%: %errmsg%"
		public static int MsgOs_NetUn = ErrorOf(ErrorSubsystem.ES_OS, 4, ErrorSeverity.E_FAILED, ErrorGeneric.EV_COMM, 3); //"%operation%: %arg%: unknown network error %errno%"
		public static int MsgOs_TooMany = ErrorOf(ErrorSubsystem.ES_OS, 5, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 1); //"%handle%: too many handles!"
		public static int MsgOs_Deleted = ErrorOf(ErrorSubsystem.ES_OS, 6, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 1); //"%handle%: deleted handled!"
		public static int MsgOs_NoSuch = ErrorOf(ErrorSubsystem.ES_OS, 7, ErrorSeverity.E_FATAL, ErrorGeneric.EV_FAULT, 1); //"%handle%: no such handle!"
		public static int MsgOs_EmptyFork = ErrorOf(ErrorSubsystem.ES_OS, 8, ErrorSeverity.E_FAILED, ErrorGeneric.EV_CLIENT, 1); //"Resource fork for %file% from server is empty."
		public static int MsgOs_NameTooLong = ErrorOf(ErrorSubsystem.ES_OS, 12, ErrorSeverity.E_FAILED, ErrorGeneric.EV_FAULT, 3); //"Filename '%filename%' is length %actual% which exceeds the internal length limit of %maxlen%."
		public static int MsgOs_ZipExists = ErrorOf(ErrorSubsystem.ES_OS, 13, ErrorSeverity.E_FAILED, ErrorGeneric.EV_FAULT, 1); //"Output zip file %file% already exists."
		public static int MsgOs_ZipOpenEntryFailed = ErrorOf(ErrorSubsystem.ES_OS, 14, ErrorSeverity.E_FAILED, ErrorGeneric.EV_FAULT, 2); //"Error %errorcode% creating new entry %entry% in zip"
		public static int MsgOs_ZipCloseEntryFailed = ErrorOf(ErrorSubsystem.ES_OS, 15, ErrorSeverity.E_FAILED, ErrorGeneric.EV_FAULT, 1); //"Error %errorcode% closing entry in zip"
		public static int MsgOs_ZipWriteFailed = ErrorOf(ErrorSubsystem.ES_OS, 16, ErrorSeverity.E_FAILED, ErrorGeneric.EV_FAULT, 2); //"Error %errorcode% writing buffer of length %len% in zip"
		public static int MsgOs_ZipMissing = ErrorOf(ErrorSubsystem.ES_OS, 17, ErrorSeverity.E_FAILED, ErrorGeneric.EV_FAULT, 1); //"Input zip file %file% is missing."
		public static int MsgOs_ZipNoEntry = ErrorOf(ErrorSubsystem.ES_OS, 18, ErrorSeverity.E_FAILED, ErrorGeneric.EV_FAULT, 2); //"Error %errorcode% locating entry %entry% in zip."
		public static int MsgOs_ZipOpenEntry = ErrorOf(ErrorSubsystem.ES_OS, 19, ErrorSeverity.E_FAILED, ErrorGeneric.EV_FAULT, 2); //"Error %errorcode% opening entry %entry% in zip."
		public static int MsgOs_ZipReadFailed = ErrorOf(ErrorSubsystem.ES_OS, 20, ErrorSeverity.E_FAILED, ErrorGeneric.EV_FAULT, 2); //"Error %errorcode% reading buffer of length %len% from zip."
		public static int MsgOs_ZlibInflateInit = ErrorOf(ErrorSubsystem.ES_OS, 21, ErrorSeverity.E_FAILED, ErrorGeneric.EV_FAULT, 2); //"inflateInit failed: for file %file%, inflateInit returned %retCode%."
		public static int MsgOs_ZlibInflateEOF = ErrorOf(ErrorSubsystem.ES_OS, 22, ErrorSeverity.E_FAILED, ErrorGeneric.EV_FAULT, 1); //"Premature end of compressed object data in file %file%"
		public static int MsgOs_ZlibInflate = ErrorOf(ErrorSubsystem.ES_OS, 23, ErrorSeverity.E_FAILED, ErrorGeneric.EV_FAULT, 2); //"inflate returned error: for file %file%, inflate returned %retCode%."
		public static int MsgOs_ZlibDeflateInit = ErrorOf(ErrorSubsystem.ES_OS, 24, ErrorSeverity.E_FAILED, ErrorGeneric.EV_FAULT, 2); //"deflateInit failed: for file %file%, deflateInit returned %retCode%."
		public static int MsgOs_ZlibInflateInitSeek = ErrorOf(ErrorSubsystem.ES_OS, 25, ErrorSeverity.E_FAILED, ErrorGeneric.EV_FAULT, 3); //"inflateInit failed: for file %file%, inflateInit returned %retCode% after seek to position %seekPos%."
	}
}
