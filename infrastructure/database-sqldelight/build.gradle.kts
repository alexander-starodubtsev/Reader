import org.jetbrains.kotlin.gradle.plugin.mpp.KotlinNativeTarget
import org.jetbrains.kotlin.gradle.plugin.mpp.apple.XCFramework

plugins {
    id("goodreader.kotlin-multiplatform")
    alias(libs.plugins.sqldelight)
}

kotlin {
    val goodReaderCore = XCFramework("GoodReaderCore")
    targets.withType<KotlinNativeTarget>().configureEach {
        binaries.framework {
            baseName = "GoodReaderCore"
            isStatic = true
            goodReaderCore.add(this)
        }
    }

    sourceSets {
        commonMain.dependencies {
            implementation(project(":core:domain"))
            implementation(project(":core:application"))
            implementation(libs.sqldelight.runtime)
        }
        commonTest.dependencies {
            implementation(libs.kotlinx.coroutines.test)
        }
        androidMain.dependencies {
            implementation(libs.sqldelight.android.driver)
        }
        jvmMain.dependencies {
            implementation(libs.sqldelight.sqlite.driver)
        }
        nativeMain.dependencies {
            implementation(libs.sqldelight.native.driver)
        }
    }
}

sqldelight {
    databases {
        create("GoodReaderDatabase") {
            packageName.set("com.goodreader.infrastructure.database")
            schemaOutputDirectory.set(file("src/commonMain/sqldelight/databases"))
            verifyMigrations.set(true)
        }
    }
}
