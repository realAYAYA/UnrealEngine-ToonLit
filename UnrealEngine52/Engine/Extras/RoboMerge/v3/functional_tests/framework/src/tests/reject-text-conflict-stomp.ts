// Copyright Epic Games, Inc. All Rights Reserved.
import * as Path from 'path'
import { FunctionalTest, P4Client, P4Util } from '../framework'
import * as System from '../system'
import { Perforce } from '../test-perforce'

type PerClient = {
	client: P4Client
	textFilePath: string
	binaryFilePath: string
}

export class RejectTextConflictStomp extends FunctionalTest {
	private mainUser1: PerClient
	private developmentUser1: PerClient
	private mainUser2: PerClient

	private perClient(user: string, stream: string) {
		const client = this.p4Client(user, stream)
		return {
			client,
			textFilePath: Path.join(client.root, 'textfile.txt'),
			binaryFilePath: Path.join(client.root, 'fake.uasset'),
		}
	}

	constructor(p4: Perforce) {
		super(p4)

		this.mainUser1 = this.perClient('testuser1', 'Main')
		this.developmentUser1 = this.perClient('testuser1', 'Development')
		this.mainUser2 = this.perClient('testuser2', 'Main')
	}

	async setup() {
		// Set up depot

		await this.p4.depot('stream', this.depotSpec())

		// Setup streams
		await this.p4.stream(this.streamSpec('Main', 'mainline'))
		await this.p4.stream(this.streamSpec('Development', 'development', 'Main'))

		await this.mainUser1.client.create(P4Util.specForClient(this.mainUser1.client))

		// Add text file 'textfile.txt'
		await System.writeFile(this.mainUser1.textFilePath, 'Simple functional test text file') 
		await this.mainUser1.client.add('textfile.txt')
		await this.mainUser1.client.submit("Adding initial file 'textfile.txt'")

		await System.writeFile(this.mainUser1.binaryFilePath, 'Simple functional test binary file') 
		await this.mainUser1.client.add('fake.uasset', true)

		await this.mainUser1.client.submit("Adding initial files 'fake.uasset'")

		// Populate Development stream
		await this.p4.populate(this.getStreamPath('Development'), `Initial branch of files from Main (${this.getStreamPath('Main')}) to Development (${this.getStreamPath('Development')})`)
	

		// Create Main workspace for testuser2 to create binary conflict
		await this.mainUser2.client.create(P4Util.specForClient(this.mainUser2.client))
		await this.mainUser2.client.sync()

		// Create future binary file conflict
		await this.mainUser2.client.edit('fake.uasset')
		await System.writeFile(this.mainUser2.binaryFilePath, 'Create second revision in Main for binary file')

		await this.mainUser2.client.edit('textfile.txt')
		await System.writeFile(this.mainUser2.textFilePath, 'Create second revision in Main for text file')

		await this.mainUser2.client.submit('Creating second revision in Main for binary and text files')

		// Create Development workspace for testuser1
		await this.developmentUser1.client.create(P4Util.specForClient(this.developmentUser1.client))
		await this.developmentUser1.client.sync()
	}

	async run() {

		// Create future binary file conflict
		await this.developmentUser1.client.edit('fake.uasset')
		await System.writeFile(this.developmentUser1.binaryFilePath, 'Create second revision in Development for binary file')

		await this.developmentUser1.client.edit('textfile.txt')
		await System.writeFile(this.developmentUser1.textFilePath, 'Create second revision in Development for text file')

		// This should result in a conflict on Main 'fake.uasset' so we can test stomp verification and execution!
		const command = `#robomerge ${this.testName}Main`
		await P4Util.submit(this.developmentUser1.client, 'Creating second revision in Development for both binary file and text file for merging\n' + command)
	}

	getBranches() {
		return [{
			streamDepot: this.testName,
			name: this.testName + 'Main',
			streamName: 'Main',
			flowsTo: [this.testName + 'Development'],
			forceAll: false
		}, {
			streamDepot: this.testName,
			name: this.testName + 'Development',
			streamName: 'Development',
			flowsTo: [this.testName + 'Main'],
		}]
	}

	async verify() {
		const edgeState = await this.getEdgeState('Development', 'Main')
		const verifyResult = await this.verifyStompRequest('Development', 'Main', edgeState)

		if (!verifyResult.validRequest && !verifyResult.nonBinaryFilesResolved && !verifyResult.remainingAllBinary) {
			this.info('Success! Stomp verify detected files remaining after the automatic merge, which is correct for this functional test.')
		}
		else {
			throw new Error(`Stomp verify returned unexpected values. Erroring...\n${JSON.stringify(verifyResult)}`)
		}

		const stompResult = await this.performStompRequest('Development', 'Main', edgeState)

		if (stompResult.statusCode === 500) {
			this.info('Robomerge successfully rejected our stomp request')
			return
		}
		
		throw new Error(`FAILURE!!! Robomerge did not reject our stomp request, which should have failed due to branch resolve requirement. Response was: \n${JSON.stringify(stompResult)}`)
	}
}
