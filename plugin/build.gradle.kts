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
    }
}

dependencies {
    // Example dependencies for plugin development
    implementation(kotlin("stdlib"))
    implementation(libs.gradle)
}