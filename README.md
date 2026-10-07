# Zeus Gateway app (Android APK via GitHub Actions)

1. Create a new GitHub repository (private is fine). No credentials are stored in this project.
2. Upload these files: package.json, capacitor.config.json, .gitignore, README.md and the www folder.
3. In the repository choose Add file > Create new file. Name it `.github/workflows/build-apk.yml`
   (typing the slashes creates the folders) and paste the contents of the same file from this zip. Commit.
4. Open the Actions tab > "Build APK" > Run workflow. Wait about 5-10 minutes.
5. Open the finished run, scroll to Artifacts, download `zeus-gateway-apk`, unzip it to get `app-debug.apk`.
6. Copy the APK to your phone and open it. Allow "install from this source" when asked.
7. Open the app > Settings and enter the HiveMQ address, port 8884, username and password.

The APK is debug-signed for your own use. It is not suitable for the Play Store.
