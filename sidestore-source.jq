# AltStore/SideStore source format: https://faq.altstore.io/developers/make-a-source
{
  name: "Math Notes",
  identifier: "dev.zack.mathnotes.source",
  apps: [{
    name: "Math Notes",
    bundleIdentifier: "dev.zack.mathnotes",
    developerName: "Zack Garza",
    localizedDescription: "Personal math notes app.",
    iconURL: "https://github.com/dzackgarza.png",
    versions: [{
      version: $version,
      buildVersion: $build,
      date: $date,
      size: $size,
      minOSVersion: "18.0",
      downloadURL: "\($base)/MathNotes.ipa"
    }]
  }],
  news: []
}
