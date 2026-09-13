#!/usr/bin/env python3
"""Inject release signingConfig into Expo-generated Android Gradle files."""

from __future__ import annotations

from pathlib import Path


GROOVY_SNIPPET = """
    def keystorePropertiesFile = rootProject.file("keystore.properties")
    def keystoreProperties = new Properties()
    if (keystorePropertiesFile.exists()) {
        keystoreProperties.load(new FileInputStream(keystorePropertiesFile))
    }

    signingConfigs {
        wcSoundsRelease {
            if (keystorePropertiesFile.exists()) {
                keyAlias keystoreProperties['keyAlias']
                keyPassword keystoreProperties['keyPassword']
                storeFile file(keystoreProperties['storeFile'])
                storePassword keystoreProperties['storePassword']
            }
        }
    }
"""

KOTLIN_SNIPPET = """
    val keystorePropertiesFile = rootProject.file("keystore.properties")
    val keystoreProperties = java.util.Properties()
    if (keystorePropertiesFile.exists()) {
        keystorePropertiesFile.inputStream().use { keystoreProperties.load(it) }
    }

    signingConfigs {
        create("wcSoundsRelease") {
            if (keystorePropertiesFile.exists()) {
                keyAlias = keystoreProperties["keyAlias"] as String
                keyPassword = keystoreProperties["keyPassword"] as String
                storeFile = file(keystoreProperties["storeFile"] as String)
                storePassword = keystoreProperties["storePassword"] as String
            }
        }
    }
"""


def patch_groovy(path: Path) -> None:
    text = path.read_text()
    if "wcSoundsRelease" not in text:
        if "android {" not in text:
            raise SystemExit(f"android {{ not found in {path}")
        text = text.replace("android {", "android {" + GROOVY_SNIPPET, 1)
    text = text.replace(
        "signingConfig signingConfigs.debug",
        "signingConfig signingConfigs.wcSoundsRelease",
    )
    path.write_text(text)
    print(f"patched {path}")


def patch_kotlin(path: Path) -> None:
    text = path.read_text()
    if "wcSoundsRelease" not in text:
        if "android {" not in text:
            raise SystemExit(f"android {{ not found in {path}")
        text = text.replace("android {", "android {" + KOTLIN_SNIPPET, 1)
    text = text.replace(
        'signingConfig = signingConfigs.getByName("debug")',
        'signingConfig = signingConfigs.getByName("wcSoundsRelease")',
    )
    text = text.replace(
        "signingConfig = signingConfigs.debug",
        'signingConfig = signingConfigs.getByName("wcSoundsRelease")',
    )
    path.write_text(text)
    print(f"patched {path}")


def main() -> None:
    groovy = Path("android/app/build.gradle")
    kotlin = Path("android/app/build.gradle.kts")
    if groovy.exists():
        patch_groovy(groovy)
    elif kotlin.exists():
        patch_kotlin(kotlin)
    else:
        raise SystemExit("android/app/build.gradle(.kts) not found - run expo prebuild first")


if __name__ == "__main__":
    main()
