param(
    [Parameter(Mandatory = $true)]
    [string]$CertificateP12,

    [Parameter(Mandatory = $true)]
    [string]$ProvisioningProfile,

    [Parameter(Mandatory = $true)]
    [string]$AppleTeamId,

    [string]$BundleId = "com.cleanup.plus.ios"
)

$ErrorActionPreference = "Stop"

$certificatePath = Resolve-Path -LiteralPath $CertificateP12
$profilePath = Resolve-Path -LiteralPath $ProvisioningProfile

$certificateBase64 = [Convert]::ToBase64String([IO.File]::ReadAllBytes($certificatePath.Path))
$profileBase64 = [Convert]::ToBase64String([IO.File]::ReadAllBytes($profilePath.Path))

Write-Host ""
Write-Host "Add these GitHub Actions secrets to your repo:"
Write-Host ""
Write-Host "APPLE_TEAM_ID"
Write-Host $AppleTeamId
Write-Host ""
Write-Host "IOS_BUNDLE_ID"
Write-Host $BundleId
Write-Host ""
Write-Host "IOS_CERTIFICATE_BASE64"
Write-Host $certificateBase64
Write-Host ""
Write-Host "IOS_PROVISIONING_PROFILE_BASE64"
Write-Host $profileBase64
Write-Host ""
Write-Host "Also add these manually:"
Write-Host ""
Write-Host "IOS_CERTIFICATE_PASSWORD"
Write-Host "The password used when exporting the .p12 certificate."
Write-Host ""
Write-Host "IOS_KEYCHAIN_PASSWORD"
Write-Host "Any strong temporary password used by the GitHub runner keychain."
Write-Host ""
