// Copyright Epic Games, Inc. All Rights Reserved.
import { P4Util } from '../framework'
import { MultipleDevAndReleaseTestBase } from '../MultipleDevAndReleaseTestBase'

const contentAfterFirstEdit = 'Initial content\n\nFirst addition'

export class TestFlags extends MultipleDevAndReleaseTestBase {
	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces()

		await P4Util.addFileAndSubmit(this.getClient('Main', 'testuser1'), 'test.txt', 'Initial content')

		await this.initialPopulate()
	}

	run() {
		const r1client = this.getClient('Release-1.0')
		const nullMergePerkin = '!' + this.fullBranchName('Dev-Perkin')
		const skipPootle = '-' + this.fullBranchName('Dev-Pootle')
		return r1client.sync()
		.then(() => P4Util.editFileAndSubmit(r1client, 'test.txt', contentAfterFirstEdit, skipPootle))
		.then(() => P4Util.editFileAndSubmit(r1client, 'test.txt', 
				'Initial content\n\nFirst addition\n\nSecond addition', nullMergePerkin + ' ' + skipPootle))
	}

	async checkNumForwardedCommands(stream: string, expected: number) {
		const change = (await this.getClient(stream).changes(1))![0]
		const description = change.description
		
		let numTags = 0

		const tag = `#ROBOMERGE[${this.botName}]`
		for (const line of description.split('\n')) {
			if (line.startsWith(tag)) {
				++numTags
			}
		}
		if (numTags !== expected) {
			throw new Error(`Unexpected number of ${tag} tags, expected: ${expected}, got ${numTags}`)
		}
	}

	verify() {
		return Promise.all([
			this.checkNumForwardedCommands('Release-2.0', 1),
			this.checkNumForwardedCommands('Main', 1),
			this.checkNumForwardedCommands('Dev-Perkin', 0),

			this.checkHeadRevision('Main', 'test.txt', 3),
			this.checkHeadRevision('Release-2.0', 'test.txt', 3),
			this.checkHeadRevision('Dev-Perkin', 'test.txt', 3),
			this.checkHeadRevision('Dev-Pootle', 'test.txt', 1),
			this.getClient('Dev-Perkin').print('test.txt').then(contents => {
				if (contents !== contentAfterFirstEdit) {
					throw new Error('null merge not so null: ' + contents)
				}
			})
		])
	}
}
