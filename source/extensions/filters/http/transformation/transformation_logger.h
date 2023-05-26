#pragma once

#include "source/common/common/logger.h"

#define TRANSFORMATION_LOG(LEVEL, FORMAT, STREAM, ...)                  \
  do {                                                                  \
      ENVOY_STREAM_LOG(LEVEL, FORMAT, STREAM, ##__VA_ARGS__);           \
      std::cout << "REQUEST_RESPONSE_LOG: " << FORMAT << std::endl;     \
  } while (0)

#define TRANSFORMATION_LOG_IF(LEVEL, CONDITION, FORMAT, STREAM, ...)        \
    do {                                                                    \
        if (CONDITION) {                                                    \
            TRANSFORMATION_LOG(LEVEL, FORMAT, STREAM, ##__VA_ARGS__);       \
        }                                                                   \
    } while (0)

#define TRANSFORMATION_SENSITIVE_LOG(LEVEL, FORMAT, PARAMS, ...)                               \
    do {                                                                                       \
        TRANSFORMATION_LOG_IF(true, LEVEL, FORMAT, (PARAMS).stream_callbacks_, ##__VA_ARGS__); \
    } while (0)