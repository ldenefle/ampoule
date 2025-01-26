# Ampoule 💡

Ampoule is a tiny Zephyr library that implements a control interface over protobuf to control led strips. 

It comes with two other projects: 
+ [its proto dependencies](https://github.com/ldenefle/ampoule-protos)
+ [its example client implementation](https://github.com/ldenefle/ampoule-cli)



## Architecture

An ingestion engine gets fed protobuf encoded packets from transports, when a packet is fully assembled, it gets passed on to a rpc command handler that calls the relevant hardware peripherals and return a response. 


```
                                                                                                                             
                                                                                                                             
                                                                                                                             
                                                                                                                             
      ┌──────────────────────┐              ┌─────────────────┐           ┌──────────────┐       ┌────────────────────┐      
      │                      │              │                 │           │              │       │                    │      
      │ Transports           │              │                 │ Commands  │              │       │ Peripherals        │      
      │┌────────────────────┐│              │                 ├──────────►│              │       │ ┌────────────────┐ │      
      ││                    ││  Bytes       │                 │           │              │ Calls │ │                │ │      
      ││   Serial           ┼┼─────────────►│   Ingestion     │           │      RPC     ┼───────┼►│  Led           │ │      
      ││                    ◄┼──────────────┼                 │ Response  │              │       │ │                │ │      
      ││                    ││              │                 │◄──────────┤              │       │ └────────────────┘ │      
      │└────────────────────┘│              │                 │           │              │       │                    │      
      │                      │              │                 │           └──────────────┘       │                    │      
      │                      │              └─────────────────┘                                  │                    │      
      │                      │                                                                   │                    │      
      │                      │                                                                   │                    │      
      │                      │                                                                   │                    │      
      │                      │                                                                   │                    │      
      └──────────────────────┘                                                                   └────────────────────┘      
                                                                                                                             
                                                                                                                             
                                                                                                                             
                                                                                                                             

```

## Protocol

The protocol is transport agnostic but a serial implementation is currently provided.

The protocol uses protobuf extensively to serialize packets, in order to deal with transport packet segmentation, the protocol appends a two byte big endian size header in front of every packets.

A packet can be represented as such
```
[ size most sign, size less significant, [proto encoded bytes of size]]
```

For example a serialized packet `[0xAA, 0xBB]` will be `[0x00, 0x02, 0xAA, 0xBB]`.

## Testing

There are currently three levels of testing in Ampoule: 
+ A pure software only testsuite that test the ingestion engine, it aims to be compiled and run on native platforms although it can also be run on hardware
+ A pure software only testuite testing peripheral access using mocks 
+ A hardware testsuite running tests in python, running pytest and aimed to be run on actual hardware (a nRF52840DK currently)

To run all "native" test suites, "hardware" suites will only be compiled.

```
west twister -v \
    -T tests \
    --inline-logs
```

To run "hardware" test suites,

```
west twister -p nrf52840dk/nrf52840 \
    --device-testing --device-serial /dev/ttyACM0 --inline-logs
```
