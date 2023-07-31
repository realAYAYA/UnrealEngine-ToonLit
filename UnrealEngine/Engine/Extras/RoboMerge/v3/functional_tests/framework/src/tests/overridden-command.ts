// Copyright Epic Games, Inc. All Rights Reserved.
import { P4Util } from '../framework'
import { MultipleDevAndReleaseTestBase } from '../MultipleDevAndReleaseTestBase'


export class OverriddenCommand extends MultipleDevAndReleaseTestBase {
	revisionCls = { node: -1, edge: -1, multiple: -1 }

	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces()

		const mainClient = this.getClient('Main')
		const r1Client = this.getClient('Release-1.0')
		const r2Client = this.getClient('Release-2.0')

		await P4Util.addFileAndSubmit(mainClient, 'node.txt', 'Initial content')
		await P4Util.addFileAndSubmit(mainClient, 'edge.txt', 'Initial content')
		await P4Util.addFileAndSubmit(mainClient, 'multiple.txt', 'Initial content')

		await this.initialPopulate()

		await Promise.all([r1Client.sync(), r2Client.sync()])

		// making edit in set-up, i.e. before added to RoboMerge
		this.revisionCls.node = await P4Util.editFileAndSubmit(this.getClient('Release-1.0'), 'node.txt', 'Initial content\n\nFirst addition')
		this.revisionCls.edge = await P4Util.editFileAndSubmit(this.getClient('Release-2.0'), 'edge.txt', 'Initial content\n\nFirst addition')
		this.revisionCls.multiple = await P4Util.editFileAndSubmit(this.getClient('Main'), 'multiple.txt', 'Initial content\n\nFirst addition')
	}

	async run() {
		await this.reconsider('Release-1.0', this.revisionCls.node, undefined, '#robomerge ' + this.fullBranchName('Release-2.0'))
		await this.reconsider('Release-2.0', this.revisionCls.edge, 'Main', '#robomerge ' + this.fullBranchName('Main'))
		const command = `#robomerge ${this.fullBranchName('Dev-Perkin')} | #robomerge ${this.fullBranchName('Dev-Pootle')}`
		await this.reconsider('Main', this.revisionCls.multiple, undefined, command)
	}

	verify() {
		const check = (file: string, counts: number[]) => [
			this.checkHeadRevision('Main', file, counts[0]),
			this.checkHeadRevision('Release-2.0', file, counts[1]),
			this.checkHeadRevision('Release-1.0', file, counts[2]),
			this.checkHeadRevision('Dev-Pootle', file, counts[3]),
			this.checkHeadRevision('Dev-Perkin', file, counts[4])
		]
		return Promise.all([
			...check('node.txt', [1, 2, 2, 1, 1]),
			...check('edge.txt', [2, 2, 1, 1, 1]),
			...check('multiple.txt', [2, 1, 1, 2, 2])
		])
	}

	allForceFlow() {
		return false
	}
}
