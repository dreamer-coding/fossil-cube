dir = include_directories('.')
cc = meson.get_compiler('c')

# Platform-specific dependencies
if host_machine.system() == 'windows'
    winsock_dep = cc.find_library('ws2_32', required: true)
    opengl_dep = cc.find_library('opengl32', required: true)
elif host_machine.system() == 'darwin'
    # On macOS, OpenGL is provided as a framework
    opengl_dep = dependency('appleframeworks', modules: ['OpenGL'])
    winsock_dep = []
else
    # Assume Linux or BSD: use GLX for OpenGL support
    opengl_dep = dependency('gl', required: true)
    winsock_dep = []
endif

fossil_cube_lib = static_library(
    'fossil-cube',
    files('cube.c'),
    install: true,
    dependencies: [
        cc.find_library('m', required: false),
        winsock_dep,
        opengl_dep
    ],
    include_directories: dir
)

fossil_cube_dep = declare_dependency(
    link_with: [fossil_cube_lib],
    include_directories: dir
)