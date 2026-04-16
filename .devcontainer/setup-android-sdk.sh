#!/bin/bash
set -e

ANDROID_SDK_ROOT="/opt/android-sdk"
CMDLINE_TOOLS_VERSION="11076708"
NDK_VERSION="27.2.12479018"

echo "Installing Android SDK..."

sudo mkdir -p "$ANDROID_SDK_ROOT"
sudo chown -R "$(whoami)" "$ANDROID_SDK_ROOT"

cd /tmp
curl -fsSL "https://dl.google.com/android/repository/commandlinetools-linux-${CMDLINE_TOOLS_VERSION}_latest.zip" -o cmdline-tools.zip
unzip -q cmdline-tools.zip -d "$ANDROID_SDK_ROOT"
mkdir -p "$ANDROID_SDK_ROOT/cmdline-tools/latest"
mv "$ANDROID_SDK_ROOT/cmdline-tools/bin"  "$ANDROID_SDK_ROOT/cmdline-tools/latest/bin"
mv "$ANDROID_SDK_ROOT/cmdline-tools/lib"  "$ANDROID_SDK_ROOT/cmdline-tools/latest/lib"
[ -f "$ANDROID_SDK_ROOT/cmdline-tools/NOTICE.txt"         ] && mv "$ANDROID_SDK_ROOT/cmdline-tools/NOTICE.txt"         "$ANDROID_SDK_ROOT/cmdline-tools/latest/"
[ -f "$ANDROID_SDK_ROOT/cmdline-tools/source.properties"  ] && mv "$ANDROID_SDK_ROOT/cmdline-tools/source.properties"  "$ANDROID_SDK_ROOT/cmdline-tools/latest/"
rm -f cmdline-tools.zip

export ANDROID_SDK_ROOT="$ANDROID_SDK_ROOT"
export ANDROID_HOME="$ANDROID_SDK_ROOT"
export ANDROID_NDK_HOME="$ANDROID_SDK_ROOT/ndk/$NDK_VERSION"
export PATH="$ANDROID_SDK_ROOT/cmdline-tools/latest/bin:$ANDROID_SDK_ROOT/platform-tools:$ANDROID_SDK_ROOT/ndk/$NDK_VERSION:$PATH"

yes | sdkmanager --licenses > /dev/null 2>&1 || true
sdkmanager --install \
    "platform-tools" \
    "platforms;android-35" \
    "build-tools;35.0.0" \
    "ndk;${NDK_VERSION}"

echo "Android SDK installed successfully."
echo "  SDK:  $ANDROID_SDK_ROOT"
echo "  NDK:  $ANDROID_NDK_HOME"

PROFILE="/home/$(whoami)/.bashrc"
{
    echo ""
    echo "# Android SDK"
    echo "export ANDROID_SDK_ROOT=$ANDROID_SDK_ROOT"
    echo "export ANDROID_HOME=$ANDROID_SDK_ROOT"
    echo "export ANDROID_NDK_HOME=$ANDROID_SDK_ROOT/ndk/$NDK_VERSION"
    echo 'export PATH="$ANDROID_SDK_ROOT/cmdline-tools/latest/bin:$ANDROID_SDK_ROOT/platform-tools:$ANDROID_SDK_ROOT/ndk/'"$NDK_VERSION"':$PATH"'
} >> "$PROFILE"
