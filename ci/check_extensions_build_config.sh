#!/bin/bash

########################
### Helper functions ###
########################

# Define the upstream repository and file path to the upstream extensions_build_config.bzl
UPSTREAM_REPO="envoyproxy/envoy"
UPSTREAM_FILE_PATH="source/extensions/extensions_build_config.bzl"
# Define the path to the local repository_locations.bzl, which contains the envoy commit hash
REPOSITORY_LOCATIONS_FILE="./bazel/repository_locations.bzl"
# Define the path to the local version
ENVOY_GLOO_FILE="./bazel/extensions/extensions_build_config.bzl"

# Function to extract the envoy commit hash from repository_locations.bzl
get_envoy_commit_hash() {
    local file_path="$1"
    local commit_hash
    commit_hash=$(grep -A 6 "envoy =" "$file_path" | grep commit | cut -d '"' -f 2)
    echo "$commit_hash"
}

# Function to trim leading and trailing whitespaces
trim() {
    local var="$*"
    var="${var#"${var%%[![:space:]]*}"}"   # remove leading whitespace characters
    var="${var%"${var##*[![:space:]]}"}"   # remove trailing whitespace characters
    echo -n "$var"
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

# Initialize arrays to store the upstream and envoy-gloo versions of the file
declare -a UPSTREAM_LINES ENVOY_GLOO_LINES

# Read the upstream version of the file into an array, including empty lines
UPSTREAM_LINES=()
while IFS= read -r line || [[ -n "$line" ]]; do
    UPSTREAM_LINES+=("$line")
done < upstream_file.tmp

# Read the envoy-gloo version of the file into an array, including empty lines
ENVOY_GLOO_LINES=()
while IFS= read -r line || [[ -n "$line" ]]; do
    ENVOY_GLOO_LINES+=("$line")
done < "$ENVOY_GLOO_FILE"

# Preprocess the envoy-gloo lines for faster lookup
declare -a PROCESSED_ENVOY_GLOO_LINES
for line in "${ENVOY_GLOO_LINES[@]}"; do
    # Remove only the first leading '#' character (if present) and any spaces before it
    trimmed_line=$(echo "$line" | sed 's/^[[:space:]]*#//')

    # remove any remaining whitespace
    trimmed_line=$(trim "$trimmed_line")

    PROCESSED_ENVOY_GLOO_LINES+=("$trimmed_line")
done

# Flag to track if any issues are found
ISSUES_FOUND=false
ERRORS=()

# Loop through each line in the upstream version
LINE_NUMBER=0
for UPSTREAM_LINE in "${UPSTREAM_LINES[@]}"; do
    ((LINE_NUMBER++))
    TRIMMED_UPSTREAM_LINE=$(trim "$UPSTREAM_LINE")

    # Skip if the line is empty or a comment
    if [[ -z "$TRIMMED_UPSTREAM_LINE" || "$TRIMMED_UPSTREAM_LINE" =~ ^# ]]; then
        continue
    fi

    # Reject any lines that don't match the format "key": "value". 
    # Additional spaces around the colon and any content within the quotes are allowed.
    if ! [[ "$TRIMMED_UPSTREAM_LINE" =~ ^[^\"]*\"[^\"]*\":\ *\"[^\"]*\" ]]; then
        continue
    fi

    # Check if the trimmed line exists in the processed envoy-gloo lines
    LINE_FOUND=false
    for PROCESSED_LINE in "${PROCESSED_ENVOY_GLOO_LINES[@]}"; do
        if [[ "$PROCESSED_LINE" == "$TRIMMED_UPSTREAM_LINE" ]]; then
            LINE_FOUND=true
            break
        fi
    done

    if [ "$LINE_FOUND" = false ]; then
        ERRORS+=("$(report_error "$LINE_NUMBER" "$UPSTREAM_LINE")")
        ISSUES_FOUND=true
    fi
done

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

# Cleanup
rm upstream_file.tmp