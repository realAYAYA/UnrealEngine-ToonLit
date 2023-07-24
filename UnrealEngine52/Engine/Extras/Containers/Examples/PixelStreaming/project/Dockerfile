ARG RELEASE
FROM ghcr.io/epicgames/unreal-engine:dev-${RELEASE} as builder

# Clone the source code for the example Unreal project
RUN git clone --progress --depth=1 'https://gitlab.com/ue4-test-projects/4.26/firstpersoncxx.git' /tmp/project

# Enable the Pixel Streaming plugin in the example Unreal project
RUN sed -i 's|"Modules"|"Plugins": [{"Name": "PixelStreaming", "Enabled": true}],\n\t"Modules"|' /tmp/project/FirstPersonCxx.uproject

# Package the example Unreal project
RUN /home/ue4/UnrealEngine/Engine/Build/BatchFiles/RunUAT.sh BuildCookRun \
	-clientconfig=Development -serverconfig=Development \
	-project=/tmp/project/FirstPersonCxx.uproject \
	-utf8output -nodebuginfo -allmaps -noP4 -cook -build -stage -prereqs -pak -archive \
	-archivedirectory=/tmp/project/dist \
	-platform=Linux

# Copy the packaged project into the Pixel Streaming runtime image
FROM ghcr.io/epicgames/unreal-engine:runtime-pixel-streaming
COPY --from=builder --chown=ue4:ue4 /tmp/project/dist/Linux /home/ue4/project

# Set the project as the container's entrypoint
ENTRYPOINT ["/home/ue4/project/FirstPersonCxx.sh"]
