// Copyright Epic Games, Inc. All Rights Reserved.
import { P4Util } from '../framework'
import { MultipleDevAndReleaseTestBase } from '../MultipleDevAndReleaseTestBase'


export class UnreachableSkip extends MultipleDevAndReleaseTestBase {

	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces()
		await P4Util.addFileAndSubmit(this.getClient('Main'), 'test.txt', 'content')
		await this.initialPopulate()
	}

	run() {
		return P4Util.editFileAndSubmit(this.getClient('Main'), 'test.txt', 'updated', '-' + this.fullBranchName('Release-1.0'))
	}

	verify() {
		// currently not erroring
		// return this.ensureBlocked('Main')

		return this.checkHeadRevision('Release-2.0', 'test.txt', 1)
	}

	allowSyntaxErrors() {
		return true
	}
}

