#!/usr/bin/env bash
# Starts an isolated Garage server, creates a test bucket, and runs the S3
# integration test. The Docker volume is removed on exit.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
declare -r HERE
declare -r PROJECT="gunrunners-garage-test"
declare -r BUCKET="gunrunners-test"
declare -r KEY_NAME="gunrunners-test"
declare -a COMPOSE

if docker info >/dev/null 2>&1; then
    COMPOSE=(docker compose)
elif command -v podman-compose >/dev/null; then
    COMPOSE=(podman-compose)
else
    echo "Docker or Podman Compose is required" >&2
    exit 1
fi

_compose() {
    "${COMPOSE[@]}" --project-name "$PROJECT" --file "$HERE/compose.yml" "$@"
}

_garage() {
    _compose exec -T garage /garage -c /etc/garage.toml "$@"
}

_cleanup() {
    _compose down --volumes --remove-orphans || true
}

_wait_for_garage() {
    local -i attempt
    for ((attempt = 0; attempt < 60; ++attempt)); do
        if _garage status >/dev/null 2>&1; then
            return
        fi
        sleep 1
    done
    _compose logs garage >&2
    return 1
}

main() {
    trap _cleanup EXIT
    _compose up --detach --wait
    _wait_for_garage

    local node_id
    node_id="$(_garage node id --quiet)"
    node_id="${node_id%%@*}"
    _garage layout assign --zone dc1 --capacity 1G "$node_id"
    _garage layout apply --version 1

    local key_output access_key secret_key
    key_output="$(_garage key create "$KEY_NAME")"
    access_key="$(sed -n 's/^Key ID: *//p' <<<"$key_output")"
    secret_key="$(sed -n 's/^Secret key: *//p' <<<"$key_output")"
    [[ -n "$access_key" && -n "$secret_key" ]]

    _garage bucket create "$BUCKET"
    _garage bucket allow --read --write --owner \
        "$BUCKET" --key "$KEY_NAME"

    cmake -S "$HERE/../.." -B "$HERE/../../build"
    cmake --build "$HERE/../../build" --target s3_test --parallel
    GUNRUNNERS_TEST_S3_ENDPOINT=http://127.0.0.1:3900 \
    GUNRUNNERS_TEST_S3_BUCKET="$BUCKET" \
    GUNRUNNERS_TEST_S3_REGION=garage \
    GUNRUNNERS_TEST_S3_ACCESS_KEY="$access_key" \
    GUNRUNNERS_TEST_S3_SECRET_KEY="$secret_key" \
        ctest --test-dir "$HERE/../../build" --output-on-failure -R '^s3$'
}

main "$@"
