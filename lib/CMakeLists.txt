zephyr_library_named(ampoule)

zephyr_library_sources(ingestion.c)

add_dependencies(ampoule ampoule_protos)
