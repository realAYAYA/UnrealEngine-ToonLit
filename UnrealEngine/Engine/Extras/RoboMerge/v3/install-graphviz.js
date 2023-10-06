#!/usr/bin/env node
// Copyright Epic Games, Inc. All Rights Reserved.

const fs = require('fs')
const gzip = require('node-gzip').gzip

Promise.all([
	fs.promises.copyFile('node_modules/@hpcc-js/wasm/dist/index.min.js', 'public/js/hpcc-wasm-index.min.js'),
	Promise.all([
		fs.promises.mkdir('public/bin').catch(() => {}),
		fs.promises.readFile('node_modules/@hpcc-js/wasm/dist/graphvizlib.wasm')
		.then(gzip)
	])
	.then(([_, compressed]) => {
		fs.promises.writeFile('public/bin/graphvizlib.wasm.gz', compressed);
	})
])
.then(() => {
	console.log('Installed GraphViz binary for web app')
})