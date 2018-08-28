DESCRIPTION = "RPC Broker"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0;md5=801f80980d171dd6425610833a22dbe6"
DEPENDS = "json-c libwebsockets libv4v dbus libxml2"

PV = "0+git${SRCPV}"

SRCREV = "${AUTOREV}"
SRC_URI = "git://github.com/tijko/rpc-broker;protocol=https;branch=master \
           file://rpc-broker.initscript                                   \
          "

S = "${WORKDIR}/git/"

INITSCRIPT_NAME = "rpc-broker"

EXTRA_OECONF += ""
EXTRA_OEMAKE += ""

inherit autotools pkgconfig update-rc.d

do_install_append() {
    install -m 0755 -d ${D}/etc
    install -m 0755 -d ${D}/etc/init.d
    install -m 0755 ${WORKDIR}/rpc-broker.initscript ${D}${sysconfdir}/init.d/rpc-broker
}
