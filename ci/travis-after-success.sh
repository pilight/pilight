#!/bin/bash -e

if [[ "$COVERAGE" == "1" && "$TRAVIS_EVENT_TYPE" != "pull_request" ]]; then
    cd build/
    gem install coveralls-lcov
    lcov -c -d CMakeFiles/pilight.dir/libs/pilight/hardware/ -d CMakeFiles/pilight.dir/libs/pilight/events/ -d CMakeFiles/pilight.dir/libs/pilight/core/ -d CMakeFiles/pilight.dir/libs/pilight/psutil/ -d CMakeFiles/pilight.dir/libs/pilight/protocols/API/ -d CMakeFiles/pilight.dir/libs/pilight/protocols/core/ -d CMakeFiles/pilight.dir/libs/pilight/protocols/generic/ -d CMakeFiles/pilight.dir/libs/pilight/protocols/network/ -d CMakeFiles/pilight.dir/libs/pilight/protocols/GPIO -o coverage.info
    lcov -r coverage.info '*433nano.c' '*IRgpio.c' '*none.c' '*zwave.cpp' '*adhoc.c' '*firmware.c' '*relay.c' '*pushover.c' '*pushbullet.c' '*dht11.c' '*dht22.c' -o coverage-filtered.info

    LCOV_INPUT_FILES="";
    for f in $(find *.info); do
		  if [ "$f" != "coverage-filtered.info" ] && [ "$f" != "coverage.info" ] && [ "$f" != "coverage-filtered-with-lua.info" ]; then
			  LCOV_INPUT_FILES="$LCOV_INPUT_FILES -a $f"
		  fi
    done

    lcov -a coverage-filtered.info $LCOV_INPUT_FILES -o coverage-filtered-with-lua.info

    lcov --list coverage-filtered-with-lua.info

    coveralls-lcov --repo-token ${COVERALLS_TOKEN} coverage-filtered-with-lua.info
fi
