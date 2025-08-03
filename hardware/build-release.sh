#!/bin/bash
  VERSION=$1

  if [ -z "$VERSION" ]; then
      echo "Usage: ./build-release.sh v1.0.1"
      exit 1
  fi

  # Update version in secrets.h
  sed -i '' "s/#define FIRMWARE_VERSION \".*\"/#define FIRMWARE_VERSION \"${VERSION#v}\"/"
  src/secrets.h

  # Build firmware
  echo "Building firmware..."
  pio run

  # Check if build succeeded
  if [ $? -eq 0 ]; then
      echo "Build successful! Binary location:"
      echo ".pio/build/arduino_nano_esp32/firmware.bin"

      # Optional: Copy to releases folder
      mkdir -p releases
      cp .pio/build/arduino_nano_esp32/firmware.bin releases/splashflag-${VERSION}.bin

       git tag v${VERSION}
        git push origin v${VERSION}

        # Create release and upload binary
        gh release create v${VERSION}  releases/splashflag-${VERSION}.bin \
            --title "SplashFlag v${VERSION}" 
  else
      echo "Build failed!"
      exit 1
  fi