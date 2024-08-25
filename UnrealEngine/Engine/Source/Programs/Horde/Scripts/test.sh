#!/bin/bash
# Run tests (run from Dockerfile)

export UE_DOTNET_VERSION=net8.0

# Start Redis and MongoDB in the background for tests to use
redis-server --save "" --appendonly no --daemonize yes || exit 1
mongod --noauth --quiet --fork --dbpath /tmp/mongodb --logpath /tmp/mongod.log || exit 1

testProjects=(
	"Source/Programs/Shared/EpicGames.BuildGraph.Tests/EpicGames.BuildGraph.Tests.csproj"
	"Source/Programs/Shared/EpicGames.Core.Tests/EpicGames.Core.Tests.csproj"
	"Source/Programs/Shared/EpicGames.Horde.Tests/EpicGames.Horde.Tests.csproj"
	"Source/Programs/Shared/EpicGames.IoHash.Tests/EpicGames.IoHash.Tests.csproj"
	"Source/Programs/Shared/EpicGames.Redis.Tests/EpicGames.Redis.Tests.csproj"
	"Source/Programs/Shared/EpicGames.Serialization.Tests/EpicGames.Serialization.Tests.csproj"
	"Source/Programs/Horde/Horde.Server.Tests/Horde.Server.Tests.csproj"
)

for csProj in "${testProjects[@]}"; do
	filename="${csProj##*/}"
	args=("test")
	if [ "$code_coverage" = "true" ]; then
		args=(dotcover test --dcOutput=/tmp/${filename}.dcvr --dcFilters="+:EpicGames*;+:Horde*;-:*.Tests")
	fi
	dotnet "${args[@]}" "$csProj" --blame-hang-timeout 5m --blame-hang-dump-type mini --logger 'console;verbosity=normal' || exit 1
done

mkdir /tmp/dotcover-report
touch /tmp/empty
if [ "$code_coverage" = "true" ]; then
	dotnet dotcover merge --source=/tmp/*.dcvr --output=/tmp/dotcover-merged.dcvr
	dotnet dotcover report --source=/tmp/dotcover-merged.dcvr --output=/tmp/dotcover-report/report.html:/tmp/dotcover-report/report.json --reportType=HTML,JSON
	zip -r /tmp/dotcover-report/dotcover-report.zip /tmp/dotcover-report
fi