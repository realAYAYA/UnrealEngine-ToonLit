// Copyright Epic Games, Inc. All Rights Reserved.
import { P4Client, P4Util } from '../framework'
import { SimpleMainAndReleaseTestBase, streams } from '../SimpleMainAndReleaseTestBase'

const TEXT_FILENAME = 'test.txt'
export class SyntaxErrorOnUnknownBranch extends SimpleMainAndReleaseTestBase {
	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces(streams)

		this.mainClient = this.getClient('Main')

		await P4Util.addFileAndSubmit(this.mainClient, TEXT_FILENAME, 'Initial content')

		await this.initialPopulate()
	}

	async run() {
		const releaseClient = this.getClient('Release')
		await releaseClient.sync()
		await P4Util.editFileAndSubmit(releaseClient, TEXT_FILENAME, 'change', 'somebranch')

		await P4Util.editFile(this.mainClient, TEXT_FILENAME, 'change')
		await P4Util.submit(this.mainClient, `Edit with specific bot command\n#robomerge[${this.botName}] somebranch`)
	}

// @todo add code to fix syntax error and retry, make sure unblocks and updates source node Slack message
// latter is probably only broken in case where there's also an edge blockage

	verify() {
		return Promise.all([this.ensureBlocked('Main'), this.ensureBlocked('Release')])
	}

	allowSyntaxErrors() {
		return true
	}

	private mainClient: P4Client
}
