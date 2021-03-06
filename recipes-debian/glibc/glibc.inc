#
# base recipe: meta/recipes-core/eglibc/eglibc.inc
# base branch: daisy
#

inherit debian-package
PV = "2.19"
DPN = "glibc"

LICENSE = "GPLv2 & LGPLv2.1 & ISC"
LIC_FILES_CHKSUM = " \
    file://COPYING;md5=b234ee4d69f5fce4486a80fdaf4a4263 \
    file://COPYING.LIB;md5=4fbd65380cdd255951079008b364516c \
    file://LICENSES;md5=e9a558e243b36d3209f380deb394b213 \
"

# Exclude patch that apply for old version:
# mips-rld-map-check.patch
# initgroups_keys.patch
# eglibc_fix_findidx_parameters.patch
# fileops-without-wchar-io.patch
# fix-tibetian-locales.patch
# CVE-2014-5119.patch
#
# Exclude GLRO_dl_debug_mask.patch because debian source code (2.19-10)
# does not support RTLD debug
#
# Exclude 0001-eglibc-menuconfig-support.patch
#         0002-eglibc-menuconfig-hex-string-options_debian.patch
#         0003-eglibc-menuconfig-build-instructions.patch
# because Debian distro does not support kconfig-frontend
# correct-valencia-locale-supported_debian.patch:
# 	This patch is fix "QA Issue: locale-base-ca-es is listed in PACKAGES multiple times"
SRC_URI += " \
    file://eglibc-svn-arm-lowlevellock-include-tls.patch \
    file://IO-acquire-lock-fix.patch \
    file://generate-supported.mk \
    file://glibc.fix_sqrt2.patch \
    file://multilib_readlib.patch \
    file://ppc-sqrt_finite.patch \
    file://ppc_slow_ieee754_sqrt.patch \
    file://add_resource_h_to_wait_h.patch \
    file://0001-R_ARM_TLS_DTPOFF32.patch \
    file://0001-eglibc-run-libm-err-tab.pl-with-specific-dirs-in-S.patch \
    file://ppce6500-32b_slow_ieee754_sqrt.patch \
    file://grok_gold.patch \
    file://0013-sysdeps-gnu-configure.ac-handle-correctly-libc_cv_ro.patch \
    file://correct-valencia-locale-supported_debian.patch \
"

B = "${WORKDIR}/build-${TARGET_SYS}"

FILESPATH = "${@base_set_filespath([ '${FILE_DIRNAME}/glibc-${PV}', '${FILE_DIRNAME}/glibc', '${FILE_DIRNAME}/files', '${FILE_DIRNAME}' ], d)}"

require glibc-ld.inc
require glibc-testing.inc

STAGINGCC = "gcc-cross-initial-${TARGET_ARCH}"
STAGINGCC_class-nativesdk = "gcc-crosssdk-initial-${SDK_SYS}"
PATH_prepend = "${STAGING_BINDIR_TOOLCHAIN}.${STAGINGCC}:"

TOOLCHAIN_OPTIONS = " --sysroot=${STAGING_DIR_TCBOOTSTRAP}"

# glibc can't be built without optimization, if someone tries to compile an
# entire image as -O0, we override it with -O2 here and give a note about it.
def get_optimization(d):
    selected_optimization = d.getVar("SELECTED_OPTIMIZATION", True)
    if bb.utils.contains("SELECTED_OPTIMIZATION", "-O0", "x", "", d) == "x":
        bb.note("glibc can't be built with -O0, -O2 will be used instead.")
        return selected_optimization.replace("-O0", "-O2")
    return selected_optimization

SELECTED_OPTIMIZATION := "${@get_optimization(d)}"

# siteconfig.bbclass runs configure which needs a working compiler
# For the compiler to work we need a working libc yet libc isn't 
# in the sysroots directory at this point. This means the libc.so
# linker script won't work as the --sysroot setting isn't correct.
# Here we create a hacked up libc linker script and pass in the right
# flags to let configure work. Ugly.
EXTRASITECONFIG = "CFLAGS='${CFLAGS} -Wl,-L${WORKDIR}/site_config_libc -L${WORKDIR}/site_config_libc -L${SYSROOT_DESTDIR}${libdir} -L${SYSROOT_DESTDIR}${base_libdir} -Wl,-L${SYSROOT_DESTDIR}${libdir} -Wl,-L${SYSROOT_DESTDIR}${base_libdir}'"
siteconfig_do_siteconfig_gencache_prepend = " \
	mkdir -p ${WORKDIR}/site_config_libc; \
	cp ${SYSROOT_DESTDIR}${libdir}/libc.so ${WORKDIR}/site_config_libc; \
	sed -i -e 's# ${base_libdir}# ${SYSROOT_DESTDIR}${base_libdir}#g' -e 's# ${libdir}# ${SYSROOT_DESTDIR}${libdir}#g' ${WORKDIR}/site_config_libc/libc.so; \
"

# nptl needs unwind support in gcc, which can't be built without glibc.
DEPENDS = "virtual/${TARGET_PREFIX}gcc-initial libgcc-initial linux-libc-headers virtual/${TARGET_PREFIX}libc-initial"
# nptl needs libgcc but dlopens it, so our shlibs code doesn't detect this
#RDEPENDS_${PN} += "${@['','libgcc']['nptl' in '${GLIBC_ADDONS}']}"
PROVIDES = "virtual/libc virtual/${TARGET_PREFIX}libc-for-gcc"
PROVIDES += "virtual/libintl virtual/libiconv"
inherit autotools
require glibc-options.inc

LEAD_SONAME = "libc.so"

CACHED_CONFIGUREVARS += " \
	ac_cv_path_BASH_SHELL=${base_bindir}/bash \
	libc_cv_slibdir=${base_libdir} \
	libc_cv_rootsbindir=${base_sbindir} \
	libc_cv_localedir=${localedir} \
	libc_cv_ssp=no \
"

GLIBC_EXTRA_OECONF ?= ""
GLIBC_EXTRA_OECONF_class-nativesdk = ""
INHIBIT_DEFAULT_DEPS = "1"

ARM_INSTRUCTION_SET = "arm"

# glibc uses PARALLELMFLAGS variable to pass parallel build info so transfer
# PARALLEL_MAKE into PARALLELMFLAGS and empty out PARALLEL_MAKE
EGLIBCPARALLELISM := "PARALLELMFLAGS="${PARALLEL_MAKE}""
EXTRA_OEMAKE[vardepsexclude] += "EGLIBCPARALLELISM"
EXTRA_OEMAKE += "${EGLIBCPARALLELISM}"
PARALLEL_MAKE = ""

# glibc make-syscalls.sh has a number of issues with /bin/dash and
# it's output which make calls via the SHELL also has issues, so 
# ensure make uses /bin/bash
EXTRA_OEMAKE += "SHELL=/bin/bash"

OE_FEATURES = "${@features_to_glibc_settings(d)}"
do_configure_prepend() {
	sed -e "s#@BASH@#/bin/sh#" -i ${S}/elf/ldd.bash.in
	echo '${OE_FEATURES}' > ${B}/option-groups.config
}

do_configure_append() {
	oe_runmake config

	# Remove quotation marks from OPTION_EGLIBC_NSSWITCH_FIXED_*. This will
	# avoid install error.
	sed -i 's/^OPTION_EGLIBC_NSSWITCH_FIXED_\(.*\)="\(.*\)"$/OPTION_EGLIBC_NSSWITCH_FIXED_\1=\2/' option-groups.config
}

GLIBC_ADDONS ?= "ports,nptl,libidn"

do_debian_patch_append() {
	# Set empty because kconfig-frontend is not supported
	echo "config:" >> ${S}/Makeconfig

	# No documentation
	sed -i -e "s:manual::" ${S}/Makeconfig
}

PACKAGES_DYNAMIC = ""

# the -isystem in bitbake.conf screws up glibc do_stage
BUILD_CPPFLAGS = "-I${STAGING_INCDIR_NATIVE}"
TARGET_CPPFLAGS = "-I${STAGING_DIR_TARGET}${includedir}"

#
# For now, we will skip building of a gcc package if it is a uclibc one
# and our build is not a uclibc one, and we skip a glibc one if our build
# is a uclibc build.
#
# See the note in gcc/gcc_3.4.0.oe
#

python __anonymous () {
    import re
    uc_os = (re.match('.*uclibc$', d.getVar('TARGET_OS', True)) != None)
    if uc_os:
        raise bb.parse.SkipPackage("incompatible with target %s" %
                                   d.getVar('TARGET_OS', True))
}

do_patch_append() {
    bb.build.exec_func('do_fix_readlib_c', d)
}

# for mips glibc now builds syscall tables for all abi's
# so we make sure that we choose right march option which is
# compatible with o32,n32 and n64 abi's
# e.g. -march=mips32 is not compatible with n32 and n64 therefore
# we filter it out in such case -march=from-abi which will be
# mips1 when using o32 and mips3 when using n32/n64

TUNE_CCARGS_mips := "${@oe_filter_out('-march=mips32', '${TUNE_CCARGS}', d)}"
TUNE_CCARGS_mipsel := "${@oe_filter_out('-march=mips32', '${TUNE_CCARGS}', d)}"

do_fix_readlib_c () {
	sed -i -e 's#OECORE_KNOWN_INTERPRETER_NAMES#${EGLIBC_KNOWN_INTERPRETER_NAMES}#' ${S}/elf/readlib.c
}

BBCLASSEXTEND = "nativesdk"
