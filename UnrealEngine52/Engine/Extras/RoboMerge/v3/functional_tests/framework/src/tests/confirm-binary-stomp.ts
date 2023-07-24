// Copyright Epic Games, Inc. All Rights Reserved.
import { FunctionalTest, P4Client, P4Util, RobomergeBranchSpec, Stream } from '../framework'

// const jsonlint: any = require('jsonlint')

export const streams: Stream[] = [
	{name: 'Main', streamType: 'mainline'},
	{name: 'DevBranch1', streamType: 'development', parent: 'Main'},
	{name: 'DevBranch2', streamType: 'development', parent: 'Main'}
]

const TEXT_FILENAME = 'testfile.txt'
const ASSET_FILENAME = 'fake.uasset'
const COLLECTION_FILENAME = 'fake.collection'

export class ConfirmBinaryStomp extends FunctionalTest {

	// per user list
	mainClient: P4Client[]

	// per dev branch list
	devClient: P4Client[]

	async setup() {
		// Set up depot

		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces(streams, undefined, ['testuser1', 'testuser2'])

		// Add base files
		this.mainClient = [this.getClient('Main', 'testuser1'), this.getClient('Main', 'testuser2')]
		this.devClient = [this.getClient('DevBranch1'), this.getClient('DevBranch2')]
		await Promise.all([
			P4Util.addFile(this.mainClient[0], TEXT_FILENAME, 'text file content'),
			P4Util.addFile(this.mainClient[0], ASSET_FILENAME, 'fake binary file content', true),
			P4Util.addFile(this.mainClient[0], COLLECTION_FILENAME, 'collection content'),
		])

		await this.mainClient[0].submit('Adding initial files')

		// Populate DevBranch streams
		await Promise.all([
			this.p4.populate(this.getStreamPath('DevBranch1'), 'Initial branch of files from Main'),
			this.p4.populate(this.getStreamPath('DevBranch2'), 'Initial branch of files from Main'),
			this.mainClient[1].sync()

		])

		await Promise.all([
			this.devClient[0].sync(),
			this.devClient[1].sync()
		])

		// Create future binary file conflict in dev branches
		await Promise.all([
			P4Util.editFile(this.devClient[0], ASSET_FILENAME, 'Changed content'),
			P4Util.editFile(this.devClient[0], COLLECTION_FILENAME, 'Changed content'),
			P4Util.editFile(this.devClient[1], ASSET_FILENAME, 'Changed content'),
			P4Util.editFile(this.devClient[1], COLLECTION_FILENAME, 'Changed content')
		])

		// This should result in a conflict on 'fake.uasset' in both streams so we can test stomp verification and execution for explicit targets!
		await Promise.all([
			P4Util.submit(this.devClient[0], 'Second revision in DevBranch1 for both binary file and collection file for merging'),
			P4Util.submit(this.devClient[1], 'Second revision in DevBranch2 for both binary file and collection file for merging')
		])
	}

	async run() {
		// Create future binary file conflict
		await Promise.all([
			P4Util.editFile(this.mainClient[1], ASSET_FILENAME, 'Conflicting content'),
			P4Util.editFile(this.mainClient[1], COLLECTION_FILENAME, 'Conflicting content')
		])

		// Submit 
		await this.mainClient[1].submit(`Creating second revision in Main for binary and collection files\n#robomerge ${this.fullBranchName("DevBranch1")}`)

		// Now Main should be blocked for both DevBranch1 and DevBranch2
	}

	getBranches(): RobomergeBranchSpec[] {
		return [{
			streamDepot: this.testName,
			name: this.fullBranchName('Main'),
			streamName: 'Main',
			flowsTo: [this.fullBranchName('DevBranch1'), this.fullBranchName('DevBranch2')],
			forceFlowTo: [this.fullBranchName('DevBranch2')],
			forceAll: false,
			additionalSlackChannelForBlockages: this.testName
		}, {
			streamDepot: this.testName,
			name: this.fullBranchName('DevBranch1'),
			streamName: 'DevBranch1',
			flowsTo: [this.fullBranchName('Main')],
		},
		{
			streamDepot: this.testName,
			name: this.fullBranchName('DevBranch2'),
			streamName: 'DevBranch2',
			flowsTo: [this.fullBranchName('Main')]
		}
	]}

	async verify() {
		// Test implicit target stomp
		await this.performAndVerifyStomp('DevBranch2')

		await Promise.all([
			this.ensureBlocked('Main', 'DevBranch1'), // This better still be blocked!
			this.ensureNotBlocked('Main', 'DevBranch2')
		])

		// Test explicit target stomp
		await this.performAndVerifyStomp('DevBranch1')

		await Promise.all([
			this.ensureNotBlocked('Main', 'DevBranch1'),
			this.ensureNotBlocked('Main', 'DevBranch2'),
		])
	}


	private async performAndVerifyStomp(branch: string) {
		await Promise.all([
			this.checkHeadRevision(branch, COLLECTION_FILENAME, 2),
			this.checkHeadRevision(branch, ASSET_FILENAME, 2)
		])

		// verify stomp
		let result = await this.verifyAndPerformStomp('Main', branch, this.testName)

		let files = result.verify.files

		if (files[0].targetFileName === `//${this.testName}/${branch}/${COLLECTION_FILENAME}` &&
			files[1].targetFileName === `//${this.testName}/${branch}/${ASSET_FILENAME}`) {
			this.info('Expected files were stomped')
		}
		else {
			this.error(files ? JSON.stringify(files) : 'no files')
			throw new Error(`Stomp verify returned unexpected values: ${JSON.stringify(result)}`)
		}

		await Promise.all([
			this.checkHeadRevision(branch, COLLECTION_FILENAME, 3),
			this.checkHeadRevision(branch, ASSET_FILENAME, 3),
			this.checkDescriptionContainsEdit(branch, ['Second revision']),
		])
	}
}
