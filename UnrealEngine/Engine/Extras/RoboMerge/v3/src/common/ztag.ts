// Copyright Epic Games, Inc. All Rights Reserved.

import { ContextualLogger } from './logger';

const colors = require('colors')
colors.enable()
colors.setTheme(require('colors/themes/generic-logging.js'))

const FIELD_RE = /^\.\.\. ([a-zA-Z]*)(\d*) (.*)/

type ZtagProperties = {[key: string]: string | number | boolean}[]
type Type = 'string' | 'integer' | 'boolean'

export type ParseOptions = {
	expected?: {[field: string]: Type}
	optional?: {[field: string]: Type}
}

class Shape {
	fields = new Map<string, string>() // encode optionality in type?

	register(field: string) {
		if (!this.fields.has(field)) {
			this.fields.set(field, 'string')
		}
	}

	allows(key: string) {
		return this.fields.has(key)
	}

	check(key: string, msg: string) {
		if (!this.allows(key)) {
			throw new Error(msg + ': ' + key)
		}
	}

	// needed?
	typeOf(_key: string) {
				
	}
}

class Record {
	values = new Map<string, string | number | boolean>()

	constructor(public shape: Shape) {
	}

	// has all required fields?
	firstMissingField() {
		for (const [field, fieldType] of this.shape.fields.entries()) {
			if (!fieldType.endsWith('?') && !this.values.has(field)) {
				return field
			}
		}
		return null
	}

	checkComplete() {
		const missing = this.firstMissingField()
		if (missing) {
			// dump fields?
			throw new Error(`Expected field ${missing} not found`)
		}
	}

	set(key: string, valStr: string) {
		const fieldType = this.shape.fields.get(key) || ''
		const optional = fieldType.endsWith('?')
		if (fieldType.startsWith('integer')) {
			// ignore empty strings for optional fields (e.g. p4.changes can return a 'shelved' property with no value)
			if (!optional || valStr) {
				const num = parseInt(valStr)
				if (isNaN(num)) {
					throw new Error(`Failed to parse number field ${key}, value: ${valStr}`)
				}
				this.values.set(key, num)
			}
		}
		else if (fieldType.startsWith('boolean')) {
			if (!optional || valStr) {
				const valLower = valStr.toLowerCase()
				if (valLower !== 'true' && valLower !== 'false') {
					throw new Error(`Failed to parse boolean field ${key}, value: ${valStr}`)
				}
				this.values.set(key, valLower === 'true')
			}
		}
		else {
			this.values.set(key, valStr)
		}
	}

	has(key: string) {
		return this.values.has(key)
	}


}

class ZtagPropertyReader {
	private lineIndex = 0
	currentField = ''
	constructor(private lines: string[], private logger: ContextualLogger) {
	}

	done() {
		return this.lineIndex >= this.lines.length
	}

	parse(shape: Shape, onStartField: (rec: Record, field: string, index: number) => boolean): Record {

		const record = new Record(shape)

		let valueLines: string[] = []
		this.currentField = ''
		for (; this.lineIndex < this.lines.length; ++this.lineIndex) {
			const line = this.lines[this.lineIndex]

			const fieldMatch = line.match(FIELD_RE)
			if (!fieldMatch) {
				valueLines.push(line)
				continue
			}

			// copying current/next pattern for now
			const nextField = fieldMatch[1]
			const index = fieldMatch[2] ? parseInt(fieldMatch[2]) : -1

			try {
				if (onStartField(record, nextField, index)) {
					break
				}
			}
			catch (err) {
				this.logger.error("Parse error, assuming continuation: " + err.toString())
				valueLines.push(line)
				continue
			}

			if (this.currentField) {
				record.set(this.currentField, valueLines.join('\n'))
				valueLines = []
			}


			this.currentField = nextField
			valueLines.push(fieldMatch[3])

		}

		if (this.currentField) {
			record.set(this.currentField, valueLines.join('\n'))
		}

		return record
	}
}

function removeTrailingNewline(rec: Record, field: string) {
	// check last record
	const lastVal = rec.values.get(field)
	if (!lastVal) {
		throw new Error('internal error')
	}

	if (typeof lastVal === 'string') {
		if (!lastVal.endsWith('\n')) {
			throw new Error('expected break!')
		}
		rec.values.set(field, lastVal.substr(0, lastVal.length - 1))
	}
}

function initialShapeFromOptions(options?: ParseOptions) {
	// deduce shape from first record
	const shape = new Shape
	const expected = options && options.expected
	if (expected) {
		for (const field in expected) {
			shape.fields.set(field, expected[field])
		}
	}

	const optional = options && options.optional
	if (optional) {
		for (const k of Object.keys(optional)) {
			shape.fields.set(k, optional[k] + '?')
		}
	}
	return shape
}


export function parseHeaderAndArrayImpl(s: string, logger: ContextualLogger, headerOptions?: ParseOptions, arrayEntryOptions?: ParseOptions) : Record[] {
	const propertyReader = new ZtagPropertyReader(s.split('\n'), logger)

	const headerShape = initialShapeFromOptions(headerOptions)

	const headerRecord = propertyReader.parse(headerShape, (_1, _2, index) => {
		return index >= 0
	})

	if (propertyReader.done()) {
		return headerRecord.values.size === 0 ? [] : [headerRecord]
	}

	const entryShape = initialShapeFromOptions(arrayEntryOptions)

	const firstEntry = propertyReader.parse(entryShape, (_, field, index) => {
		if (index < 0) {
			throw new Error('expected array entry')
		}
		entryShape.register(field)
		return index > 0
	})

	const entries: Record[] = [firstEntry]
	while (!propertyReader.done()) {
		entries.push(propertyReader.parse(entryShape, (_, field, index) => {
			if (!entryShape.allows(field)) {
				throw new Error('Unexpected field: ' + field)
				// throw new Error(`Expected field '${field}' missing in output`)
			}
			return index > entries.length
		}))
	}

	removeTrailingNewline(entries[entries.length - 1], propertyReader.currentField)

	return [headerRecord, ...entries]
}

export function parseHeaderAndArray(s: string, logger: ContextualLogger, headerOptions?: ParseOptions, arrayEntryOptions?: ParseOptions): ZtagProperties {
	return parseHeaderAndArrayImpl(s, logger, headerOptions, arrayEntryOptions).map(rec => {
		rec.checkComplete()
		return Object.fromEntries([...rec.values])
	})
}

export function parseZtagOutput(ztagOutput: string, logger: ContextualLogger, options?: ParseOptions) {
	if (ztagOutput.trim().length === 0) {
		return []
	}

	if (ztagOutput.endsWith('\n\n')) {
		ztagOutput = ztagOutput.substr(0, ztagOutput.length - 1)
	}
	else if (ztagOutput.endsWith('\n')) {
		console.log('No empty line')
	}
	else {
		console.log('No empty line or final new line')
		ztagOutput += '\n'
	}

	const propertyReader = new ZtagPropertyReader(ztagOutput.split('\n'), logger)

	const shape = initialShapeFromOptions(options)


	const firstRecord = propertyReader.parse(shape, (rec, field) => {
		shape.register(field)
		return rec.has(field)
	})

	if (firstRecord.values.size === 0) {
		return []
	}

	removeTrailingNewline(firstRecord, propertyReader.currentField)
	const records: Record[] = [firstRecord]
	while (!propertyReader.done()) {
		const newRecord = propertyReader.parse(shape, (rec, field) => {
			if (!shape.allows(field)) {
				throw new Error('Unexpected field: ' + field)
				// throw new Error(`Expected field '${field}' missing in output`)
			}
			return rec.has(field)
		})
		removeTrailingNewline(newRecord, propertyReader.currentField)
		records.push(newRecord)
	}

	return records.map(rec => {
		rec.checkComplete()
		return Object.fromEntries([...rec.values])
	})
}

function equal(lhs: any[], rhs: any[]) {
	if (lhs.length !== rhs.length) {
		return false
	}

	for (let index = 0; index < lhs.length; ++index) {
		const lk = Object.keys(lhs[index])
		const rk = Object.keys(rhs[index])

		if (lk.length !== rk.length) {
			return false
		}
		for (const k of lk) {
			if ((lhs[index] as any)[k] !== (rhs[index] as any)[k]) {
				return false
			}
		}
	}
	return true
}

let tests = 0
let failures = 0
function doTest(it: string, s: string, logger: ContextualLogger, options: ParseOptions, expected?: any[], arrayEntryOptions?: ParseOptions) {
	// last ztag record seems to match others in ending with double \n
	// may need to revisit this when dealing with arrays

	++tests
	const fail = () => {
		console.log(colors.error('Failed: ' + it))
		++failures
	}

	let result: any[]
	try {
		result = arrayEntryOptions ? parseHeaderAndArray(s, logger, options, arrayEntryOptions) : parseZtagOutput(s + '\n', logger, options)
	}
	catch (err) {
		if (expected) {

			console.log(err.toString())
			fail()
		}
		return
	}
	if (!expected) {
		return fail()
	}
	if (!equal(result, expected)) {
		console.log(result)
		fail()
	}
}


// a\n\nb\n\n
// ---
// a

// b

// ---
// > String.split "." "a..b.."
// ["a","","b","",""]

// means I'm interpreting \n on last item as an empty line
// comes with assumption that lines all end with \n
// for now artificially remove final \n, can be more lenient in real code maybe

// way to think about it - compare to Python lines from a file
// but then you end up having to strip \ns off everything for no reason

// split and normalise, report if not as expected

// is rejoining also a problem? 

export function runTests(logger: ContextualLogger) {
	tests = 0
	failures = 0

	const test = (it: string, s: string, options: ParseOptions, expected?: any[], arrayEntryOptions?: ParseOptions) =>
		doTest(it, s, logger, options, expected, arrayEntryOptions)

if (true) {
	test('single entry', '... first woo\n... second yay\n', {},
						[{first: 'woo', second: 'yay'}])
	test('two entries', '... first a\n... second b\n\n... first c\n... second d\n', {},
						[{first: 'a', second: 'b'}, {first: 'c', second: 'd'}])
	test('multiline last', '... first a\n... second b\nsome more\n\n... first c\n... second d\n', {},
						[{first: 'a', second: 'b\nsome more'}, {first: 'c', second: 'd'}])
	test('multiline first', '... first a\nsome more\n... second b\n\n... first c\n... second d\n', {},
						[{first: 'a\nsome more', second: 'b'}, {first: 'c', second: 'd'}])
	test('multiline middle', '... first a\n... second b\nsome more\n... third c\n\n... first c\n... second d\n', {optional: {'third': 'string'}},
						[{first: 'a', second: 'b\nsome more', third: 'c'}, {first: 'c', second: 'd'}])
	test('multiline end', '... first a\n... second b\n\n... first c\n... second d\nsome more\n', {},
						[{first: 'a', second: 'b'}, {first: 'c', second: 'd\nsome more'}])
	test('field mismatch',	'... first a\n... second b\n\n... first c\n... eek d\n', {})
	test('no break',		'... first a\n... second b\n... first c\n... second d\n', {})
	test('expected', '... first woo\n... second yay\n', {expected: {first: 'string'}},
						[{first: 'woo', second: 'yay'}])
	test('expected fail', '... first woo\n... second yay\n', {expected: {boo: 'string'}})
	test('optional', '... first a\n... second b\n\n... first c\n... opt d\n... second e\n', {optional: {opt: 'string'}},
						[{first: 'a', second: 'b'}, {first: 'c', opt: 'd', second: 'e'}])
	test('optional fail', '... first a\n... second b\n\n... first c\n... bleh d\n', {optional: {opt: 'string'}})
	test('number', '... first 3\n... second 7\n', {expected: {first: 'integer', second: 'integer'}},
						[{first: 3, second: 7}])
	test('number fail', '... first 3\n... second woo\n', {expected: {first: 'integer', second: 'integer'}})
}
	// arrays (e.g. describe)
	test('simple array', '... y woo\n... z yay\n... a0 boo\n... a1 hoo\n', {},
						[{y: 'woo', z: 'yay'}, {a: 'boo'}, {a: 'hoo'}], {})
	test('array multiline in header', '... y woo\n... z yay\nsome more\n... a0 boo\n... a1 hoo\n', {},
						[{y: 'woo', z: 'yay\nsome more'}, {a: 'boo'}, {a: 'hoo'}], {})
	test('array multiline in array', '... y woo\n... z yay\n... a0 boo\nsome more\n... a1 hoo\n', {},
						[{y: 'woo', z: 'yay'}, {a: 'boo\nsome more'}, {a: 'hoo'}], {})
	test('array expected', '... y woo\n... z yay\n... a0 boo\n... a1 hoo\n', {expected: {y: 'string'}},
						[{y: 'woo', z: 'yay'}, {a: 'boo'}, {a: 'hoo'}], {expected: {a: 'string'}})

	// expected
	if (failures > 0) {
		logger.warn('ztag tests: ' + colors.warn(`Ran ${tests}, failed: ${failures}`))
	}
	else {
		logger.info('ztag tests: ' + colors.info(`${tests} succeeded`))
	}
	return failures
}

