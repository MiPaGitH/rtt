SUMMARY = "program that provides real time tasks"
DESCRIPTION = "program that runs code in a real time task"
SECTION = "rtt"
DEPENDS = ""
LICENSE = "GPL-3.0-only"
#LIC_FILES_CHKSUM = "file:///home/mp/p/rtt/COPYING;md5=89167747856237fe0668952b93368b72"
LIC_FILES_CHKSUM = "file://COPYING;md5=89167747856237fe0668952b93368b72"

#FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}-${PV}:"

SRC_URI = "file:///home/mp/p/rtt-1.0.tar.gz"
inherit autotools

