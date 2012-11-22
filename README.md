android-sdcard-writer
=====================

Utility to write images to SD cards under Android. Can also patch Allwinner A10 images so that they contain the hardware specific information of the device (script.bin + SPL DRAMC settings)

Java code is in Berryboot directory.
Utility to patch SPL in a10-patchspl.c.
It assumes that the SPL file to patch was build with 'make cubieboard' (currently searches the spl binary for a struct with the cubie settings to find the location to patch, pending a cleaner solution)

Binary build available at: http://get.berryboot.com/
 
