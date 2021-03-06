APPNAME = 'elasto'
VERSION = '0.7.3'
LIBELASTO_API_VERS = '0.1.0'

top = '.'
out = 'build'
recurse_subdirs = 'ccan lib client test doc libworkqueue tcmu'

def options(opt):
	opt.load('compiler_c')
	opt.load('gnu_dirs')
	opt.recurse(recurse_subdirs)

def configure(conf):
	conf.load('compiler_c')
	conf.load('gnu_dirs')
	conf.env.CFLAGS = ['-Wall', '-D_LARGEFILE64_SOURCE', '-D_GNU_SOURCE']
	# append flags from CFLAGS environment var
	conf.cc_add_flags()
	conf.env.LIBELASTO_API_VERS = LIBELASTO_API_VERS
	conf.define('LIBELASTO_API_VERS', LIBELASTO_API_VERS)
	conf.define('ELASTO_VERS', VERSION)
	conf.check(lib='event')
	# coarse check for libevent >= 2.1.x, whick doesn't have a pkgconfig.
	# a check for bufferevent_openssl_socket_new() would be better.
	conf.check(header_name='event2/visibility.h')
	conf.check(lib='crypto')
	conf.check(lib='expat')
	conf.recurse(recurse_subdirs)
	conf.write_config_header('config.h')

def build(bld):
	bld.recurse(recurse_subdirs)
