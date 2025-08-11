# **Fossil CUBE by Fossil Logic**

**Fossil CUBE** is a lightweight, portable graphical user interface (GUI) library written in pure C with zero external dependencies. Designed to provide a clean, minimal abstraction layer for creating cross-platform desktop and embedded graphical applications, Fossil CUBE focuses on core GUI functionality—windows, controls, input handling, and event management—without directly depending on OpenGL or other graphics APIs.

Its modular design, small footprint, and audit-friendly codebase make it ideal for developers who need a reliable, easily integrated GUI foundation on Windows, macOS, Linux, and embedded platforms.

### Key Features

- **Cross-Platform GUI Framework**  
  Provides unified windowing, input, and widget abstractions on Windows, macOS, and Linux.

- **Zero External Dependencies**  
  Written entirely in portable C for maximum auditability and ease of integration.

- **Lightweight and Efficient**  
  Minimal memory usage and fast execution suitable for embedded and resource-constrained environments.

- **Modular Design**  
  Easily extended with custom controls and platform-specific backends.

- **Input & Event Handling**  
  Supports keyboard, mouse, and touch inputs with flexible event dispatch.

- **Flexible Rendering Backend**  
  Allows integration with OpenGL, Vulkan, software rendering, or other graphics engines externally.

## Getting Started

### Prerequisites

- **Meson Build System**  
  Fossil CUBE uses Meson for build configuration. If you don’t have Meson installed, please follow the installation instructions on the official [Meson website](https://mesonbuild.com/Getting-meson.html).

### Adding Fossil CUBE as a Dependency

#### Using Meson

### **Install or Upgrade Meson** (version 1.3 or newer recommended):

```sh
   python -m pip install meson           # Install Meson
   python -m pip install --upgrade meson # Upgrade Meson
```
###	Add the .wrap File
Place a file named fossil-cube.wrap in your subprojects directory with the following content:

```ini
# ======================
# Git Wrap package definition
# ======================
[wrap-git]
url = https://github.com/fossillogic/fossil-cube.git
revision = v0.1.0

[provide]
fossil-cube = fossil_cube_dep
```

###	Integrate in Your meson.build
Add the dependency by including this line:

```meson
cube_dep = dependency('fossil-cube')
```


## Build Configuration Options

Customize your build with the following Meson options:
	•	Enable Tests
To run the built-in test suite, configure Meson with:

```sh
meson setup builddir -Dwith_test=enabled
```

## Contributing and Support

For those interested in contributing, reporting issues, or seeking support, please open an issue on the project repository or visit the [Fossil Logic Docs](https://fossillogic.com/docs) for more information. Your feedback and contributions are always welcome.