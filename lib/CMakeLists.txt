zephyr_library_named(ampoule)

zephyr_library_sources(
    ingestion.c
    command.c
)

add_dependencies(ampoule ampoule_protos)
