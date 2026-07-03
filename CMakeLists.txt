on:
  push:
    branches: [ main ]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout code
        uses: actions/checkout@v3
        with:
          submodules: recursive # Très important pour récupérer les dossiers manquants

      - name: Set up JDK 17
        uses: actions/setup-java@v3
        with:
          java-version: '17'
          distribution: 'temurin'

      - name: Grant execute permission for gradlew
        run: chmod +x gradlew

      - name: Build with Gradle
        # On désactive la doc et les traductions pour éviter les erreurs CMake
        run: ./gradlew assembleRelease -Pbuild_doc=OFF -Pbuild_po=OFF

      - name: Upload APK Artifact
        uses: actions/upload-artifact@v3
        with:
          name: app-release
          path: app/build/outputs/apk/release/*.apk
