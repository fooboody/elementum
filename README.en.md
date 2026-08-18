Language: [日本語](README.md)

# Elementum

> **Elementum is an independent, unofficial fork of [Kushview Element](https://github.com/kushview/element).**
> It is not affiliated with, endorsed by, or supported by Kushview, LLC. This repository retains the upstream project's licensing and attribution while maintaining its own implementation choices and documentation.

Elementum is a modular audio plug-in host derived from Kushview Element. It can be used standalone and, where enabled by the build, as an audio plug-in host. Audio and MIDI are routed through editable node graphs, making it suitable for effects racks, virtual-instrument setups, reusable processing graphs, and live-performance workflows.

This fork adds many features, including convenience-focused graph-editing workflows, per-node load monitoring, profile-aware multi-instance operation, and various customizations through direct editing of configuration files. Windows is currently used for testing, while changes aim to preserve cross-platform behavior wherever practical.

## Relationship to Upstream Element

Elementum began as a clone of [Kushview Element](https://github.com/kushview/element). Upstream Element remains the source of the core architecture, prior history, and much of the original functionality. Elementum-specific changes are maintained in this repository and may not be suitable for, accepted by, or supported by the upstream project.

Please use the appropriate project when reporting an issue:

| Topic | Where to report it |
|---|---|
| Behavior reproducible in unmodified upstream Element | [Kushview Element issue tracker](https://gitlab.com/kushview/element/-/issues) |
| Elementum-specific feature, regression, or documentation issue | [This repository's issue tracker](https://github.com/fooboody/elementum/issues) |

## Key Capabilities

Elementum retains the core Element workflow of connecting Audio and MIDI nodes into reusable graphs. Core capabilities include plug-in hosting, graph nesting, MIDI routing, virtual keyboard input, undo/redo, embedded plug-in editors, and scripting support. Actual available plug-in formats depend on the operating system, build configuration, installed plug-ins, and host context.

The fork also adds workflow-oriented functionality including the following.

| Area | Elementum additions |
|---|---|
| Graph editing | Nearby Audio/MIDI connection and disconnection, safe nearby Audio connection swapping, and Alt-click Enable/Disable toggling |
| MIDI workflow | Optional automatic wiring from both `MIDI IN DEVICE` and `MIDI IN` nodes to newly added MIDI-input nodes |
| Node control | Batch Enable/Disable for selected nodes and functional Audio input muting, including Audio sidechain inputs |
| Profiles and UI | Profile-specific concurrent instances with `--profile-dir`, robust start-hidden restoration, and a clickable data-directory status segment |
| Customization | Configuration-file control of Graph Editor colors, node-state colors, and menu appearance |

## Running Elementum

The standalone application is named `elementum` on Windows and Linux builds, and `Elementum` for the macOS application bundle. Its settings and profile data are stored in the standard application-data location unless a valid profile directory is selected.

### Profile-specific instances

Use `--profile-dir` to start Elementum with a separate data profile:

```text
elementum --profile-dir "C:\Elementum Profiles\Live"
```

A second launch using the same effective data profile is forwarded to the existing instance; different valid profiles may run concurrently. Other arguments, including session files, can be used with `--profile-dir`.

## Building from Source

Elementum uses CMake and requires CMake 3.26 or newer. See the platform-specific guide in [docs/building.md](docs/building.md).

A typical Release build on Windows, using Visual Studio generators, is:

```bash
cmake -S . -B build -A x64 -DELEMENT_BUILD_PLUGINS=ON -DELEMENT_ENABLE_ASIO=ON
cmake --build build --config Release
```

With this generator, the built standalone executable is normally located at:

```text
build\element_app_artefacts\Release\elementum.exe
```

The exact output directory can vary with the selected generator and build configuration. For Linux and macOS build instructions, see [docs/building.md](docs/building.md).

## Configuration and Customization

Most normal settings are available from the application UI. Several advanced Graph Editor colors and related options are intentionally configurable by directly editing the profile `.conf` file. Shut down the relevant Elementum instance before manually editing its configuration file.

## License and Attribution

Elementum is distributed under the **GNU General Public License v3.0 or later**. License texts for this repository and bundled materials are available in [LICENSES](LICENSES). Source files retain their original copyright notices and SPDX identifiers; Elementum changes do not remove or supersede upstream attribution.

The upstream Element authors and contributors are acknowledged in [AUTHORS.md](AUTHORS.md). Upstream documentation, source history, and trademarks remain the property of their respective owners.

## Project Status

This repository is an independently maintained customization fork. No claim is made that builds from this repository are official Kushview releases, compatible with every plug-in, or supported by Kushview, LLC.

For upstream Element information and official upstream downloads, visit [Kushview Element](https://kushview.net/element/).
