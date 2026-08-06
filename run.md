# Run

## t1

```
cd seanergys/telemetry-collector
./telemetry-collector config.yaml
```

## t2 

```
cd seanergys/dataplane/seanerbus/server
RUST_LOG=debug cargo run
```

## t3 

```
cd seanergys/dataplane/seanerbus/cpp_examples
./subscribe_test 6bc3d210-a49f-4422-8fe8-26ca480d0e1f
```

## export

```
export PKG_CONFIG_PATH="$HOME/seanergys/telemetry-collector/install/capnp-0.8/lib/pkgconfig:$PKG_CONFIG_PATH"
export PATH="$HOME/seanergys/telemetry-collector/install/capnp-0.8/bin:$PATH"
export LD_LIBRARY_PATH="$HOME/seanergys/telemetry-collector/install/capnp-0.8/lib:$LD_LIBRARY_PATH"

export PATH="$HOME/seanergys/telemetry-collector/install/net-snmp/bin:$HOME/seanergys/telemetry-collector/install/net-snmp/sbin:$PATH"
export LD_LIBRARY_PATH="$HOME/seanergys/telemetry-collector/install/net-snmp/lib:$LD_LIBRARY_PATH"
```
