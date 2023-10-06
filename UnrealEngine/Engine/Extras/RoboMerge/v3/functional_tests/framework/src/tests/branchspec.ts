// Copyright Epic Games, Inc. All Rights Reserved.
import { P4Util } from '../framework'
import { MultipleDevAndReleaseTestBase } from '../MultipleDevAndReleaseTestBase'


export class BranchspecTest extends MultipleDevAndReleaseTestBase {
	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces()
		await this.p4.branch(this.branchSpec('Main->Pootle', `${this.getStreamPath('Main')}/Allowed_* ${this.getStreamPath('Dev-Pootle')}/Allowed_*`))

		await P4Util.addFileAndSubmit(this.getClient('Main'), 'Allowed_test.uasset', 'Initial content', true)
		await P4Util.addFileAndSubmit(this.getClient('Main'), 'Filtered_test.uasset', 'Initial content', true)

		await this.initialPopulate()
	}

	async run() {
		const r1client = this.getClient('Release-1.0')
		await r1client.sync()
		await P4Util.editFile(r1client, 'Allowed_test.uasset', 'Initial content\nSome more!')
		await P4Util.editFile(r1client, 'Filtered_test.uasset', 'Initial content\nSome more!')
		await P4Util.submit(r1client, "Files")
	}

	async verify() {

		this.info('Ensuring branchspec filtered as expected')

		return Promise.all([
			this.checkHeadRevision('Main', 'Allowed_test.uasset', 2),
			this.checkHeadRevision('Dev-Perkin', 'Allowed_test.uasset', 2),
			this.checkHeadRevision('Dev-Pootle', 'Allowed_test.uasset', 2),
			this.checkHeadRevision('Main', 'Filtered_test.uasset', 2),
			this.checkHeadRevision('Dev-Perkin', 'Filtered_test.uasset', 2),
			this.checkHeadRevision('Dev-Pootle', 'Filtered_test.uasset', 1),
		])
	}

	getEdges() {
		return [{
			from: this.fullBranchName('Main'),
			to: this.fullBranchName('Dev-Pootle'),
			branchspec: 'Main->Pootle'
		}]
	}

}
