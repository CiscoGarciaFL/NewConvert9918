# Third-party notices

New Convert 9918 uses Qt 6.10.3 dynamically and redistributes the Qt runtime
libraries needed by each platform package. Qt is available under commercial
licenses and open-source licenses including GNU LGPL version 3. The release
packages use the LGPL option; recipients may replace the dynamically linked Qt
libraries with compatible builds.

- Qt project and source: <https://www.qt.io/>
- Qt licensing: <https://www.qt.io/licensing/open-source-lgpl-obligations>
- GNU LGPL version 3 text: <https://www.gnu.org/licenses/lgpl-3.0.html>
- GNU GPL version 3 text incorporated by the LGPL:
  <https://www.gnu.org/licenses/gpl-3.0.html>

Qt modules packaged by the application are limited to those discovered by
Qt's deployment tooling from Qt Core, GUI, Widgets, QML, Quick, Quick Controls,
and their required runtime dependencies and plugins. The exact deployed file
list and Qt SBOM metadata are preserved in the release artifacts.

No ImgSource code or binary is included. The application uses the clean-room
image-loading and conversion implementation in this repository.
