# **Fossil CUBE by Fossil Logic**

**Fossil CUBE** is a lightweight, portable graphical user interface (GUI) library written in pure C. Designed for maximum portability and minimal dependencies, Fossil CUBE relies solely on OpenGL for rendering and abstracts away OS-level graphics details. This makes it ideal for cross-platform applications requiring efficient, hardware-accelerated 2D and 3D graphics without bulky frameworks or heavy dependencies.

### Key Features

- **Minimal Dependencies**  
  Uses only OpenGL for all rendering tasks—no external GUI toolkits or libraries required.

- **Cross-Platform Support**  
  Compatible with Windows, macOS, Linux, and embedded platforms with OpenGL support.

- **Pure C Implementation**  
  Written entirely in clean, portable C for easy integration, auditability, and customization.

- **Hardware-Accelerated Graphics**  
  Leverages OpenGL to deliver smooth, efficient rendering on modern GPUs.

- **Modular and Extensible**  
  Flexible design allows you to extend and customize widgets, rendering pipelines, and input handling.

- **Simple API**  
  Designed with ease of use in mind for rapid GUI development.

## Getting Started

### Prerequisites

- **OpenGL Development Environment**  
  You need an OpenGL-compatible environment and drivers installed on your system.

- **Meson Build System**  
  Fossil CUBE uses Meson for build configuration. If you don’t have Meson installed, see the official [Meson website](https://mesonbuild.com/Getting-meson.html).

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