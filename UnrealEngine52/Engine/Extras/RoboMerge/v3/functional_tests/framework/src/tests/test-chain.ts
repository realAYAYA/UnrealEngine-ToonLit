// Copyright Epic Games, Inc. All Rights Reserved.
import { P4Util } from '../framework'
import { MultipleDevAndReleaseTestBase } from '../MultipleDevAndReleaseTestBase'
import { Change } from '../test-perforce'

export class TestChain extends MultipleDevAndReleaseTestBase {
	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces()

		await P4Util.addFileAndSubmit(this.getClient('Main'), 'test.txt', 'Initial content')

		await this.initialPopulate()
	}

	run() {
		const r1client = this.getClient('Release-1.0')
		return r1client.sync()
			.then(() => P4Util.editFileAndSubmit(r1client, 'test.txt', 'Initial content\nSome more!'))
	}

	verify() {
		return Promise.all([
			this.getClient('Dev-Pootle').changes(1)
				.then((changes: Change[]) => {
					const description = changes[0]!.description
					let numTags = 0
					for (const line of description.split('\n')) {
						if (line.startsWith('#ROBOMERGE-SOURCE')) {
							++numTags
						}
					}
					if (numTags !== 1) {
						throw new Error(`Expected exactly one #ROBOMERGE-SOURCE tag, got ${numTags}`)
					}
				}),
			this.checkHeadRevision('Main', 'test.txt', 2),
			this.checkHeadRevision('Release-2.0', 'test.txt', 2),
			this.checkHeadRevision('Dev-Perkin', 'test.txt', 2),
			this.checkHeadRevision('Dev-Pootle', 'test.txt', 2)
		])
	}
}
