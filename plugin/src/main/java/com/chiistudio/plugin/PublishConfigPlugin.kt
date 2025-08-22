package com.chiistudio.plugin
import org.gradle.api.Plugin
import org.gradle.api.Project
import org.gradle.api.publish.maven.MavenPublication
import org.gradle.kotlin.dsl.*

class PublishConfigPlugin : Plugin<Project> {
    override fun apply(target: Project) {
        with(target) {
            group = findProperty("GROUP_ID") as String
            version = findProperty("VERSION_NAME") as String

            plugins.apply("maven-publish")

            afterEvaluate {
                extensions.configure<org.gradle.api.publish.PublishingExtension>("publishing") {
                    publications {
                        create<MavenPublication>("release") {
                            // If Android library
                            if (components.findByName("release") != null) {
                                from(components["release"])
                            }
                            // If plain Java or Gradle plugin
                            else if (components.findByName("java") != null) {
                                from(components["java"])
                            }
                            // Fallback
                            else {
                                from(components.first())
                            }

                            groupId = findProperty("GROUP_ID") as String
                            artifactId = findProperty("ARTIFACT_ID") as String
                            version = findProperty("VERSION_NAME") as String

                            pom {
                                name.set(findProperty("POM_NAME") as String)
                                description.set(findProperty("POM_DESCRIPTION") as String)
                                url.set(findProperty("POM_URL") as String)
                                licenses {
                                    license {
                                        name.set(findProperty("POM_LICENSE_NAME") as String)
                                        url.set(findProperty("POM_LICENSE_URL") as String)
                                    }
                                }
                                developers {
                                    developer {
                                        id.set(findProperty("POM_DEVELOPER_ID") as String)
                                        name.set(findProperty("POM_DEVELOPER_NAME") as String)
                                        email.set(findProperty("POM_DEVELOPER_EMAIL") as String)
                                    }
                                }
                            }
                        }
                    }

                    repositories {
                        mavenLocal()
                        maven {
                            name = "core-android"
                            url = uri(System.getenv("GITHUB_PUBLISH"))
                            isAllowInsecureProtocol = true
                            credentials {
                                username = findProperty("username") as String? ?: System.getenv("GITHUB_USERNAME")
                                password = findProperty("access_token") as String? ?: System.getenv("GITHUB_ACCESS_TOKEN")
                            }
                        }
                    }
                }
            }
        }
    }
}