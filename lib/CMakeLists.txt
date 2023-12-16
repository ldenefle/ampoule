zephyr_library_named(ampoule)

zephyr_library_sources(
    ingestion.c
    command.c
)

zephyr_library_sources_ifdef(CONFIG_AMPOULE_TRANSPORT_SERIAL transports/serial.c)

add_dependencies(ampoule ampoule_protos)
