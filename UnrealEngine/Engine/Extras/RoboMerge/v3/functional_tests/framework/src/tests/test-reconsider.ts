// Copyright Epic Games, Inc. All Rights Reserved.
import { FunctionalTest, P4Util, Stream } from '../framework'

const streams: Stream[] = [
	{name: 'Main', streamType: 'mainline'},
	{name: 'Dev-Pootle', streamType: 'development', parent: 'Main'},
	{name: 'Dev-Perkin', streamType: 'development', parent: 'Main'}
]

export class TestReconsider extends FunctionalTest {
	revisionCl = -1

	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces(streams)

		const mainClient = this.getClient('Main', 'testuser1')
		await P4Util.addFileAndSubmit(mainClient, 'test.txt', 'Initial content')

		const desc = 'Initial branch of files from Main'
		await Promise.all([
			this.p4.populate(this.getStreamPath('Dev-Perkin'), desc),
			this.p4.populate(this.getStreamPath('Dev-Pootle'), desc)
		])

		// making edit in set-up, i.e. before added to RoboMerge
		this.revisionCl = await P4Util.editFileAndSubmit(mainClient, 'test.txt', 'Initial content\n\nFirst addition')
	}

	async run() {
		await Promise.all([
			this.checkHeadRevision('Main', 'test.txt', 2),
			this.checkHeadRevision('Dev-Pootle', 'test.txt', 1),
			this.checkHeadRevision('Dev-Perkin', 'test.txt', 1) 
		])
		await this.reconsider('Main', this.revisionCl)
	}

	async verify() {
		return Promise.all([
			this.checkHeadRevision('Main', 'test.txt', 2),
			this.checkHeadRevision('Dev-Pootle', 'test.txt', 2), // reconsidering source node, affects all edges
			this.checkHeadRevision('Dev-Perkin', 'test.txt', 2),

			this.checkDescriptionContainsEdit('Dev-Pootle'),
			this.checkDescriptionContainsEdit('Dev-Perkin'),
		])
	}

	getBranches() {
		return [
			this.makeForceAllBranchDef('Main', ['Dev-Perkin', 'Dev-Pootle']),
			this.makeForceAllBranchDef('Dev-Perkin', []),
			this.makeForceAllBranchDef('Dev-Pootle', [])
		]
	}
}
