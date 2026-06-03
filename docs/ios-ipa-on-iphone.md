# Build a runnable CleanUp+ IPA for your iPhone

This project can produce an installable IPA through GitHub Actions, but iOS still requires Apple signing assets.

## What you need

- An Apple Developer account.
- Your iPhone registered in the Apple Developer portal by UDID.
- An iOS Development certificate exported as `.p12`.
- An iOS App Development provisioning profile for bundle ID `com.cleanup.plus.ios`, including your iPhone.

Without a provisioning profile that includes your iPhone UDID, the IPA may build but it will not open/install on your phone.

## GitHub secrets

Add these secrets in your GitHub repo:

- `APPLE_TEAM_ID`
- `IOS_BUNDLE_ID` with value `com.cleanup.plus.ios`
- `IOS_CERTIFICATE_BASE64`
- `IOS_CERTIFICATE_PASSWORD`
- `IOS_PROVISIONING_PROFILE_BASE64`
- `IOS_KEYCHAIN_PASSWORD`

Use this helper from Windows to encode the `.p12` and `.mobileprovision` files:

```powershell
.\scripts\encode-ios-signing-files.ps1 `
  -CertificateP12 "C:\path\to\certificate.p12" `
  -ProvisioningProfile "C:\path\to\profile.mobileprovision" `
  -AppleTeamId "YOUR_TEAM_ID"
```

Copy the printed values into GitHub Actions secrets.

## Run the build

1. Push this project to GitHub.
2. Open GitHub Actions.
3. Run **Build signed iOS IPA**.
4. Download the `CleanUpPlus-iOS-IPA` artifact.
5. Install the `.ipa` on the registered iPhone with Finder, Apple Configurator, or another trusted sideload tool.

## Why this is needed

The Windows machine can create Android APKs and source packages, but Apple only allows iOS app compilation/signing through Xcode on macOS. The workflow uses GitHub's macOS runner as the Mac build machine.
