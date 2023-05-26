#pragma once

#include "source/common/common/logger.h"

#define TRANSFORMATION_LOG(LEVEL, FORMAT, STREAM, ...)                  \
  do {                                                                  \
      ENVOY_STREAM_LOG(LEVEL, FORMAT, STREAM, ##__VA_ARGS__);           \
  } while (0)

#define TRANSFORMATION_LOG_IF(LEVEL, CONDITION, FORMAT, STREAM, ...)        \
    do {                                                                    \
        if (CONDITION) {                                                    \
            TRANSFORMATION_LOG(LEVEL, FORMAT, STREAM, ##__VA_ARGS__);       \
        }                                                                   \
    } while (0)

#define TRANSFORMATION_SENSITIVE_LOG(LEVEL, FORMAT, TRANSFORMATION, FILTER_CONFIG, STREAM, ...)                                 \
    do {                                                                                                 \
        TRANSFORMATION_LOG_IF(LEVEL, ((FILTER_CONFIG)->logRequestResponseInfo() || (TRANSFORMATION)->logRequestResponseInfo()), FORMAT, STREAM, ##__VA_ARGS__); \
    } while (0)