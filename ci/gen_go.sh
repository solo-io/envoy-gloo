#!/bin/bash
set -e
set -x

# copied from bazel_envoy_api_go_build in envoy's do_ci.sh

# TODO: if we ever have protos using other protos from our repos, we'll need to fix this to not be envoyproxy
GO_IMPORT_BASE="github.com/envoyproxy/go-control-plane/api/envoy"
GO_TARGETS=(//api/envoy/...)
read -r -a GO_PROTOS <<< "$(bazel query "${BAZEL_GLOBAL_OPTIONS[@]}" "kind('go_proto_library', ${GO_TARGETS[*]})" | tr '\n' ' ')"
bazel build "${BAZEL_BUILD_OPTIONS[@]}" \
        --experimental_proto_descriptor_sets_include_source_info \
        --remote_download_outputs=all \
        "${GO_PROTOS[@]}"
rm -rf build_go
mkdir -p build_go
echo "Copying go protos -> build_go"
BAZEL_BIN="$(bazel info "${BAZEL_BUILD_OPTIONS[@]}" bazel-bin)"
for GO_PROTO in "${GO_PROTOS[@]}"; do
        # strip @envoy_api//
    RULE_DIR="$(echo "${GO_PROTO:12}" | cut -d: -f1)"
    PROTO="$(echo "${GO_PROTO:12}" | cut -d: -f2)"
    INPUT_DIR="${BAZEL_BIN}/api/envoy/${RULE_DIR}/${PROTO}_/${GO_IMPORT_BASE}/${RULE_DIR}"
    OUTPUT_DIR="build_go/${RULE_DIR}"
    mkdir -p "$OUTPUT_DIR"
    if [[ ! -e "$INPUT_DIR" ]]; then
        echo "Unable to find input ${INPUT_DIR}" >&2
        exit 1
    fi
    # echo "Copying go files ${INPUT_DIR} -> ${OUTPUT_DIR}"
    while read -r GO_FILE; do
        cp -a "$GO_FILE" "$OUTPUT_DIR"
        if [[ "$GO_FILE" = *.validate.go ]]; then
            sed -i '1s;^;//go:build !disable_pgv\n;' "$OUTPUT_DIR/$(basename "$GO_FILE")"
        fi
    done <<< "$(find "$INPUT_DIR" -name "*.go")"
done

# remove all folders from the `go`  folder
rm -rf ./go/config
rm -rf ./go/type
cp -r build_go/* ./go
rm -rf build_go