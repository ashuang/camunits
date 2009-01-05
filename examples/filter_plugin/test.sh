#!/bin/sh

if [ ! -e build ]; then
    echo "Error.  Build first"
    exit 1
fi

CAMUNITS_PLUGIN_PATH=build camview -c test-chain.xml
