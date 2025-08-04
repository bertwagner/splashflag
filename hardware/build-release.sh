#!/bin/bash
VERSION=$1

if [ -z "$VERSION" ]; then
    echo "Usage: ./build-release.sh v1.0.1"
    exit 1
fi

# Remove 'v' prefix for version number
VERSION_NUMBER=${VERSION#v}

echo "Building SplashFlag firmware version: $VERSION"

# Update version in secrets.h
echo "Updating version in secrets.h..."
sed -i '' "s/#define FIRMWARE_VERSION \".*\"/#define FIRMWARE_VERSION \"$VERSION_NUMBER\"/" src/secrets.h

# Build firmware
echo "Building firmware..."
pio run

# Check if build succeeded
if [ $? -eq 0 ]; then
    echo "✅ Build successful!"
    
    # Create releases directory
    mkdir -p releases
    
    # Create the firmware.bin file for release
    cp .pio/build/arduino_nano_esp32/firmware.bin releases/firmware.bin
    
    echo "📦 Binary file created:"
    echo "  - releases/firmware.bin"
    
    # Create release in the PRIVATE splashflag-releases repository
    echo "🚀 Creating release in bertwagner/splashflag-releases..."
    
    # Ensure version has 'v' prefix for GitHub release
    if [[ ! $VERSION == v* ]]; then
        VERSION="v$VERSION"
    fi
    
    # Use gh CLI to create release in the private releases repo
    gh release create $VERSION \
        releases/firmware.bin \
        --repo bertwagner/splashflag-releases \
        --title "SplashFlag $VERSION" \
        --notes "SplashFlag firmware release $VERSION

Built from: bertwagner/splashflag@$(git rev-parse --short HEAD)"
    
    if [ $? -eq 0 ]; then
        echo "✅ Release created successfully in bertwagner/splashflag-releases"
        echo "🔗 Devices will automatically detect and install this update"
        
        # Optionally tag the source repo (but don't create release there)
        echo "📌 Tagging source repository..."
        git tag $VERSION
        git push origin $VERSION
        
        echo ""
        echo "🎉 Release $VERSION complete!"
        echo "   Source code: tagged in bertwagner/splashflag"
        echo "   Binary release: published to bertwagner/splashflag-releases"
        
    else
        echo "❌ Failed to create release in bertwagner/splashflag-releases"
        echo "Make sure you have access to the private repository"
        exit 1
    fi
    
else
    echo "❌ Build failed!"
    exit 1
fi