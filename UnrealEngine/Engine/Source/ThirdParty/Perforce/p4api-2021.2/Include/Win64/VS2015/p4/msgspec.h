/*
 * Copyright 1995, 2000 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * msgspec.h - definitions of errors for spec subsystem.
 */

class MsgSpec {

    public:

	static ErrorId SpecBranch;
	static ErrorId SpecClient;
	static ErrorId SpecLabel;
	static ErrorId SpecLdap;
	static ErrorId SpecLicense;
	static ErrorId SpecChange;
	static ErrorId SpecDepot;
	static ErrorId SpecGroup;
	static ErrorId SpecProtect;
	static ErrorId SpecRemote;
	static ErrorId SpecRepo;
	static ErrorId SpecServer;
	static ErrorId SpecStream;
	static ErrorId SpecTrigger;
	static ErrorId SpecTypeMap;
	static ErrorId SpecUser;
	static ErrorId SpecJob;
	static ErrorId SpecEditSpec;
	static ErrorId SpecExtension;
	static ErrorId SpecExtensionIns;

	// Retired ErrorIds. We need to keep these so that clients 
	// built with newer apis can commnunicate with older servers 
	// still sending these.
};
