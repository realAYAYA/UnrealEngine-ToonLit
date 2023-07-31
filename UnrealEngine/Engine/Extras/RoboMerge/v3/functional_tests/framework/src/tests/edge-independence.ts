// Copyright Epic Games, Inc. All Rights Reserved.
import { P4Util } from '../framework'
import { MultipleDevAndReleaseTestBase } from '../MultipleDevAndReleaseTestBase'


const NUM_EDITS = 5
export class EdgeIndependence extends MultipleDevAndReleaseTestBase {
	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces()

		await P4Util.addFileAndSubmit(this.getClient('Main'), 'test.uasset', 'Initial content', true)

		await this.initialPopulate()
	}

	async run() {
		// prepare a conflict
		const perkinClient = this.getClient('Dev-Perkin')
		await perkinClient.sync()
		await P4Util.editFileAndSubmit(perkinClient, 'test.uasset', 'Different!')

		const r1client = this.getClient('Release-1.0')
		await r1client.sync()

		for (let editNum = 0; editNum < NUM_EDITS; ++editNum) {
			await P4Util.editFileAndSubmit(r1client, 'test.uasset', `rev${editNum}`)
		}
	}

	async verify() {
		await Promise.all([
			this.checkHeadRevision('Main', 'test.uasset', NUM_EDITS + 1),
			this.checkHeadRevision('Release-2.0', 'test.uasset', NUM_EDITS + 1),
			this.checkHeadRevision('Dev-Pootle', 'test.uasset', NUM_EDITS + 1),
			this.checkHeadRevision('Dev-Perkin', 'test.uasset', 2),
			//this.ensureBlocked(infoBeforeStomp.edges, escapeBranchName('Dev-Perkin')),
			this.ensureNotBlocked('Main', 'Dev-Pootle'),
			this.verifyAndPerformStomp('Main', 'Dev-Perkin')
		])

		// Give RM plenty of time to stomp and merge all those test.uasset revisions
		await this.waitForRobomergeIdle()

		await Promise.all([
			this.checkHeadRevision('Dev-Perkin', 'test.uasset', NUM_EDITS + 2),
			this.ensureNotBlocked('Main', 'Dev-Perkin')
		])
	}
}
