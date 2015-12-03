#!/bin/sh

ndk-build NDK_TOOLCHAIN_VERSION=4.8 NDK_DEBUG=0 V=0 -j8 APP_CFLAGS="-w -fpermissive"
ant release
jarsigner -verbose -sigalg SHA1withRSA -digestalg SHA1 -keystore ../../../myks.keystore bin/quakeremake-release-unsigned.apk xashdroid -tsa https://timestamp.geotrust.com/tsa
/home/a1ba/.android/android-sdk-linux/build-tools/22.0.1/zipalign 4 bin/quakeremake-release-unsigned.apk bin/quakeremake.apk
#adb install -r -f bin/cs16-client-debug.apk
