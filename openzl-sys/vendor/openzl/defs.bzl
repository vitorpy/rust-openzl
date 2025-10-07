# Copyright (c) Meta Platforms, Inc. and affiliates.

load("@fbcode//fbpkg:fbpkg.bzl", "fbpkg")
load("@fbcode_macros//build_defs:cpp_binary.bzl", "cpp_binary")
load("@fbcode_macros//build_defs:cpp_library.bzl", "cpp_library")
load("@fbcode_macros//build_defs:cpp_unittest.bzl", "cpp_unittest")
load("@fbsource//tools/build_defs:type_defs.bzl", "is_string")
load("@fbsource//xplat/security/lionhead:defs.bzl", "ALL_EMPLOYEES", "Interaction", "Metadata", "Priv", "Reachability", "Severity")
load("@fbsource//xplat/security/lionhead/build_defs:generic_harness.bzl", "generic_lionhead_harness")
load("//security/lionhead/harnesses:defs.bzl", "cpp_lionhead_harness")

ZS_HEADER_INCLUDE_PATH = "-Idata_compression/experimental/zstrong"

def _strip_prefix(headers, prefix):
    header_map = {}
    for header in headers:
        if not header.startswith(prefix):
            fail("Header {} does not start with {}".format(header, prefix))
        name = header[len(prefix):]
        header_map[name] = header
    return header_map

def public_headers(headers):
    return _strip_prefix(headers, "include/")

def private_headers(headers):
    return _strip_prefix(headers, "src/")

_ZS_COMPILER_FLAGS = [
    "-fno-sanitize=pointer-overflow",
]

_ZS_DEV_COMPILER_FLAGS = [
    "-Wall",
    "-Wcast-qual",
    "-Wcast-align",
    "-Wshadow",
    "-Wstrict-aliasing=1",
    "-Wstrict-prototypes",
    "-Wundef",
    "-Wpointer-arith",
    "-Wvla",
    "-Wformat=2",
    "-Wfloat-equal",
    "-Wredundant-decls",  # these two flags together guarantee that each
    "-Wmissing-prototypes",  # function is declared exactly once.
    "-Wswitch-enum",  # all enum vals must have cases in switches.
    "-pedantic",
    "-Wno-unused-function",  # because editor flags static inline functions
    "-Wimplicit-fallthrough",
    "-Wno-c2x-extensions",
    # "-DZS_ERROR_ENABLE_LEAKY_ALLOCATIONS=1", # set this to always create verbose errors
]

_ZS_DEV_C_COMPILER_FLAGS = [
    "-Wextra",
    "-Wconversion",
    "-Wno-missing-field-initializers",  # Allow missing fields in designated initializers
]

_ZS_SRC_FILE_COMPILER_FLAGS = {
    "src/openzl/common/errors.c": [
        "-Wno-format-nonliteral",
    ],
    "src/openzl/common/logging.c": [
        "-Wno-format-nonliteral",
    ],
    "src/zstrong/common/errors.c": [
        "-Wno-format-nonliteral",
    ],
    "src/zstrong/common/logging.c": [
        "-Wno-format-nonliteral",
    ],
}

_ZS_PROPAGATED_PP_FLAGS = [
    ZS_HEADER_INCLUDE_PATH,
]

_ZS_C_COMPILER_FLAGS = [
    "-std=c11",
]

_ZS_CXX_COMPILER_FLAGS = [
    "-Wno-c99-extensions",  # permit use of C99 features from C++ (i.e., tests)
    "-Wno-language-extension-token",
]

ZS_HARNESS_MODES = [
    "fbcode//security/lionhead/mode/dbgo-asan-libfuzzer",
    "fbcode//security/lionhead/mode/opt-asan-afl",
    "fbcode//security/lionhead/mode/opt-asan-libfuzzer",
    "fbcode//security/lionhead/mode/opt-ubsan-security-libfuzzer",
]

ZS_FUZZ_METADATA = Metadata(
    # Class of users that can access the target.
    exposure = ALL_EMPLOYEES,
    # Which project does this harness belong to
    project = "data_compression_experimental",
    # Security impact of target crashing or being killed.
    severity_denial_of_service = Severity.FILE_LOW_PRI_TASK,
    # Security impact of attacker breaking/bypassing target functionality.
    severity_service_takeover = Severity.FILE_LOW_PRI_TASK,
    # No auth is needed
    privilege_required = Priv.PRE_AUTH,
    # Not reachable through network
    reachability = Reachability.LOCAL,
    # no clicks neede by user
    user_interaction_required = Interaction.ZERO_CLICK,
)

def _is_release():
    return native.package_name().startswith("openzl/versions/release")

def _zs_src_file_compiler_flags(src):
    flags = []
    if not is_string(src):
        flags = src[1]
        src = src[0]

    if src.endswith(".c"):
        flags += _ZS_C_COMPILER_FLAGS
        if not _is_release():
            flags += _ZS_DEV_C_COMPILER_FLAGS
    elif src.endswith(".cpp"):
        flags += _ZS_CXX_COMPILER_FLAGS

    flags += _ZS_SRC_FILE_COMPILER_FLAGS.get(src, [])

    return (src, flags)

def _add_zs_compiler_flags(kwargs, strict_conversions = True, float_equal = True):
    # Add common compiler flags
    compiler_flags = list(_ZS_COMPILER_FLAGS)

    # Add dev or release compiler flags
    if not _is_release():
        compiler_flags += _ZS_DEV_COMPILER_FLAGS

    # Add the original compiler flags
    compiler_flags += kwargs.get("compiler_flags", [])

    # Remove strict conversions
    if not strict_conversions:
        compiler_flags = [x for x in compiler_flags if x != "-Wconversion"]

    # Remove float equal
    if not float_equal:
        compiler_flags = [x for x in compiler_flags if x != "-Wfloat-equal"]

    kwargs["compiler_flags"] = compiler_flags

    # add file-specific compiler flags
    kwargs["srcs"] = [
        _zs_src_file_compiler_flags(src)
        for src in kwargs.get("srcs", [])
    ]

def zs_library(**kwargs):
    _add_zs_compiler_flags(kwargs)
    _zs_library(**kwargs)

def zs_cxxlibrary(strict_conversions = True, float_equal = True, **kwargs):
    _add_zs_compiler_flags(kwargs, strict_conversions = strict_conversions, float_equal = float_equal)
    _zs_library(**kwargs)

def _zs_library(**kwargs):
    propagated_pp_flags = kwargs.get("propagated_pp_flags", [])
    kwargs["propagated_pp_flags"] = _ZS_PROPAGATED_PP_FLAGS + propagated_pp_flags

    cpp_library(
        **kwargs
    )

def zs_binary(**kwargs):
    _add_zs_compiler_flags(kwargs)
    _zs_binary(**kwargs)

def zs_release_binary(**kwargs):
    if _is_release():
        _add_zs_compiler_flags(kwargs)
        _zs_binary(**kwargs)

def zs_cxxbinary(strict_conversions = True, float_equal = True, **kwargs):
    _add_zs_compiler_flags(kwargs, strict_conversions = strict_conversions, float_equal = float_equal)
    _zs_binary(**kwargs)

def _zs_binary(**kwargs):
    cpp_binary(**kwargs)

def zs_unittest(**kwargs):
    _add_zs_compiler_flags(kwargs)
    cpp_unittest(**kwargs)

def dev_fbpkg_builder(**kwargs):
    if not _is_release():
        fbpkg.builder(**kwargs)

def release_fbpkg_builder(**kwargs):
    if _is_release():
        fbpkg.builder(**kwargs)

def zs_fuzzers(ftest_names, generator = None, **kwargs):
    """
    Generates fuzzers for each test case in :ftest_names:.

    Args:
        srcs: The source files with the fuzzers.

        ftest_names: A list of tuples of (test_suite, test_case) to fuzz.
        Each should have a matching FUZZ(test_suite, test_case) in the source file.

        generator: Optionally a binary target that accepts three parameters:
        test_suite, test_case, and output_directory. It should generate an appropiate
        seed corpus for the given ftest in the output_directory. This is used to seed
        the fuzzer during corpus expansion. This allows us to e.g. dynamically generate
        relevant Zstrong compressed frame with interesting transforms for decompression
        fuzzers.

        deps: The fuzzer's dependencies.
    """
    if _is_release():
        # Give release fuzzers a different name
        prefix = "Release_Zstrong_"
    else:
        prefix = "Zstrong_"

    for ftest_name in ftest_names:
        name = prefix + "_".join(ftest_name)
        cpp_lionhead_harness(
            name = name,
            metadata = ZS_FUZZ_METADATA,
            ftest_name = ftest_name,
            harness_configs = {mode: {} for mode in ZS_HARNESS_MODES},
            **kwargs
        )

        if generator:
            generator_name = name + "_Generator"
            generator_config = {
                "seed_generator_command": [
                    "./generator",
                    ftest_name[0],
                    ftest_name[1],
                    "@out_seed_folder@",
                ],
            }
            generic_lionhead_harness(
                name = generator_name,
                bundle_spec_version = 1,
                environment_constraints = {
                    "remote_execution.linux": {},
                    "tw.lionhead": {},
                },
                harness_configs = {mode: generator_config for mode in ZS_HARNESS_MODES},
                harness_default_modes = {
                    "coverage": "fbcode//security/lionhead/mode/opt-cov.v2",
                    "expand": "fbcode//security/lionhead/mode/opt-asan-libfuzzer",
                    "reproduce": "fbcode//security/lionhead/mode/opt-asan-libfuzzer",
                },
                mapped_srcs = {
                    "fuzz": "fbsource//xplat/security/lionhead/utils/runners/libfuzzer:fuzz",
                    "fuzz_utils.py": "fbsource//xplat/security/lionhead/utils/runners:fuzz_utils",
                    "generator": generator,
                    generator_name: ":" + name + "_bin",
                },
                metadata = ZS_FUZZ_METADATA,
            )

def zs_raw_fuzzer(name, **kwargs):
    if _is_release():
        # Give release fuzzers a different name
        name = "Release_" + name
    cpp_lionhead_harness(
        name = name,
        metadata = ZS_FUZZ_METADATA,
        harness_configs = {mode: {} for mode in ZS_HARNESS_MODES},
        **kwargs
    )
