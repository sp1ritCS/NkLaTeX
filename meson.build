project('NkLaTeX', 'c',
	version : '0.1',
	default_options : ['warning_level=3', 'c_std=gnu2x'])

app_id = 'arpa.sp1rit.NoteKit.NkLaTeX'
nklatex_prefix = get_option('prefix')
nklatex_bindir = join_paths(nklatex_prefix, get_option('bindir'))
nklatex_datadir = join_paths(nklatex_prefix, get_option('datadir'))
nklatex_pkgdatadir = join_paths(nklatex_datadir, meson.project_name())

gnome = import('gnome')

nk_ext = gnome.gdbus_codegen('notekit_external', 'com.github.blackhole89.NoteKit.External.xml',
  interface_prefix: 'com.github.blackhole89.NoteKit.',
  namespace: 'NoteKit',
  object_manager: true
)

nkl_res = gnome.compile_resources('nkl_res', 'nklatex.gresource.xml',
  c_name: 'nkl_resource'
)

conf_data = configuration_data()
conf_data.set_quoted('application_id', app_id)
conf_data.set_quoted('version', meson.project_version())
conf_data.set10('debug', get_option('buildtype') == 'debug')
conf_data.set_quoted('latex2svg_loc', get_option('prefix') / get_option('libexecdir') / 'nklatex-latex2svg')
configure_file(input : 'config.h.in',
	output : 'config.h',
	configuration : conf_data
)

service_conf_data = configuration_data()
service_conf_data.set('nklatex', nklatex_bindir / 'nklatex')
service = configure_file(input: '@0@.service.in'.format(app_id),
  output: '@0@.service'.format(app_id),
  configuration: service_conf_data
)

install_data('latex2svg', rename: 'nklatex-latex2svg', install_dir: get_option('libexecdir'))
install_data('@0@.desktop'.format(app_id), install_dir: get_option('datadir') / 'applications')
install_data('@0@.svg'.format(app_id), install_dir: get_option('datadir') / 'icons'  / 'hicolor'  / 'scalable' / 'apps')
install_data('@0@.gschema.xml'.format(app_id), install_dir: get_option('datadir') / 'glib-2.0' / 'schemas')
install_data(service, install_dir: get_option('datadir') / 'dbus-1' / 'services')
gnome.post_install(
  glib_compile_schemas: true,
  update_desktop_database: true
)

app = executable('nklatex', [
		'nklatex.c',
		nk_ext,
        nkl_res
	],
	dependencies: [
		dependency('gtk4'),
		dependency('libadwaita-1'),
		dependency('gtksourceview-5'),
        dependency('librsvg-2.0'),
		dependency('libzstd'),
        dependency('zlib')
	],
	install : true
)

test('basic', app)

devenv = environment()
devenv.set('NK_LATEX_LATEX2SVG_LOCATION', meson.current_source_dir() / 'latex2svg')
gnome.compile_schemas(build_by_default: true, depend_files: '@0@.gschema.xml'.format(app_id))
devenv.set('GSETTINGS_SCHEMA_DIR', meson.current_build_dir())
meson.add_devenv(devenv)
