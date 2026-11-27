pluginManagement {
    includeBuild("build-logic")
    repositories {
        google()
        maven { url = uri("https://maven-central.storage-download.googleapis.com/maven2/") }
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        maven { url = uri("https://maven-central.storage-download.googleapis.com/maven2/") }
        mavenCentral()
    }
}

rootProject.name = "GoodReader"

include(
    ":core:domain",
    ":core:application",
    ":core:alignment",
    ":infrastructure:database-sqldelight",
    ":apps:android",
    ":apps:windows",
)
