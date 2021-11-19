#!/bin/bash -e

function perform_lint() {
    # Read config
    local OPTIONS=$(<ci/astyle.conf)$'\n'

    # Add whitelisted files to exclude
    while read line; do
        OPTIONS+="exclude='$line'"$'\n'
    done <"ci/lint-whitelist.txt"

    # Run lint
    local RESULTS=$(astyle --formatted --recursive --suffix=none \
        --options=<(echo $OPTIONS) '*.c' '*.h')

    local FORMATTEDFILES=$(echo "$RESULTS" | grep Formatted)

    # If necessary, output diff from formatted files
    if [[ $FORMATTEDFILES != "" ]]; then
        echo "Lint failed, changes needed:"
        echo "$FORMATTEDFILES"
        git --no-pager diff --minimal
        exit 1
    fi

    echo "Lint succeeded."
}

perform_lint
