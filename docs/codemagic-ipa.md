# Build CleanUp+ IPA with Codemagic

Codemagic can provide the cloud Mac/Xcode machine. The project is ready for it through:

`codemagic.yaml`

## In the screen you opened

1. Choose **GitHub**.
2. Click **Next: Authorize Codemagic**.
3. Select the repository where you pushed `C:\Users\rafad\Documents\CleanUP+`.
4. Let Codemagic scan the repo.
5. Select the `ios-development-ipa` workflow.

## Code signing setup

To install the IPA on your iPhone, iOS requires signing.

In Codemagic:

1. Open **Team settings**.
2. Go to **codemagic.yaml settings**.
3. Open **Code signing identities**.
4. Add an iOS certificate.
5. Add a provisioning profile for bundle ID:

   `com.cleanup.plus.ios`

For direct install on your phone, use a **Development** provisioning profile that includes your iPhone UDID, or use **Ad Hoc** if your account/profile supports it.

The `codemagic.yaml` workflow currently uses:

```yaml
ios_signing:
  distribution_type: development
  bundle_identifier: com.cleanup.plus.ios
```

## Workflows

`ios-development-ipa`

Builds a signed IPA. This is the one you want for your phone.

`ios-simulator-check`

Builds without signing for the iOS Simulator. This is useful only to check compile errors.

## Important

Codemagic can be free for build minutes, but Apple still requires valid signing assets before an IPA can run on a real iPhone.
