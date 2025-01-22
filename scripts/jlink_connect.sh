#!/bin/bash

export DIR="./build/zephyr"
if [[ -f ${DIR}"/merged.hex" ]]; then
    cp ${DIR}/merged.hex ${DIR}/zephyr_rtt.hex
else
    cp ${DIR}/zephyr.hex ${DIR}/zephyr_rtt.hex
fi
JLinkExe -CommanderScript scripts/startup.jlink -NoGui 1

