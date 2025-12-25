#!/usr/bin/env bash

URL="http://192.168.0.216/webhook"
KEY="SECRET123"

DOMAINS=(
    "pine64.com"
    "google.com"
)

WARN_DAYS=30

for domain in "${DOMAINS[@]}"; do
    EXPIRY=$(echo | openssl s_client -servername "$domain" -connect "$domain:443" 2>/dev/null | openssl x509 -noout -enddate 2>/dev/null | cut -d= -f2)

    if [ -n "$EXPIRY" ]; then
        EXPIRY_TS=$(date -d "$EXPIRY" +%s)
        NOW_TS=$(date +%s)
        DAYS_LEFT=$(( (EXPIRY_TS - NOW_TS) / 86400 ))

        if [ "$DAYS_LEFT" -lt 0 ]; then
            curl -sX POST $URL \
              -d "key=$KEY" \
              -d "message=$domain SSL EXPIRE!" \
              -d "priority=2" \
              -d "duration=60"
        elif [ "$DAYS_LEFT" -lt "$WARN_DAYS" ]; then
            curl -sX POST $URL \
              -d "key=$KEY" \
              -d "message=$domain SSL ${DAYS_LEFT}j" \
              -d "priority=1" \
              -d "duration=60"
        fi
    else
        curl -sX POST $URL \
          -d "key=$KEY" \
          -d "message=$domain SSL ERROR" \
          -d "priority=2" \
          -d "duration=60"
    fi
done
