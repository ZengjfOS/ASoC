# RPI ALSA Tinyalsa

## 参考文档

* [分析树莓派的启动、工作原理，借鉴其软件架构设计上的优点。](https://github.com/ZengjfOS/RaspberryPi)
* [tinyalsa](https://github.com/tinyalsa/tinyalsa)

## 设备节点信息

* `/dev/snd`
  ```
  pi@raspberrypi:/dev/snd $ ls -al
  total 0
  drwxr-xr-x   3 root root      160 Feb 19 02:38 .
  drwxr-xr-x  14 root root     3360 Feb 19 02:38 ..
  drwxr-xr-x   2 root root       60 Feb 19 02:38 by-path
  crw-rw----+  1 root audio 116,  0 Feb 19 02:38 controlC0
  crw-rw----+  1 root audio 116, 16 Feb 19 02:38 pcmC0D0p
  crw-rw----+  1 root audio 116, 17 Feb 19 02:38 pcmC0D1p
  crw-rw----+  1 root audio 116,  1 Feb 19 02:37 seq
  crw-rw----+  1 root audio 116, 33 Feb 19 02:38 timer
  ```
* `/proc/asound/`
  ```
  pi@raspberrypi:/proc/asound $ ls -al
  total 0
  dr-xr-xr-x   4 root root 0 Feb 19 02:38 .
  dr-xr-xr-x 103 root root 0 Jan  1  1970 ..
  lrwxrwxrwx   1 root root 5 Feb 19 02:49 ALSA -> card0
  dr-xr-xr-x   4 root root 0 Feb 19 02:49 card0
  -r--r--r--   1 root root 0 Feb 19 02:49 cards
  -r--r--r--   1 root root 0 Feb 19 02:49 devices
  -r--r--r--   1 root root 0 Feb 19 02:49 modules
  -r--r--r--   1 root root 0 Feb 19 02:49 pcm
  dr-xr-xr-x   2 root root 0 Feb 19 02:49 seq
  -r--r--r--   1 root root 0 Feb 19 02:49 timers
  -r--r--r--   1 root root 0 Feb 19 02:49 version
  ```
* `/sys/class/sound/`
  ```
  pi@raspberrypi:/sys/class/sound $ ls -al
  total 0
  drwxr-xr-x  2 root root 0 Feb 19 02:37 .
  drwxr-xr-x 53 root root 0 Nov  3  2016 ..
  lrwxrwxrwx  1 root root 0 Feb 19 02:38 card0 -> ../../devices/platform/soc/soc:audio/bcm2835_alsa/sound/card0
  lrwxrwxrwx  1 root root 0 Feb 19 02:51 controlC0 -> ../../devices/platform/soc/soc:audio/bcm2835_alsa/sound/card0/controlC0
  lrwxrwxrwx  1 root root 0 Feb 19 02:51 pcmC0D0p -> ../../devices/platform/soc/soc:audio/bcm2835_alsa/sound/card0/pcmC0D0p
  lrwxrwxrwx  1 root root 0 Feb 19 02:51 pcmC0D1p -> ../../devices/platform/soc/soc:audio/bcm2835_alsa/sound/card0/pcmC0D1p
  lrwxrwxrwx  1 root root 0 Feb 19 02:51 timer -> ../../devices/virtual/sound/timer
  ```

## tinyalsa安装简易使用

```S
root@raspberrypi:/home/pi/zengjf# wget https://github.com/tinyalsa/tinyalsa/archive/master.zip
--2019-02-19 06:20:14--  https://github.com/tinyalsa/tinyalsa/archive/master.zip
Resolving github.com (github.com)... 52.74.223.119, 13.250.177.223, 13.229.188.59
Connecting to github.com (github.com)|52.74.223.119|:443... connected.
HTTP request sent, awaiting response... 302 Found
Location: https://codeload.github.com/tinyalsa/tinyalsa/zip/master [following]
--2019-02-19 06:20:15--  https://codeload.github.com/tinyalsa/tinyalsa/zip/master
Resolving codeload.github.com (codeload.github.com)... 13.229.189.0, 54.251.140.56, 13.250.162.133
Connecting to codeload.github.com (codeload.github.com)|13.229.189.0|:443... connected.
HTTP request sent, awaiting response... 200 OK
Length: unspecified [application/zip]
Saving to: ‘master.zip’

master.zip                                          [   <=>                                                                                               ]  86.92K   136KB/s    in 0.6s

2019-02-19 06:20:17 (136 KB/s) - ‘master.zip’ saved [89005]

root@raspberrypi:/home/pi/zengjf# unzip master.zip
Archive:  master.zip
52c1bf5fbdc4c9a47825a1da5e5b7340f35cef66
   creating: tinyalsa-master/
  inflating: tinyalsa-master/.gitignore
  inflating: tinyalsa-master/.travis.yml
  inflating: tinyalsa-master/Android.bp
  inflating: tinyalsa-master/CMakeLists.txt
  inflating: tinyalsa-master/Makefile
  inflating: tinyalsa-master/NOTICE
  inflating: tinyalsa-master/README.md
   creating: tinyalsa-master/debian/
  inflating: tinyalsa-master/debian/changelog
 extracting: tinyalsa-master/debian/compat
  inflating: tinyalsa-master/debian/control
  inflating: tinyalsa-master/debian/copyright
 extracting: tinyalsa-master/debian/libtinyalsa-dev.dirs.in
  inflating: tinyalsa-master/debian/libtinyalsa-dev.install.in
 extracting: tinyalsa-master/debian/libtinyalsa.dirs.in
  inflating: tinyalsa-master/debian/libtinyalsa.install.in
  inflating: tinyalsa-master/debian/rules
  inflating: tinyalsa-master/debian/tinyalsa.dirs
  inflating: tinyalsa-master/debian/tinyalsa.install
   creating: tinyalsa-master/doxygen/
  inflating: tinyalsa-master/doxygen/Doxyfile
  inflating: tinyalsa-master/doxygen/Makefile
  inflating: tinyalsa-master/doxygen/mainpage.h
   creating: tinyalsa-master/examples/
  inflating: tinyalsa-master/examples/Makefile
  inflating: tinyalsa-master/examples/meson.build
  inflating: tinyalsa-master/examples/pcm-readi.c
  inflating: tinyalsa-master/examples/pcm-writei.c
   creating: tinyalsa-master/include/
   creating: tinyalsa-master/include/tinyalsa/
  inflating: tinyalsa-master/include/tinyalsa/asoundlib.h
  inflating: tinyalsa-master/include/tinyalsa/attributes.h
  inflating: tinyalsa-master/include/tinyalsa/interval.h
  inflating: tinyalsa-master/include/tinyalsa/limits.h
  inflating: tinyalsa-master/include/tinyalsa/meson.build
  inflating: tinyalsa-master/include/tinyalsa/mixer.h
  inflating: tinyalsa-master/include/tinyalsa/pcm.h
  inflating: tinyalsa-master/include/tinyalsa/version.h
  inflating: tinyalsa-master/meson.build
  inflating: tinyalsa-master/meson_options.txt
   creating: tinyalsa-master/scripts/
  inflating: tinyalsa-master/scripts/travis-build.sh
  inflating: tinyalsa-master/scripts/version.py
   creating: tinyalsa-master/src/
  inflating: tinyalsa-master/src/Makefile
  inflating: tinyalsa-master/src/limits.c
  inflating: tinyalsa-master/src/mixer.c
  inflating: tinyalsa-master/src/pcm.c
   creating: tinyalsa-master/utils/
  inflating: tinyalsa-master/utils/Makefile
  inflating: tinyalsa-master/utils/meson.build
  inflating: tinyalsa-master/utils/tinycap.1
  inflating: tinyalsa-master/utils/tinycap.c
  inflating: tinyalsa-master/utils/tinymix.1
  inflating: tinyalsa-master/utils/tinymix.c
  inflating: tinyalsa-master/utils/tinypcminfo.1
  inflating: tinyalsa-master/utils/tinypcminfo.c
  inflating: tinyalsa-master/utils/tinyplay.1
  inflating: tinyalsa-master/utils/tinyplay.c
  inflating: tinyalsa-master/utils/tinywavinfo.c
root@raspberrypi:/home/pi/zengjf# ls
dts  master.zip  spi-tools-master  tinyalsa-master  tmux
root@raspberrypi:/home/pi/zengjf# cd tinyalsa-master/
root@raspberrypi:/home/pi/zengjf/tinyalsa-master# ls
Android.bp  CMakeLists.txt  debian  doxygen  examples  include  Makefile  meson.build  meson_options.txt  NOTICE  README.md  scripts  src  utils
root@raspberrypi:/home/pi/zengjf/tinyalsa-master# make
make -C src
make[1]: Entering directory '/home/pi/zengjf/tinyalsa-master/src'
gcc -Wall -Wextra -Werror -Wfatal-errors -I ../include -fPIC    -c -o limits.o limits.c
gcc -Wall -Wextra -Werror -Wfatal-errors -I ../include -fPIC    -c -o mixer.o mixer.c
gcc -Wall -Wextra -Werror -Wfatal-errors -I ../include -fPIC    -c -o pcm.o pcm.c
ar rv libtinyalsa.a limits.o mixer.o pcm.o
ar: creating libtinyalsa.a
a - limits.o
a - mixer.o
a - pcm.o
gcc  -shared -Wl,-soname,libtinyalsa.so.1 limits.o mixer.o pcm.o -o libtinyalsa.so.1.1.1
ln -sf libtinyalsa.so.1.1.1 libtinyalsa.so.1
ln -sf libtinyalsa.so.1 libtinyalsa.so
make[1]: Leaving directory '/home/pi/zengjf/tinyalsa-master/src'
make -C utils
make[1]: Entering directory '/home/pi/zengjf/tinyalsa-master/utils'
gcc -Wall -Wextra -Werror -Wfatal-errors -I ../include -fPIC -O2   -c -o tinyplay.o tinyplay.c
gcc -L ../src -pie  tinyplay.o ../src/libtinyalsa.a   -o tinyplay
gcc -Wall -Wextra -Werror -Wfatal-errors -I ../include -fPIC -O2   -c -o tinycap.o tinycap.c
gcc -L ../src -pie  tinycap.o ../src/libtinyalsa.a   -o tinycap
gcc -Wall -Wextra -Werror -Wfatal-errors -I ../include -fPIC -O2   -c -o tinymix.o tinymix.c
gcc -L ../src -pie  tinymix.o ../src/libtinyalsa.a   -o tinymix
gcc -Wall -Wextra -Werror -Wfatal-errors -I ../include -fPIC -O2   -c -o tinypcminfo.o tinypcminfo.c
gcc -L ../src -pie  tinypcminfo.o ../src/libtinyalsa.a   -o tinypcminfo
make[1]: Leaving directory '/home/pi/zengjf/tinyalsa-master/utils'
make -C doxygen
make[1]: Entering directory '/home/pi/zengjf/tinyalsa-master/doxygen'
Makefile:11: "doxygen is not available please install it"
make[1]: Nothing to be done for 'all'.
make[1]: Leaving directory '/home/pi/zengjf/tinyalsa-master/doxygen'
make -C examples
make[1]: Entering directory '/home/pi/zengjf/tinyalsa-master/examples'
gcc -Wall -Wextra -Werror -Wfatal-errors -I ../include    pcm-readi.c ../src/libtinyalsa.so   -o pcm-readi
gcc -Wall -Wextra -Werror -Wfatal-errors -I ../include    pcm-writei.c ../src/libtinyalsa.so   -o pcm-writei
make[1]: Leaving directory '/home/pi/zengjf/tinyalsa-master/examples'
root@raspberrypi:/home/pi/zengjf/tinyalsa-master# make install
install -d /usr/local/include/tinyalsa/
install include/tinyalsa/attributes.h /usr/local/include/tinyalsa/
install include/tinyalsa/pcm.h /usr/local/include/tinyalsa/
install include/tinyalsa/mixer.h /usr/local/include/tinyalsa/
install include/tinyalsa/asoundlib.h /usr/local/include/tinyalsa/
install include/tinyalsa/version.h /usr/local/include/tinyalsa/
make -C src install
make[1]: Entering directory '/home/pi/zengjf/tinyalsa-master/src'
install -d /usr/local/lib/
install libtinyalsa.a /usr/local/lib/
install libtinyalsa.so.1.1.1 /usr/local/lib/
ln -sf libtinyalsa.so.1.1.1 /usr/local/lib/libtinyalsa.so.1
ln -sf libtinyalsa.so.1 /usr/local/lib/libtinyalsa.so
make[1]: Leaving directory '/home/pi/zengjf/tinyalsa-master/src'
make -C utils install
make[1]: Entering directory '/home/pi/zengjf/tinyalsa-master/utils'
install -d /usr/local/bin
install tinyplay /usr/local/bin/
install tinycap /usr/local/bin/
install tinymix /usr/local/bin/
install tinypcminfo /usr/local/bin/
install -d /usr/local/share/man/man1
install tinyplay.1 /usr/local/share/man/man1/
install tinycap.1 /usr/local/share/man/man1/
install tinymix.1 /usr/local/share/man/man1/
install tinypcminfo.1 /usr/local/share/man/man1/
make[1]: Leaving directory '/home/pi/zengjf/tinyalsa-master/utils'
make -C doxygen install
make[1]: Entering directory '/home/pi/zengjf/tinyalsa-master/doxygen'
make[1]: Nothing to be done for 'install'.
make[1]: Leaving directory '/home/pi/zengjf/tinyalsa-master/doxygen'
root@raspberrypi:/home/pi/zengjf/tinyalsa-master# tiny
tinycap      tinymix      tinypcminfo  tinyplay
root@raspberrypi:/home/pi/zengjf/tinyalsa-master# tinypcminfo
Info for card 0, device 0:

PCM out:
      Access:   0x000009
   Format[0]:   0x000006
   Format[1]:   00000000
 Format Name:   U8, S16_LE
   Subformat:   0x000001
        Rate:   min=8000Hz      max=48000Hz
    Channels:   min=1           max=2
 Sample bits:   min=8           max=16
 Period size:   min=256         max=131072
Period count:   min=1           max=128

PCM in:
cannot open device '/dev/snd/pcmC0D0c': No such file or directory
Device does not exist.
```