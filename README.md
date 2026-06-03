# CleanUp+

CleanUp+ is a native Android storage cleaner and duplicate finder with a shared C++ core that can also be bridged into iOS.

## What is included

- `common/cleanup_core`: cross-platform C++17 scanner core.
- `android`: Android app using Kotlin, Jetpack Compose, Material 3, CMake and Android NDK.
- `ios`: native iOS SwiftUI app project plus Objective-C++ bridge for calling the same C++ core.

## Core features

- Scans filesystem roots with `std::filesystem`.
- Finds duplicate photos by file size and content hash.
- Marks the newest file in each duplicate group as the one to keep.
- Finds files larger than the configurable threshold, defaulting to 100 MB.
- Finds screenshots by path/name.
- Finds downloads under `Download` or `Downloads`.
- Finds empty folders.
- Deletes only after an explicit `confirmed = true` call.
- Refuses broad unsafe delete targets and only deletes folders when they are empty.

## Android behavior

The Android app requests storage access and scans the shared storage root returned by `Environment.getExternalStorageDirectory()`.

For Android 11 and newer, a full-device cleaner needs the special **All files access** permission (`MANAGE_EXTERNAL_STORAGE`). Without it, Android scoped storage prevents reliable full-path scanning and deleting. This permission is heavily restricted for Play Store distribution, but it is the correct capability for a real storage cleaner outside a limited media-only scan.

The UI includes:

- Home screen with total, used and available storage.
- Scan screen with current rules and permission state.
- Results screen with file rows, checkboxes, image thumbnails, size, modified date and path.
- "Keep newest" selection for duplicate photos.
- "Delete Selected" with confirmation dialog before native deletion.
- Settings for large-file threshold, screenshot/download inclusion and theme mode.

## Build Android

1. Open `android/` in Android Studio.
2. Install Android SDK 36, CMake and NDK when prompted.
3. If needed, create `android/local.properties`:

   ```properties
   sdk.dir=C\:\\Users\\YOUR_USER\\AppData\\Local\\Android\\Sdk
   ```

4. Run the `app` configuration on a device or emulator.

Command-line build once the SDK is installed:

```powershell
cd android
gradle :app:assembleDebug
```

This workspace also includes a local SDK under `.android-sdk/` and a machine-local
`android/local.properties` so the APK can be built from this Windows machine.

## Built deliverables

Ready-to-open files are in `dist/`:

- `CleanUpPlus-debug.apk`
- `CleanUpPlus_iOS_Source.zip`
- `Viewer_android.exe`
- `Viewer_ios.exe`

The two viewer executables are interactive Windows previews. They show CleanUp+
inside a phone frame, include a console panel for actions/errors, and let you
click Scan, navigation tabs, result rows, Keep newest, Delete Selected and
settings controls.

## Test the C++ core

The C++ core can be built and tested without Android:

```powershell
cmake -S common/cleanup_core -B build/cleanup_core
cmake --build build/cleanup_core --config Debug
ctest --test-dir build/cleanup_core -C Debug --output-on-failure
```

## iOS build and IPA notes

The `ios/CleanUpPlusIOS` folder is a full SwiftUI Xcode project. It includes:

- SwiftUI screens for Home, Scan, Results and Settings.
- Objective-C++ bridge in `ios/CleanUpCoreBridge.h/.mm`.
- Shared C++ scanner core from `common/cleanup_core`.
- App icon asset and shared Xcode scheme.

iOS does not allow arbitrary full-device scanning. The iOS app scans app-accessible storage such as Documents and temporary files.

To build an installable IPA without a local Mac, use the GitHub Actions workflow:

`.github/workflows/ios-signed-ipa.yml`

You still need Apple signing secrets for your own iPhone. See `docs/ios-ipa-on-iphone.md`.

## Next production steps

- Add MediaStore-backed Android scanning for a Play-Store-friendly mode.
- Add trash/recycle-bin behavior before permanent deletion if product requirements call for undo.
- Add background work with notifications for very large libraries.
- Add instrumented Android tests around permissions and deletion flows.
