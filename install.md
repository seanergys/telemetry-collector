# Installation

## dep

```
# ice2
module load gcc/14.2.0-gcc-13.2.0-57z6vik   

```


## capnp

```
#why v0.8?  version dep in -> /home/di93zad/seanergys/dataplane/seanerbus/bindings/cpp/build/seanerbus/msg/msg.capnp.h

curl -O https://capnproto.org/capnproto-c++-0.8.0.tar.gz
tar zxf capnproto-c++-0.8.0.tar.gz

cd capnproto-c++-0.8.0

./configure --prefix=/home/di93zad/seanergys/telemetry-collector/install/capnp-0.8 CXXFLAGS="-fPIC" CFLAGS="-fPIC"
make -j install


export PATH="/home/di93zad/seanergys/telemetry-collector/install/capnp-0.8/bin:$PATH"
export LD_LIBRARY_PATH="/home/di93zad/seanergys/telemetry-collector/install/capnp-0.8/lib:$LD_LIBRARY_PATH"
export PKG_CONFIG_PATH="/home/di93zad/seanergys/telemetry-collector/install/capnp-0.8/lib/pkgconfig:$PKG_CONFIG_PATH"
```


## yaml-cpp

```
git clone https://github.com/jbeder/yaml-cpp.git
cd yaml-cpp
mkdir build && cd build

cmake ..   -DCMAKE_INSTALL_PREFIX=/home/di93zad/seanergys/telemetry-collector/install/yaml-cpp   -DCMAKE_BUILD_TYPE=Release -DYAML_BUILD_SHARED_LIBS=OFF -DCMAKE_CXX_FLAGS="-fPIC" -DCMAKE_POSITION_INDEPENDENT_CODE=ON

make -j
make -j install
```


## seanerbus

```
# rebuild with v0.8

cd dataplane/seanerbus/bindings/cpp/build
rm -rf CMakeCache.txt CMakeFiles/  

cmake .. -DCMAKE_INSTALL_PREFIX=../../../install_seanerbus2 -DCMAKE_POSITION_INDEPENDENT_CODE=ON

make -j install
```


