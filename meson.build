project('marina', ['c', 'cpp'], version: '0.1.0', meson_version: '>=1.0.0', default_options: ['cpp_std=c++23', 'buildtype=release'])
qt6 = import('qt6')
qt = dependency('qt6', version: '>=6.10', modules: ['Core', 'DBus', 'Gui', 'Network', 'Qml', 'Quick', 'QuickControls2', 'WaylandClient'])
kf6_core = dependency('KF6CoreAddons', version: '>=6.13')
mauikit = dependency('MauiKit4', version: '>=4.0.5', required: true)
cpp_compiler = meson.get_compiler('cpp')
if cpp_compiler.has_argument('-Wno-sfinae-incomplete')
    add_project_arguments('-Wno-sfinae-incomplete', language: 'cpp')
endif
kf6_i18n = declare_dependency(
    include_directories: include_directories(get_option('prefix') / 'include' / 'KF6' / 'KI18n'),
    dependencies: [cpp_compiler.find_library('KF6I18n'), cpp_compiler.find_library('KF6I18nQml')],
)
wayland = dependency('wayland-client', required: true)
layer_shell = cpp_compiler.find_library('LayerShellQtInterface', required: true)
cpp_args = []
if host_machine.cpu_family() == 'x86_64'
    cpp_args += ['-march=x86-64-v3']
endif
qtwaylandscanner = find_program('qtwaylandscanner', dirs: ['/usr/lib/qt6/libexec', '/usr/lib/qt6/bin'])
wayland_scanner = find_program('wayland-scanner')
wayland_protocol = 'src/protocols/hyprland-global-shortcuts-v1.xml'
wayland_client_header = custom_target(
    'hyprland-global-shortcuts-client-header',
    input: wayland_protocol,
    output: 'wayland-hyprland-global-shortcuts-v1-client-protocol.h',
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
)
wayland_private_code = custom_target(
    'hyprland-global-shortcuts-private-code',
    input: wayland_protocol,
    output: 'wayland-hyprland-global-shortcuts-v1-protocol.c',
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
)
protocol_header = custom_target(
    'hyprland-global-shortcuts-header',
    input: wayland_protocol,
    output: 'qwayland-hyprland-global-shortcuts-v1.h',
    command: [qtwaylandscanner, 'client-header', '@INPUT@'],
    capture: true,
    depends: [wayland_client_header],
)
protocol_code = custom_target(
    'hyprland-global-shortcuts-code',
    input: wayland_protocol,
    output: 'qwayland-hyprland-global-shortcuts-v1.cpp',
    command: [qtwaylandscanner, 'client-code', '@INPUT@'],
    capture: true,
    depends: [wayland_client_header],
)
qt_sources = qt6.preprocess(qresources: 'src/qml.qrc', moc_headers: ['src/controllers/dockmodel.h', 'src/controllers/globalshortcuts.h'], moc_sources: ['src/main.cpp', 'src/controllers/globalshortcuts.cpp'])
executable('marina', ['src/main.cpp', 'src/controllers/dockmodel.cpp', 'src/controllers/globalshortcuts.cpp', protocol_header, protocol_code, wayland_private_code] + qt_sources, dependencies: [qt, kf6_core, kf6_i18n, mauikit, wayland, layer_shell], cpp_args: cpp_args, install: true)
install_data('org.maui.marina.desktop.in', install_dir: get_option('datadir') / 'applications', rename: 'org.maui.marina.desktop')
