# mosquitto-example

This Repository contains examples of how to use MQTT using [mosquitto](https://mosquitto.org/).

## Build

```bash
cmake -B build
cmake --build build
```

## Publish

```bash
./build/mqtt_pub -t test -m hello
```

## Subscribe

```bash
./build/mqtt_sub -t test
```

## References

- [mosquitto](https://mosquitto.org/)
