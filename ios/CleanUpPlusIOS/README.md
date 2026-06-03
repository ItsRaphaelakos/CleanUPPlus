# CleanUp+ iOS

This is the native iOS wrapper for CleanUp+.

- SwiftUI UI.
- Objective-C++ bridge in `../CleanUpCoreBridge.h/.mm`.
- Shared C++ scanner core in `../../common/cleanup_core`.
- Bundle ID: `com.cleanup.plus.ios`.
- Minimum iOS: 16.0.

## Build locally on a Mac

```bash
xcodebuild -project CleanUpPlusIOS.xcodeproj \
  -scheme CleanUpPlusIOS \
  -configuration Release \
  -destination "generic/platform=iOS" \
  archive
```

## Build a runnable IPA without owning a Mac

Use the GitHub Actions workflow:

`.github/workflows/ios-signed-ipa.yml`

The workflow runs on a GitHub-hosted macOS machine and exports a signed IPA when these secrets are present:

- `APPLE_TEAM_ID`
- `IOS_BUNDLE_ID`
- `IOS_CERTIFICATE_BASE64`
- `IOS_CERTIFICATE_PASSWORD`
- `IOS_PROVISIONING_PROFILE_BASE64`
- `IOS_KEYCHAIN_PASSWORD`

Read `docs/ios-ipa-on-iphone.md` in the repository root for the exact setup.
