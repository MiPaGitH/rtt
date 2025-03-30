the rtt-1.0 folder contains the program code plus the files needed for autotools
the meta-rtt directory contains the yocto recipe to build the rtt module
to use this recipe just add in ....poky/build/conf/local.conf the follwing line:
  IMAGE_INSTALL:append = " rtt"


