// Copyright Epic Games, Inc. All Rights Reserved.
import { P4Util } from '../framework'
import { MultipleDevAndReleaseTestBase } from '../MultipleDevAndReleaseTestBase'


export class PostToAdditionalChannel extends MultipleDevAndReleaseTestBase {
	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces()

		await P4Util.addFileAndSubmit(this.getClient('Main'), 'test.uasset', 'Initial content', true)
		await P4Util.addFileAndSubmit(this.getClient('Main'), 'test2.uasset', 'Initial content', true)

		await this.initialPopulate()
	}

	async run() {
		// prepare some conflicts
		await Promise.all(['Dev-Perkin']
			.map(branchName => this.getClient(branchName))
			.map(client => client.sync()
				.then(() => P4Util.editFileAndSubmit(client, 'test.uasset', 'Different content')))
		)

		// prepare some conflicts
		await Promise.all(['Dev-Pootle']
			.map(branchName => this.getClient(branchName))
			.map(client => client.sync()
				.then(() => P4Util.editFileAndSubmit(client, 'test2.uasset', 'Different content')))
		)

		const r1client = this.getClient('Release-1.0')
		await r1client.sync()
		await P4Util.editFileAndSubmit(r1client, 'test.uasset', 'Initial content\nSome more!')
		await P4Util.editFileAndSubmit(r1client, 'test2.uasset', 'Initial content\nSome more!')
	}

	async verify() {

		this.info(`Ensuring messages written to expected channels`)

		await Promise.all([
			this.ensureConflictMessagePostedToSlack('Main', 'Dev-Perkin'),
			this.ensureConflictMessagePostedToSlack('Main', 'Dev-Perkin', 'Perkin'),
			this.ensureNoConflictMessagePostedToSlack('Main', 'Dev-Perkin', 'Pootle'),
			this.ensureNoConflictMessagePostedToSlack('Main', 'Dev-Pootle'),
			this.ensureNoConflictMessagePostedToSlack('Main', 'Dev-Pootle', 'Perkin'),
			this.ensureConflictMessagePostedToSlack('Main', 'Dev-Pootle', 'Pootle')
		])
	}

	getEdges() {
		return [{
			from: this.fullBranchName('Main'),
			to: this.fullBranchName('Dev-Perkin'),
			additionalSlackChannel: 'Perkin'
		},
		{
			from: this.fullBranchName('Main'),
			to: this.fullBranchName('Dev-Pootle'),
			additionalSlackChannel: 'Pootle',
			postOnlyToAdditionalChannel: true
		}]
	}

}
