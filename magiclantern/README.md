
## Installation instructions

For the experimental 5D4 firmware 1.3.3 model, see the
[5D4 build and tool guide](5D4_133.md) and [local changelog](../Changelog).

These instructions have been tested on Linux.  

Qemu is somewhat unusual in that it expects to be built from outside of the source dir.  This is clean, keeping the binaries separate from the code, with no risk of checking them in accidentally.

Clone both the ML and qemu-eos repos, from within the same directory:

```
git clone https://github.com/reticulatedpines/magiclantern_simplified
git clone https://github.com/reticulatedpines/qemu-eos
cd qemu-eos; git switch qemu-eos-v4.2.1; cd ..
```
Create a new directory, qemu-eos-build, also at the same level, and build qemu-eos from inside it:

```
mkdir qemu-eos-build
cd qemu-eos-build
../qemu-eos/configure --target-list=arm-softmmu --enable-plugins --disable-docs --enable-vnc --enable-gtk --disable-vte
make -j16
make plugins
cp tests/plugin/libmagiclantern.so arm-softmmu/plugins/
```
You can now run qemu, the main binary is `arm-softmmu/qemu-system-arm` - but you don't need to run this directly, and may want to have easy access to GDB scripts, depending on which model cam you're interested in, so:

For a flat EOS 5D Mark IV dump containing `ROM1.BIN` and `SFDATA.BIN`,
`magiclantern/run_5d4_133.sh /path/to/dump/ROM1.BIN` stages the expected `5D4/`
layout without copying the dumps and runs a 30-second boot smoke test.
Disk writes go to disposable snapshots; the supplied card images are not
updated by this launcher. Set `QEMU_EOS_BINARY` to use an external build.
Pass `--gui` as the last argument to request a GTK window that stays open until
you close QEMU; without it the probe is headless and runs for 30 seconds.
It uses the current 5D4 model and MPU spells and writes the trace to
`/tmp/5d4-qemu.log` by default. Earlier interactive runs reached the
Date/Time/Zone dialog at 960x540; boot is not reliable across input states.
See the guide for the latest validation limits. Initial Zico emulation supports
palette upload (C0), rectangle fills (DF), monochrome masks (E4), and ASCII
text (40). Text uses the CUD Gothic TrueType font directly from guest ROM
through FreeType; install the FreeType development package before building.
The display reads the MARV ARGB8888 surface at `0x03911D00`.

This is partial, firmware-specific graphics emulation: font size is currently
32 pixels, icons and other graphics commands are incomplete, and interactive
controls are partially supported. Page Up/Page Down navigate with the rear dial;
Space sends SET. These keys were verified opening Zone/Time difference and
changing Paris to Azores without losing the dialog. No numpad is required.
F1 prints keyboard help in the launching terminal. Joystick mappings remain
provisional. It does not emulate the Zico processor.

The 5D4 requires LCD interrupt 0x86 to release Winsys_Wait_v after a redraw.
The model supplies recurring refresh events after scanout starts and latches
the GUI layer on Ximr composition (command 24), before ARM reuses the working
surface. This prevents stalled controls and disappearing labels during edits.
Use `-d io,verbose` for bounded packet and payload traces at the Zico doorbell.
The current packet header is at `0xBFF00500`; producer and consumer indices
are at `0xBFF00410` and `0xBFF00428`.

```
cd ../qemu-eos/magiclantern
./run_qemu.py --help
```

If you make any changes to source in qemu-eos, from qemu-eos-build:

```
make -j16 && make plugins && cp tests/plugin/libmagiclantern.so arm-softmmu/plugins/
```


## Inventory

build_qemu_direct.py, build_qemu_docker.py:  
Either can be used to create a zip containing a standalone Qemu installation.  This is no use for dev work, but does allow running emulated cams.  It is intended for curious end-users, and for running the testing suite against a known clean build.

run_qemu.py:  
Runs Qemu with a valid set of parameters to emulate a cam.  Handles various error conditions, cleaning up temp files, etc.

run_tests.py:  
Runs the Qemu based regression test suite.  Currently, this is far from complete, only a proof-of-concept to replace the old, unworkable testing system.

### 5D4 vector icons

Zico command 7B submits a vector-object descriptor. Its ROM resource begins
with tag 9999; AAA0 contains OpenVG-style segment bytes and AAA1 signed
coordinates in sixteenths of a pixel. The current decoder handles move,
line, horizontal/vertical line, quadratic/cubic curves and close, with
an even-odd fill. A scan of 1,670 resources in the supplied ROM found only
move, line, horizontal/vertical line, quadratic curves and close.
Cairo development headers/libraries are required to build the renderer.
`tools/decode_canon_vector.py ROM1.BIN ADDRESS OUTPUT.svg` inspects a resource.

Other model support was checked in the local QEMU sources: the shared older
camera path displays 4-bit/8-bit palette overlays (and YUV); it has no tagged
vector-resource decoder to reuse. The EOS M10 startup also bypasses MZRM
waits, but does not provide graphics command rendering. The new resource
parser has no camera-specific ROM addresses; its 7B packet/descriptor adapter
is currently enabled only for 5D4. No other camera ROM was available here
for a binary format comparison, so identical formats are not assumed.
BBB1 paint colors and AAAC paint/path references are rendered. Live checks
verified the date-dialog arrows, GPS and daylight-saving icons, plus the
main-menu camera, AF, playback, wrench, custom-settings and star icons.
Additional descriptor transforms and other camera packet layouts have not
been verified. Menu background palette fidelity is a separate limitation.
