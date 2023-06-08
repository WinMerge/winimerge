# WinIMerge #

WinIMerge is a simple image diff/merge tool like WinMerge.

## Screenshot 

![winimerge.png](https://bitbucket.org/repo/RoKbrr/images/3384177401-winimerge.png)

## Dependencies

This software uses the FreeImage open source image library.

See http://freeimage.sourceforge.net for details.

FreeImage is used under the the GNU GPL version.

This software uses the OpenImageIO library.

See https://github.com/OpenImageIO/oiio for details.

OpenImageIO is distributed under the BSD-3-Clause license.

## How to build (Visual Studio 2019)

Download [vcpkg](https://github.com/Microsoft/vcpkg) dependency manager,
e.g. by using git:
```
  git clone https://github.com/Microsoft/vcpkg.git
```
Install [vcpkg](https://github.com/Microsoft/vcpkg):
```
  cd vcpkg
  ./bootstrap-vcpkg.bat
  ./vcpkg integrate install
```
Download and open winimerge
```
  git clone https://github.com/winmerge/winimerge
  git clone https://github.com/winmerge/freeimage
  cd winimerge
  buildbin.vs2019.cmd
```

## License

GPL2
