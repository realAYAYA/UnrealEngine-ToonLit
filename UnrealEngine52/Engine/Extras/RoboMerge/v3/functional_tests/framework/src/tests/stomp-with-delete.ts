// Copyright Epic Games, Inc. All Rights Reserved.
import { P4Util } from '../framework'
import { SimpleMainAndReleaseTestBase, streams } from '../SimpleMainAndReleaseTestBase'

export class StompWithDelete extends SimpleMainAndReleaseTestBase {

	async setup() {
		// Set up depot

		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces(streams)
		await P4Util.addFileAndSubmit(this.getClient('Main'), 'fake.uasset', 'Initial content', true)
		await this.initialPopulate()
		await P4Util.editFileAndSubmit(this.getClient('Main'), 'fake.uasset', 'New content')
	}

	async run() {
		await this.getClient('Release').sync()
		await P4Util.deleteFileAndSubmit(this.getClient('Release'), 'fake.uasset')
		await this.waitForRobomergeIdle()
		await this.ensureBlocked('Release', 'Main')
		await this.verifyAndPerformStomp('Release', 'Main')
	}

	async verify() {
		await this.ensureNotBlocked('Release', 'Main')
	}
}
