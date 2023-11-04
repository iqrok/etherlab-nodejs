{
	"targets": [{
		"target_name": "ecat",
		"include_dirs": [
			"<!@(node -p \"require('node-addon-api').include_dir\")",
			"<!@(find 'src/etherlab-helper' -name 'include' -type d)",
			"/usr/local/include"
		],
		"sources": [
			"src/api.cc",
			"<!@(find 'src/etherlab-helper/src/' -name '*.cpp')",
		],
		"library_dirs": [
			"/usr/local/lib/",
			"/usr/lib/"
		],
		"libraries": [
			"-lethercat",
			"-lpthread"
		],
		"cflags": [
			"-std=c++20",
			"-fexceptions",
			"-lstdc++",
			"-O2",
			"-Wfatal-errors",
			"-Wall",
			"-Wpedantic",
			"-Wno-missing-field-initializers"
		],
		"cflags_cc": [
			"-std=c++20",
			"-fexceptions",
			"-lstdc++",
			"-O2",
			"-Wfatal-errors",
			"-Wall",
			"-Wpedantic",
			"-Wno-missing-field-initializers"
		],
		"variables": {
			"build_debug": '<!(printf %d ${ECAT_BUILD_DEBUG} || printf %d 0)',
		},
		'defines': [ "NO_LOCK=1" ],
		'conditions': [
			['build_debug > 0', {
				"defines": [ "VERBOSE=<(build_debug)" ],
				"cflags": [ "-g3" ],
				"cflags_cc": [ "-g3" ],
			}]
		]
	}]
}
