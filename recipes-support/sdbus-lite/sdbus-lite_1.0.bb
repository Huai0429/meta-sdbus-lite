SUMMARY = "Lightweight C wrapper library for systemd sd-bus"
DESCRIPTION = "A streamlined C library that wraps libsystemd sd-bus to simplify D-Bus IPC. \
It provides easy-to-use APIs for initialization, method invocation, signal handling, \
and property management, enabling unified and reusable IPC across multiple applications."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "\
    file://sdbus_lite.pc.in;subdir=src \
    file://sdbus_lite.c;subdir=src \
    file://sdbus_lite.h;subdir=src \
    file://Makefile;subdir=src \
    file://sdbus_lite.pc.in;subdir=src \
"

S = "${WORKDIR}/src"

DEPENDS += "systemd"

inherit pkgconfig

do_compile() {
    oe_runmake
}

do_install() {
    install -d ${D}${libdir}
    install -m 0755 ${S}/libsdbus_lite.so.1.0.0 ${D}${libdir}/libsdbus_lite.so.1.0.0
    ln -sf libsdbus_lite.so.1.0.0 ${D}${libdir}/libsdbus_lite.so.1
    ln -sf libsdbus_lite.so.1 ${D}${libdir}/libsdbus_lite.so
    install -m 0644 ${S}/libsdbus_lite.a ${D}${libdir}/libsdbus_lite.a

    install -d ${D}${includedir}/sdbus_lite
    install -m 0644 ${S}/sdbus_lite.h ${D}${includedir}/sdbus_lite/sdbus_lite.h

    install -d ${D}${libdir}/pkgconfig
    sed -e 's#@PREFIX@#${prefix}#g' \
        -e 's#@LIBDIR@#${libdir}#g' \
        -e 's#@INCLUDEDIR@#${includedir}#g' \
        ${S}/sdbus_lite.pc.in > ${D}${libdir}/pkgconfig/sdbus_lite.pc
}

FILES:${PN} = "${libdir}/libsdbus_lite.so.*"
FILES:${PN}-dev += "${includedir}/sdbus_lite/sdbus_lite.h ${libdir}/libsdbus_lite.so ${libdir}/pkgconfig/sdbus_lite.pc"
FILES:${PN}-staticdev += "${libdir}/libsdbus_lite.a"
