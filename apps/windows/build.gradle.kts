plugins {
    alias(libs.plugins.kotlin.multiplatform)
    alias(libs.plugins.kotlin.compose)
    alias(libs.plugins.compose.multiplatform)
    alias(libs.plugins.ktlint)
    alias(libs.plugins.detekt)
}

kotlin {
    jvmToolchain(17)
    jvm()

    sourceSets {
        commonMain.dependencies {
            implementation(project(":core:domain"))
            implementation(project(":core:application"))
            implementation(project(":infrastructure:database-sqldelight"))
            implementation(compose.runtime)
            implementation(compose.foundation)
            implementation(compose.material3)
            implementation(compose.ui)
            implementation(libs.kotlinx.coroutines.core)
        }
        commonTest.dependencies {
            implementation(kotlin("test"))
        }
        jvmTest.dependencies {
            implementation(libs.kotlinx.coroutines.test)
        }
        jvmMain.dependencies {
            implementation(compose.desktop.currentOs)
            implementation(libs.sqldelight.sqlite.driver)
            implementation(libs.jaudiotagger)
            implementation("org.apache.pdfbox:pdfbox:3.0.4")
        }
    }
}

compose.desktop {
    application {
        mainClass = "com.goodreader.windows.MainKt"
    }
}

detekt {
    buildUponDefaultConfig = true
    config.setFrom(files(rootProject.file("config/detekt/detekt.yml")))
}

ktlint {
    version.set("1.5.0")
}
