# Copyright Epic Games, Inc. All Rights Reserved.

from PIL import Image

def MakeIcon(folder, output):
    sizes = [256, 48, 32, 24, 16]
    images = []
    for size in sizes:
        print(f"Loading {folder}/icon_{size}x{size}.png")
        image = Image.open(f"{folder}/icon_{size}x{size}.png")
        images.append(image)
    print(f"Creating {output}")
    images[0].save(f"{output}", append_images=images)

MakeIcon("LiveLinkHub", "LiveLinkHub.ico")
