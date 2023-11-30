#!/bin/bash

########################
### Helper functions ###
########################

# Define the upstream repository and file path
UPSTREAM_REPO="envoyproxy/envoy"
UPSTREAM_FILE_PATH="source/extensions/extensions_build_config.bzl"
UPSTREAM_URL="https://raw.githubusercontent.com/$UPSTREAM_REPO/main/$UPSTREAM_FILE_PATH"

# Define the path to the local version
ENVOY_GLOO_FILE="./bazel/extensions/extensions_build_config.bzl"

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
    local github_line_url="https://github.com/$UPSTREAM_REPO/blob/main/$UPSTREAM_FILE_PATH#L$line_number"
    echo "Error: Line not found in the envoy-gloo version of extensions_build_config.bzl at line $line_number\n\t$line_content\n\tSee: $github_line_url"
}

########################
#### Main execution ####
########################

# Initialize arrays to store the upstream and envoy-gloo versions of the file
declare -a UPSTREAM_LINES ENVOY_GLOO_LINES

# Read the upstream version of the file into an array
curl -s "$UPSTREAM_URL" -o upstream_file.tmp
if [ $? -ne 0 ]; then
    echo "Error: Failed to fetch the upstream file from $UPSTREAM_URL"
    exit 1
fi
IFS=$'\n' read -r -d '' -a UPSTREAM_LINES < upstream_file.tmp

# Check if the envoy-gloo file exists and is readable
if [ ! -r "$ENVOY_GLOO_FILE" ]; then
    echo "Error: The envoy-gloo file $ENVOY_GLOO_FILE does not exist or is not readable"
    exit 1
fi
IFS=$'\n' read -r -d '' -a ENVOY_GLOO_LINES < "$ENVOY_GLOO_FILE"

# Preprocess the envoy-gloo lines for faster lookup
declare -a PROCESSED_ENVOY_GLOO_LINES
for line in "${ENVOY_GLOO_LINES[@]}"; do
    trimmed_line=$(trim "${line##\#}")
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