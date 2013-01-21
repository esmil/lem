# Maintainer: Emil Renner Berthing <esmil@mailme.dk>

_lua='builtin'
#_lua='lua'
#_lua='lua5.1' # works, but not recommended
#_lua='luajit'

pkgname=lem
pkgver=0.3
pkgrel=1
pkgdesc='A Lua Event Machine'
arch=('i686' 'x86_64' 'armv5tel' 'armv7l')
url='https://github.com/esmil/lem'
license=('LGPL')
case "$_lua" in
builtin) depends=('glibc');;
lua)     depends=('lua');;
lua5.1)  depends=('lua51');;
luajit)  depends=('luajit');;
esac
source=()

build() {
  cd "$startdir"

  ./configure --prefix=/usr --with-lua=$_lua
  make
}

package() {
  cd "$startdir"

  make DESTDIR="$pkgdir/" install
}

# vim:set ts=2 sw=2 et:
