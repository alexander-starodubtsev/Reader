plugins {
    id("goodreader.kotlin-multiplatform")
    alias(libs.plugins.kover)
}

kotlin {
    sourceSets {
        commonMain.dependencies {
            implementation(libs.kotlinx.datetime)
        }
    }
}

kover {
    reports {
        total {
            verify {
                rule {
                    minBound(80)
                }
            }
        }
    }
}
