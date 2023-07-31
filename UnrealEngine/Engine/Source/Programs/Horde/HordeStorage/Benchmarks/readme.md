These here are benchmarks that can be run using SuperBenchmarker
`choco install SuperBenchmarker`

# Insert random blobs
`sb -u https://dev-jupiter.northeurope.cloudapp.azure.com/api/v1/c/ue4.ddc/test/{{{name}}} -n 100 -c 16 -g 2 -t .\templates\putBlobs.txt -f .\templates\putBlobs.csv -m PUT -B -b`

# Fetch random blobs
Note that all of these may not succeed depending on which blobs were inserted in the past

`sb -u https://dev-jupiter.northeurope.cloudapp.azure.com/api/v1/c/ue4.ddc/test/{{{name}}} -n 100 -c 16 -g 2 -t .\templates\putBlobs.txt -f .\templates\putBlobs.csv -B -b`
