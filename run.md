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