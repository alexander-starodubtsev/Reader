plugins {
    alias(libs.plugins.kotlin.multiplatform) apply false
    alias(libs.plugins.kotlin.android) apply false
    alias(libs.plugins.android.application) apply false
    alias(libs.plugins.android.library) apply false
    alias(libs.plugins.kotlin.compose) apply false
    alias(libs.plugins.compose.multiplatform) apply false
    alias(libs.plugins.sqldelight) apply false
    alias(libs.plugins.ktlint) apply false
    alias(libs.plugins.detekt) apply false
    alias(libs.plugins.kover) apply false
}

val kotlinCodeProjects = listOf(
    ":core:domain",
    ":core:application",
    ":core:alignment",
    ":infrastructure:database-sqldelight",
    ":apps:android",
    ":apps:windows",
)

tasks.register("qualityCheck") {
    group = "verification"
    description = "Runs tests, formatting, static analysis, and coverage verification."
    dependsOn(
        "test",
        "ktlintCheck",
        "detekt",
        ":core:domain:koverVerify",
        ":core:application:koverVerify",
    )
}

tasks.register("test") {
    group = "verification"
    description = "Runs shared JVM and Android unit tests."
    dependsOn(
        ":core:domain:jvmTest",
        ":core:application:jvmTest",
        ":core:alignment:jvmTest",
        ":infrastructure:database-sqldelight:jvmTest",
        ":apps:android:testDebugUnitTest",
        ":apps:windows:jvmTest",
    )
}

tasks.register("ktlintCheck") {
    group = "verification"
    description = "Checks Kotlin formatting in all code modules."
    dependsOn(kotlinCodeProjects.map { "$it:ktlintCheck" })
}

tasks.register("detekt") {
    group = "verification"
    description = "Runs Detekt on all Kotlin code modules."
    dependsOn(kotlinCodeProjects.map { "$it:detekt" })
}

tasks.register("ktlintFormat") {
    group = "formatting"
    description = "Formats Kotlin source and build scripts in all code modules."
    dependsOn(kotlinCodeProjects.map { "$it:ktlintFormat" })
}
