#!/bin/bash -e

if [[ "$COVERAGE" == "1" && "$TRAVIS_EVENT_TYPE" != "pull_request" ]]; then
    cd build/
    gem install coveralls-lcov
    lcov -c -d CMakeFiles/pilight.dir/libs/pilight/hardware/ -d CMakeFiles/pilight.dir/libs/pilight/events/ -d CMakeFiles/pilight.dir/libs/pilight/core/ -d CMakeFiles/pilight.dir/libs/pilight/psutil/ -d CMakeFiles/pilight.dir/libs/pilight/protocols/API/ -d CMakeFiles/pilight.dir/libs/pilight/protocols/core/ -d CMakeFiles/pilight.dir/libs/pilight/protocols/generic/ -d CMakeFiles/pilight.dir/libs/pilight/protocols/network/ -d CMakeFiles/pilight.dir/libs/pilight/protocols/GPIO -o coverage.info
    lcov -r coverage.info '*433nano.c' '*IRgpio.c' '*none.c' '*zwave.cpp' '*adhoc.c' '*firmware.c' '*relay.c' '*pushover.c' '*pushbullet.c' '*dht11.c' '*dht22.c' -o coverage-filtered.info
    lcov --list coverage-filtered.info
    coveralls-lcov --repo-token ${COVERALLS_TOKEN} coverage-filtered.info
fi
