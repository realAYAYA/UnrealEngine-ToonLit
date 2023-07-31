// Copyright Epic Games, Inc. All Rights Reserved.
import { P4Util } from '../framework'
import { MultipleDevAndReleaseTestBase } from '../MultipleDevAndReleaseTestBase'

export class IndirectTarget extends MultipleDevAndReleaseTestBase {

	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces()

		await P4Util.addFileAndSubmit(this.getClient('Main'), 'test.txt', 'Initial content')

		await this.initialPopulate()
	}

	run() {
		const r1client = this.getClient('Release-1.0')
		return r1client.sync()
			.then(() => P4Util.editFileAndSubmit(r1client, 'test.txt', 'Initial content\nSome more!',
				this.fullBranchName('Dev-Perkin')))
	}

	verify() {
		return Promise.all([
			this.checkHeadRevision('Main', 'test.txt', 2),
			this.checkHeadRevision('Release-2.0', 'test.txt', 2),
			this.checkHeadRevision('Dev-Perkin', 'test.txt', 2),
			this.checkHeadRevision('Dev-Pootle', 'test.txt', 1)
		])
	}

	allForceFlow() {
		return false
	}
}
