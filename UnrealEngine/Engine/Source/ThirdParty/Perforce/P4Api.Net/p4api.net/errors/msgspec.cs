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
		public static int MsgSpec_SpecBranch = ErrorOf(ErrorSubsystem.ES_SPEC, 1, ErrorSeverity.E_INFO, ErrorGeneric.EV_NONE, 0); //
		public static int MsgSpec_SpecClient = ErrorOf(ErrorSubsystem.ES_SPEC, 2, ErrorSeverity.E_INFO, ErrorGeneric.EV_NONE, 0); //
		public static int MsgSpec_SpecStream = ErrorOf(ErrorSubsystem.ES_SPEC, 14, ErrorSeverity.E_INFO, ErrorGeneric.EV_NONE, 0); //
		public static int MsgSpec_SpecLabel = ErrorOf(ErrorSubsystem.ES_SPEC, 3, ErrorSeverity.E_INFO, ErrorGeneric.EV_NONE, 0); //
		public static int MsgSpec_SpecLdap = ErrorOf(ErrorSubsystem.ES_SPEC, 16, ErrorSeverity.E_INFO, ErrorGeneric.EV_NONE, 0); //
		public static int MsgSpec_SpecLicense = ErrorOf(ErrorSubsystem.ES_SPEC, 13, ErrorSeverity.E_INFO, ErrorGeneric.EV_NONE, 0); //
		public static int MsgSpec_SpecChange = ErrorOf(ErrorSubsystem.ES_SPEC, 4, ErrorSeverity.E_INFO, ErrorGeneric.EV_NONE, 0); //
		public static int MsgSpec_SpecDepot = ErrorOf(ErrorSubsystem.ES_SPEC, 5, ErrorSeverity.E_INFO, ErrorGeneric.EV_NONE, 0); //
		public static int MsgSpec_SpecGroup = ErrorOf(ErrorSubsystem.ES_SPEC, 6, ErrorSeverity.E_INFO, ErrorGeneric.EV_NONE, 0); //
		public static int MsgSpec_SpecProtect = ErrorOf(ErrorSubsystem.ES_SPEC, 7, ErrorSeverity.E_INFO, ErrorGeneric.EV_NONE, 0); //
		public static int MsgSpec_SpecServer = ErrorOf(ErrorSubsystem.ES_SPEC, 15, ErrorSeverity.E_INFO, ErrorGeneric.EV_NONE, 0); //
		public static int MsgSpec_SpecTrigger = ErrorOf(ErrorSubsystem.ES_SPEC, 8, ErrorSeverity.E_INFO, ErrorGeneric.EV_NONE, 0); //
		public static int MsgSpec_SpecTypeMap = ErrorOf(ErrorSubsystem.ES_SPEC, 9, ErrorSeverity.E_INFO, ErrorGeneric.EV_NONE, 0); //
		public static int MsgSpec_SpecUser = ErrorOf(ErrorSubsystem.ES_SPEC, 10, ErrorSeverity.E_INFO, ErrorGeneric.EV_NONE, 0); //
		public static int MsgSpec_SpecJob = ErrorOf(ErrorSubsystem.ES_SPEC, 11, ErrorSeverity.E_INFO, ErrorGeneric.EV_NONE, 0); //
		public static int MsgSpec_SpecEditSpec = ErrorOf(ErrorSubsystem.ES_SPEC, 12, ErrorSeverity.E_INFO, ErrorGeneric.EV_NONE, 0); //
		public static int MsgSpec_SpecRemote = ErrorOf(ErrorSubsystem.ES_SPEC, 17, ErrorSeverity.E_INFO, ErrorGeneric.EV_NONE, 0); //
		public static int MsgSpec_SpecRepo = ErrorOf(ErrorSubsystem.ES_SPEC, 18, ErrorSeverity.E_INFO, ErrorGeneric.EV_NONE, 0); //
	}
}
