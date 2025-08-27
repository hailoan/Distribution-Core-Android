rootProject.name = "Distribution-Core-Android"

pluginManagement {
    fun Settings.getEnv(
        key: String,
        defaultValue: String? = null
    ): String = providers.environmentVariable(key).orNull
        ?: defaultValue
        ?: throw IllegalArgumentException("Environment variable '$key' is required but not set")


    val artifactName = getEnv("GITHUB_USERNAME")
    val artifactToken = getEnv("GITHUB_ACCESS_TOKEN")
    val artifactoryUrl = getEnv("GITHUB_PUBLISH")
    repositories {
        google()
        mavenCentral()
        mavenLocal()
        gradlePluginPortal()
        // Example custom Maven repo
        maven {
            name = "core-android"
            url = uri(artifactoryUrl)
            isAllowInsecureProtocol = true
            credentials {
                username = artifactName
                password = artifactToken
            }
        }
    }
}

dependencyResolutionManagement {
    fun Settings.getEnv(
        key: String,
        defaultValue: String? = null
    ): String = providers.environmentVariable(key).orNull
        ?: defaultValue
        ?: throw IllegalArgumentException("Environment variable '$key' is required but not set")

    val artifactName = getEnv("GITHUB_USERNAME")
    val artifactToken = getEnv("GITHUB_ACCESS_TOKEN")
    val artifactoryUrl = getEnv("GITHUB_PUBLISH")
    repositoriesMode.set(RepositoriesMode.PREFER_SETTINGS)
    repositories {
        google()
        mavenCentral()
        mavenLocal()
        gradlePluginPortal()
        // Example custom Maven repo
        maven {
            name = "core-android"
            url = uri(artifactoryUrl)
            isAllowInsecureProtocol = true
            credentials {
                username = artifactName
                password = artifactToken
            }
        }
    }
}


include(":app")
val listModule: List<String> = arrayListOf(
)
listModule.forEach {
    includeBuild("$it") {
        dependencySubstitution {
            substitute(module("com.chiistudio:$it")).using(project(":$it"))
        }
    }
}
include(":plugin")
include(":core")
include(":network")
