#
# NOTE: This Dockerfile must be built with the Engine directory as CWD.
#       In the Horde build script we stage the required files before running:
#
#         docker build -f Source/Programs/Horde/Horde.Server/Dockerfile . 
#
#       Running directly from the source tree will cause a lot of unnecessary
#       files to be uploaded to Docker before building.

ARG code_coverage="false"

FROM redis:7.2 AS redis

# Build image
FROM mcr.microsoft.com/dotnet/sdk:8.0 AS build-env
WORKDIR /app
COPY Source/Programs/Horde/nuget.config ./Source/Programs/Horde/nuget.config
RUN DEBIAN_FRONTEND=noninteractive \
	apt-get -qq update && \
	apt-get -qq install curl zip && \
	dotnet tool install --global JetBrains.dotCover.GlobalTool && \
	ln -s /root/.dotnet/tools/dotnet-dotcover /usr/local/bin/dotnet-dotcover

COPY --from=redis /usr/local/bin/redis-server /usr/local/bin/redis-server

# Since the .deb does not install in this image, just download it and extract the static binary
RUN wget https://repo.mongodb.org/apt/debian/dists/bookworm/mongodb-org/7.0/main/binary-amd64/mongodb-org-server_7.0.4_amd64.deb && \
	dpkg -x mongodb-org-server_7.0.4_amd64.deb /tmp/mongodb && \
	cp /tmp/mongodb/usr/bin/mongod /usr/local/bin/mongod

# Restore all the C# projects
COPY Source/Programs/Shared/EpicGames.Core/*.csproj ./Source/Programs/Shared/EpicGames.Core/
COPY Source/Programs/Shared/EpicGames.Horde/*.csproj ./Source/Programs/Shared/EpicGames.Horde/
COPY Source/Programs/Shared/EpicGames.IoHash/*.csproj ./Source/Programs/Shared/EpicGames.IoHash/
COPY Source/Programs/Shared/EpicGames.Perforce/*.csproj ./Source/Programs/Shared/EpicGames.Perforce/
COPY Source/Programs/Shared/EpicGames.Perforce.Managed/*.csproj ./Source/Programs/Shared/EpicGames.Perforce.Managed/
COPY Source/Programs/Shared/EpicGames.Redis/*.csproj ./Source/Programs/Shared/EpicGames.Redis/
COPY Source/Programs/Shared/EpicGames.Serialization/*.csproj ./Source/Programs/Shared/EpicGames.Serialization/
COPY Source/Programs/Shared/EpicGames.Serialization.Tests/*.csproj ./Source/Programs/Shared/EpicGames.Serialization.Tests/
COPY Source/Programs/Horde/Horde.Agent/*.csproj ./Source/Programs/Horde/Horde.Agent/
COPY Source/Programs/Horde/Horde.Server/*.csproj ./Source/Programs/Horde/Horde.Server/
COPY Source/Programs/Horde/Horde.Server.Tests/*.csproj ./Source/Programs/Horde/Horde.Server.Tests/
RUN dotnet restore Source/Programs/Horde/Horde.Server/Horde.Server.csproj -p:WithHordeStorage=false

# Build the server
COPY Binaries/DotNET/EpicGames.Perforce.Native/ Binaries/DotNET/EpicGames.Perforce.Native/
COPY Source/Programs/Shared/ Source/Programs/Shared/
COPY Source/Programs/Horde/.editorconfig Source/Programs/Horde/.editorconfig
COPY Source/Programs/Horde/Horde.Server/ Source/Programs/Horde/Horde.Server/
COPY Source/Programs/AutomationTool/AutomationUtils/Matchers/ Source/Programs/AutomationTool/AutomationUtils/Matchers/
COPY Source/Programs/UnrealBuildTool/Matchers/ Source/Programs/UnrealBuildTool/Matchers/
ARG msbuild_args
RUN dotnet publish Source/Programs/Horde/Horde.Server/Horde.Server.csproj -c Release -o out -p:WithHordeStorage=false $msbuild_args -p:RunAnalyzersDuringBuild=True

# Run tests
COPY Source/Programs/Horde/Horde.Agent/ Source/Programs/Horde/Horde.Agent/
COPY Source/Programs/Horde/Horde.Server.Tests/ Source/Programs/Horde/Horde.Server.Tests/
COPY Source/Programs/Shared/EpicGames.Serialization.Tests/ Source/Programs/Shared/EpicGames.Serialization.Tests/

ARG code_coverage
COPY Source/Programs/Horde/Scripts/test.sh Source/Programs/Horde/Scripts/test.sh
RUN bash Source/Programs/Horde/Scripts/test.sh

# Remove native libs not used on Linux x86_64
RUN rm -rf /app/out/runtimes/osx* && \
    rm -rf /app/out/runtimes/win* && \
    rm -rf /app/out/runtimes/linux-arm* && \
    rm -rf /app/out/runtimes/linux/native/libgrpc_csharp_ext.x86.so


# Placeholder stage that's used to produce smaller output (only specific files from build-env stage is copied)
# To retrieve code coverage reports, docker buildx can be run with `--target build-env-output --output type=local,dest=docker-output`
FROM scratch AS build-env-output
# Provide at least one file that exists (/tmp/empty), so COPY operation won't fail
COPY --from=build-env /tmp/empty /tmp/dotcover-report/*.zip /

# Create the runtime image
FROM mcr.microsoft.com/dotnet/aspnet:8.0 AS runtime
WORKDIR /app
COPY --from=build-env /app/out .
ENTRYPOINT [ "dotnet", "HordeServer.dll" ]
