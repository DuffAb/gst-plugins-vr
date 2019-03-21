# GStreamer VR Plugins

This repository contains GStreamer plugins for watching spherical video in VR and a Python GTK+ player SPHVR.

## Disclaimer

Gst VR Plugins are in a very early development stage, you will get motion sick :)

## Dependencies

### VR Plugins

* GStreamer
* GStreamer GL Plugins
* Meson
* OpenHMD
* libfreenect2
* graphene

### SPHVR

* Python 3
* GTK+ 3.X

### Install Library
install pip3
```
$ sudo apt install python3-pip
```
install meson
```
$ sudo pip3 install meson
```
install ninja
```
download ninja binary from: https://github.com/ninja-build/ninja/releases
then cp ninja to /usr/bin/     do not forget chmod for ninja
$ sudo apt install libassimp-dev
```
cd subprojects/graphene/ to compile graphene

Program g-ir-scanner found: NO
```
$ sudo apt install gobject-introspection
```
Couldn't find include 'GObject-2.0.gir'
```
$ sudo apt install libgirepository1.0-dev
```
## Build

```
./configure
make
```

## Usage

### View spherical video on a DK2

```
gst-launch-1.0 filesrc location=~/video.webm ! decodebin ! glupload ! glcolorconvert ! videorate ! vrcompositor ! video/x-raw\(memory:GLMemory\), width=1920, height=1080, framerate=75/1 ! hmdwarp ! glimagesink
```

### Open 2 Windows with Tee

```
GST_GL_XINITTHREADS=1 \ gst-launch-1.0 filesrc location=~/video.webm ! decodebin ! videoscale ! glupload ! glcolorconvert ! videorate ! vrcompositor ! video/x-raw\(memory:GLMemory\), width=1920, height=1080, framerate=75/1 ! hmdwarp ! tee name=t ! queue ! glimagesink t. ! queue ! glimagesink
```

### Display point cloud from Kinect v2

```
gst-launch-1.0 freenect2src sourcetype=0 ! glupload ! glcolorconvert ! pointcloudbuilder ! video/x-raw\(memory:GLMemory\), width=1920, height=1080 ! glimagesink
```


### Run vrtestsrc

```
gst-launch-1.0 vrtestsrc ! video/x-raw\(memory:GLMemory\), width=1920, height=1080 ! glimagesink
```

### Run a video in SPHVR

```
./sphvr/sphvr file:///home/bmonkey/video.webm
```

## License

LGPLv2
