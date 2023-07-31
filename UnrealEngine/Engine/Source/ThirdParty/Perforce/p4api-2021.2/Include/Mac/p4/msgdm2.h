/*
 * Copyright 2019 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * msgdm2.h - overflow definitions of errors for data manager core subsystem.
 */

class MsgDm2 {
    public:
	static ErrorId ExistingStorage;
	static ErrorId ConfigHistData;
	static ErrorId LbrScanBadState;
	static ErrorId LbrScanCtlNotFound;
	static ErrorId UnshelveStreamResolve;
	static ErrorId RequiresAutoIdCode;
	static ErrorId SpecMissingBuiltin;
	static ErrorId StreamSpecIntegOkay;
	static ErrorId CheckFailedNoDB;
	static ErrorId RequiresAutoIdOrPosCode;
	static ErrorId CannotRecreateDeleteField;
	static ErrorId SpecRepairDisallowNNN; 
	static ErrorId SpecRepairNoCustomSpec; 
	static ErrorId NoStreamSpecPermsWarn;
	static ErrorId StreamSpecProtectsNotCompatible;
	static ErrorId StreamOpenedByUser;
	static ErrorId StreamOpenReOpen;
	static ErrorId RemoteLabelOpenFailed;
	static ErrorId RemoteLabelUpdateFailed;
	static ErrorId RemoteStreamUpdateFailed;
	static ErrorId StreamAtChangeDeleted;
	static ErrorId StreamNotOpenInChange;
	static ErrorId StreamParentViewNoChange;
	static ErrorId LbrRevVerOutOfRange;
	static ErrorId GblLockIndexMismatch;
	static ErrorId GblLockIndexMissing;
	static ErrorId GblLockMissing;
	static ErrorId StreamlogInteg;
	static ErrorId RemoteAutoGenSpecFailed;
	static ErrorId StreamParentViewMustBeOpen;
	static ErrorId StreamPVSourceComment;
	static ErrorId BeginUpgradeStep;
	static ErrorId EndUpgradeStep;
	static ErrorId StreamNoCmtClientBadSave;
	static ErrorId ConnNeedsFwdCrypto;
	static ErrorId NoStreamTypeChangePV;
	static ErrorId PurgeTaskStream;
	static ErrorId PurgeCheckWldDelIgn;
	static ErrorId PurgeCheckWldDel;
	static ErrorId PurgeCheckIgn;
	static ErrorId PurgePurgeCheckWldDelIgn;
	static ErrorId PurgePurgeCheckWldDel;
	static ErrorId PurgePurgeCheckIgn;
	static ErrorId IdHasWhitespace;
	static ErrorId IdHasEquals;
	static ErrorId RmtAddTopologyFailed;
	static ErrorId RmtTopologyExists;
	static ErrorId ImportDittoGraph;
	static ErrorId ReopenHasMoved;
	static ErrorId TopologyData;
	static ErrorId StreamViewMatchData;
	static ErrorId NoTopologyRecord;
	static ErrorId NoServerIDSet;
} ;
