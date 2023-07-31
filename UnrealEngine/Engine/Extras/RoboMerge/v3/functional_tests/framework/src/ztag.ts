// Copyright Epic Games, Inc. All Rights Reserved.

type ContextualLogger = number

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

	// @ts-ignore
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
				// this.logger.error("Parse error, assuming continuation: " + err.toString())
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

		// console.log(lastVal.replace(/\n/g, '-'))

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

	// console.log(ztagOutput.replace(/\n/g, '-'))
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
