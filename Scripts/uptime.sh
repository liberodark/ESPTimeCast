#!/usr/bin/env bash

URL="http://192.168.0.216/webhook"
KEY="SECRET123"

TARGETS=(
    "https://website.com/"
    "1.1.1.1"
)

check_target() {
    local target="$1"

    if [[ "$target" =~ ^https?:// ]]; then
        curl -sL -w "%{http_code}" -o /dev/null --max-time 10 "$target"
    else
        if ping -c 1 -W 5 "$target" > /dev/null 2>&1; then
            echo "200"
        else
            echo "000"
        fi
    fi
}

get_name() {
    local target="$1"

    if [[ "$target" =~ ^https?:// ]]; then
        echo "$target" | sed -E 's|https?://([^/]+).*|\1|'
    else
        echo "$target"
    fi
}

while true; do
    for target in "${TARGETS[@]}"; do
        CODE=$(check_target "$target")
        if [ "$CODE" != "200" ]; then
            NAME=$(get_name "$target")
            curl -sX POST $URL \
              -d "key=$KEY" \
              -d "message=$NAME DOWN" \
              -d "priority=2" \
              -d "duration=60"
        fi
    done
    sleep 300
done
