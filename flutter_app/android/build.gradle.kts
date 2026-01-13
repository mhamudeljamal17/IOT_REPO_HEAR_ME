import org.gradle.api.file.Directory
import org.gradle.api.tasks.Delete

buildscript {
    // Kotlin version can be adjusted if your project needs a different one
    val kotlinVersion = "1.8.22"

    repositories {
        google()
        mavenCentral()
    }

    dependencies {
        // Android Gradle Plugin (keep this version if your project already uses it)
        classpath("com.android.tools.build:gradle:7.1.2")

        classpath("com.google.gms:google-services:4.3.15")

        // Kotlin Gradle plugin
        classpath("org.jetbrains.kotlin:kotlin-gradle-plugin:$kotlinVersion")
    }
}

allprojects {
    repositories {
        google()
        mavenCentral()
    }
}

// Keep your custom build directory setup
val newBuildDir: Directory = rootProject.layout.buildDirectory.dir("../../build").get()
rootProject.layout.buildDirectory.value(newBuildDir)

subprojects {
    val newSubprojectBuildDir: Directory = newBuildDir.dir(project.name)
    project.layout.buildDirectory.value(newSubprojectBuildDir)
}

subprojects {
    project.evaluationDependsOn(":app")
}

tasks.register<Delete>("clean") {
    delete(rootProject.layout.buildDirectory)
}
