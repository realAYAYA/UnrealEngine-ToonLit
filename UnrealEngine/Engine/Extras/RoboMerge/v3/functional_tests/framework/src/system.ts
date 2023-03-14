// Copyright Epic Games, Inc. All Rights Reserved.
import * as System from 'fs'

export function mkdir(path: string) {

	return new Promise<void>((done, fail) => {
		System.mkdir(path, {recursive: true, mode: 0o777}, err => {
			if (err && err.code !== 'EEXIST') {
				fail(err)
			}
			else {
				done()
			}
		})
	})
}

export function readFile(path: string) {
	return new Promise<string>((done, fail) => {
		System.readFile(path, 'ascii', (err, data) => {
			if (err) {
				fail(err)
			}
			else {
				done(data)
			}
		})
	})
}

export function writeFile(path: string, content: string) {
	return new Promise<void>((done, fail) => {
		System.writeFile(path, content, 'ascii', err => {
			if (err) {
				fail(err)
			}
			else {
				done()
			}
		})
	})
}

export function appendToFile(path: string, content: string) {
	return new Promise<void>((done, fail) => {
		System.appendFile(path, content, 'ascii', (err: Error) => {
			if (err) {
				fail(err)
			}
			else {
				done()
			}
		})
	})
}

const SHORT_SLEEP = 2
const MEDIUM_SLEEP = 5
const LONG_SLEEP = 8

export function sleep(seconds: number) {
	return new Promise<NodeJS.Timeout>((done, _) => setTimeout(done, seconds * 1000))
}

export async function shortSleep() {
	return sleep(SHORT_SLEEP)
}

export async function mediumSleep() {
	return sleep(MEDIUM_SLEEP)
}

export async function longSleep() {
	return sleep(LONG_SLEEP)
}
