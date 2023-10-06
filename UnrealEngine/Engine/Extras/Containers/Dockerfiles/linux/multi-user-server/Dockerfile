ARG UNREAL_ENGINE_RELEASE
FROM ghcr.io/epicgames/unreal-engine:dev-${UNREAL_ENGINE_RELEASE} as source

# Gather the required files for the UnrealMultiUserServer binary and its dependencies
COPY copy-target.py /tmp/copy-target.py
RUN python3 /tmp/copy-target.py '/home/ue4/UnrealEngine/Engine' '/tmp/gathered/Engine' UnrealMultiUserServer Linux

# Copy the gathered files from the Unreal Engine development image
FROM gcr.io/distroless/cc-debian10
COPY --from=source /tmp/gathered/Engine /opt/Engine

# Expose UDP port 9999
EXPOSE 9999/udp

# Set the UnrealMultiUserServer binary as the container's entrypoint
ENTRYPOINT ["/opt/Engine/Binaries/Linux/UnrealMultiUserServer"]
