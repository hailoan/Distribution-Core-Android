plugins {
    `kotlin-dsl`                // If writing in Kotlin
    `java-gradle-plugin`        // Required for Gradle plugins
    `maven-publish`             // Required to publish to Maven Local
}

group = "com.chiistudio"        // Maven group ID
version = "1.0.0"               // Plugin version

gradlePlugin {
    plugins {
        create("chiistudioPlugin") {
            id = "com.chiistudio.plugin" // Plugin ID used in `plugins {}` block
            implementationClass = "com.chiistudio.plugin.PublishConfigPlugin"
        }
    }
}

repositories {
    gradlePluginPortal()
    mavenCentral()
}

publishing {
    repositories {
        mavenLocal() // Enable publishing to ~/.m2/repository
        maven {
            name = "core-android"
            url = uri(System.getenv("GITHUB_PUBLISH"))
            credentials {
                username = findProperty("username") as String? ?: System.getenv("GITHUB_USERNAME")
                password = findProperty("access_token") as String? ?: System.getenv("GITHUB_ACCESS_TOKEN")
            }
        }
    }
}

dependencies {
    // Example dependencies for plugin development
    implementation(kotlin("stdlib"))
    implementation(libs.gradle)
}