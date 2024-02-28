#!/bin/bash

########################
### Helper functions ###
########################

# Define the upstream repository and file path to the upstream extensions_build_config.bzl
# UPSTREAM_REPO will be overridden in get_UPSTREAM_REPO
UPSTREAM_REPO="envoyproxy/envoy"
UPSTREAM_FILE_PATH="source/extensions/extensions_build_config.bzl"
# Define the path to the local repository_locations.bzl, which contains the envoy commit hash
REPOSITORY_LOCATIONS_FILE="./bazel/repository_locations.bzl"
# Define the path to the local version
ENVOY_GLOO_FILE="./bazel/extensions/extensions_build_config.bzl"

get_UPSTREAM_REPO() {
    local file_path="$1"
    local remote
    remote=$(grep -A 4 "envoy =" "$file_path" | grep remote | cut -d '"' -f 2 | cut -d '/' -f 4,5)
    echo "$remote"
}

# Function to extract the envoy commit hash from repository_locations.bzl
get_envoy_commit_hash() {
    local file_path="$1"
    local commit_hash
    commit_hash=$(grep -A 2 "envoy =" "$file_path" | grep commit | cut -d '"' -f 2)
    echo "$commit_hash"
}

# Function to report an error
report_error() {
    local line_number=$1
    local line_content="$2"
    local github_line_url="https://github.com/$UPSTREAM_REPO/blob/$ENVOY_COMMIT_HASH/$UPSTREAM_FILE_PATH#L$line_number"
    echo "Error: Line not found in the envoy-gloo version of extensions_build_config.bzl at line $line_number\n\t$line_content\n\tSee: $github_line_url"
}

########################
#### Main execution ####
########################

UPSTREAM_REPO=$(get_UPSTREAM_REPO "$REPOSITORY_LOCATIONS_FILE")
if [ -z "$UPSTREAM_REPO" ]; then
    echo "Error: Failed to extract envoy repo from $REPOSITORY_LOCATIONS_FILE"
    exit 1
fi

# Extract the envoy commit hash
ENVOY_COMMIT_HASH=$(get_envoy_commit_hash "$REPOSITORY_LOCATIONS_FILE")
if [ -z "$ENVOY_COMMIT_HASH" ]; then
    echo "Error: Failed to extract envoy commit hash from $REPOSITORY_LOCATIONS_FILE"
    exit 1
fi

# Update the URL to point to the specific envoy commit
UPSTREAM_URL="https://raw.githubusercontent.com/$UPSTREAM_REPO/$ENVOY_COMMIT_HASH/$UPSTREAM_FILE_PATH"

curl -s "$UPSTREAM_URL" -o upstream_file.tmp
if [ $? -ne 0 ]; then
    echo "Error: Failed to fetch the upstream file from $UPSTREAM_URL"
    exit 1
fi

# Initialize associative arrays to store the upstream and envoy-gloo-ee KV pairs
declare -A UPSTREAM_LINE_NUMBERS UPSTREAM_KV ENVOY_GLOO_KV

# Read the upstream version of the file into an array, including empty lines.
# We skip commented lines here as we're not particularly concerned with extensions
# that upstream doesn't compile in.
UPSTREAM_LINE_NUM=1
while IFS= read -r line || [[ -n "$line" ]]; do
    if [[ $line == *#* ]]; then
        UPSTREAM_LINE_NUM=$((UPSTREAM_LINE_NUM+1))
        continue
    fi

    # Split line into an array (kv) at each double quote
    # We are interested in lines of the form: "<extension name>": "<extension filepath>",
    # where there may be signficant whitespace in any of the non-quoted regions.
    # If the line matches this format, kv[1] is the extension name and kv[3] is the extension filepath
    IFS='"' read -ra kv <<< "$line"
    if [[ ${#kv[@]} = 5 ]]; then
        UPSTREAM_KV[${kv[1]}]=${kv[3]}
        UPSTREAM_LINE_NUMBERS[${kv[1]}]=${UPSTREAM_LINE_NUM}
    fi
    UPSTREAM_LINE_NUM=$((UPSTREAM_LINE_NUM+1))
done < upstream_file.tmp

# Read the envoy-gloo version of the file into an array, including empty lines
# We allow commented lines here to enable us to not compile extensions we don't want
# or need, but this makes sure that it was a deliberate omission.
while IFS= read -r line || [[ -n "$line" ]]; do
    IFS='"' read -ra kv <<< "$line"
    [[ ${#kv[@]} = 5 ]] && ENVOY_GLOO_KV[${kv[1]}]=${kv[3]}
done < "$ENVOY_GLOO_FILE"

# Flag to track if any issues are found
ISSUES_FOUND=false
ERRORS=()

# Loop over the KVs we got from upstream and make sure we have them here
for UPSTREAM_KV_ENTRY in "${!UPSTREAM_KV[@]}"; do
    if [[ -z "${ENVOY_GLOO_KV[$UPSTREAM_KV_ENTRY]+exists}" ]]; then
        ISSUES_FOUND=true
        ERRORS+=("$(report_error "${UPSTREAM_LINE_NUMBERS[$UPSTREAM_KV_ENTRY]}" "\"$UPSTREAM_KV_ENTRY\":\"${UPSTREAM_KV[$UPSTREAM_KV_ENTRY]}\"")")
    fi
done

# Cleanup
rm upstream_file.tmp

# Check if any issues were found
if [ "$ISSUES_FOUND" = true ]; then
    for err in "${ERRORS[@]}"; do
        echo -e "$err"
    done
    echo "Envoy-gloo extensions_build_config.bzl is not up to date with the upstream version."
    exit 1
else
    echo "Envoy-gloo extensions_build_config.bzl is up to date with the upstream version."
fi
