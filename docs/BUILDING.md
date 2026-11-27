# Building GoodReader

## C++ core

Install CMake 3.25+, Ninja, a C++20 compiler, and vcpkg. Set `VCPKG_ROOT` to a
vcpkg checkout and bootstrap it once. On macOS:

```sh
brew install cmake ninja vcpkg
git clone https://github.com/microsoft/vcpkg.git "$HOME/vcpkg"
export VCPKG_ROOT="$HOME/vcpkg"
"$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
```

Configure, compile, and run tests:

```sh
cmake --preset default
cmake --build --preset default
ctest --preset default
```

For an optimized build, use `release` in place of `default`. To collect LLVM
coverage, configure with `coverage`, run the tests, then export the resulting
profiles with `llvm-profdata` and `llvm-cov`. CI enforces at least 80% line
coverage for `core/library`, `core/sync`, and `core/stats`.

## Android

Android uses Kotlin/Jetpack Compose for presentation and JNI for calls into the
C++ core. Install Android Studio, Android SDK 35+, CMake 3.22.1, and NDK
27.2.12479018. The JNI bridge currently validates selected book paths; the
Kotlin domain/application and SQLDelight layers remain a temporary
specification for the full import/library API:

```sh
./gradlew :apps:android:assembleDebug :apps:android:testDebugUnitTest
./gradlew :apps:android:connectedDebugAndroidTest
```

The connected test command requires an Android emulator or device.

## iOS and macOS

Apple targets use SwiftUI. The current transitional app still imports the
Kotlin `GoodReaderCore` framework; it is not yet wired to the C++ core. Install
Xcode, Java 17, XcodeGen, and Android SDK 35 (the Gradle build configures the
Android project even when only the Apple framework task is requested). Then
from the repository root build and copy the framework:

```sh
export ANDROID_HOME="$HOME/Library/Android/sdk"
export JAVA_HOME=$(/usr/libexec/java_home -v 17)
./gradlew :infrastructure:database-sqldelight:assembleGoodReaderCoreReleaseXCFramework
mkdir -p apps/apple/Frameworks
cp -R infrastructure/database-sqldelight/build/XCFrameworks/release/GoodReaderCore.xcframework \
  apps/apple/Frameworks/
xcodegen generate --spec apps/apple/project.yml --project apps/apple
open apps/apple/GoodReader.xcodeproj
```

In Xcode, select the `GoodReadermacOS` scheme and run it. `--project` expects
the output directory (`apps/apple`), not the `.xcodeproj` bundle path.

## Windows

Windows uses WinUI 3 with C# ViewModels and P/Invoke into the C API. Install
Visual Studio 2022 with the C++ and Windows App SDK workloads. Exact app build
commands will be documented with the Windows project; the C++ core itself
builds with the CMake presets and MSVC.

## Transition checks

Until C++ tests replace the existing Kotlin tests, run the current Kotlin
specification checks as well:

```sh
./gradlew qualityCheck
./gradlew :core:domain:jvmTest :core:application:jvmTest :core:alignment:jvmTest
```

CI is expected to run native core tests on Linux, macOS, and Windows, plus the
Android build and remaining platform checks.
