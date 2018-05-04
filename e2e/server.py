#!/usr/bin/env python

from concurrent import futures
import time

import grpc

import cache_pb2
import cache_pb2_grpc

_ONE_DAY_IN_SECONDS = 60 * 60 * 24


# TODO(talnordan): This service is not thread-safe.
class CacheServicer(cache_pb2_grpc.CacheServiceServicer):
  """Provides methods that implement functionality of cache server."""

  def __init__(self):
    self.cache = {}

  def Get(self, request, context):
    key = request.key
    default = ""
    value = self.cache.get(key, default)
    response = cache_pb2.CacheGetResponse(value=value)
    return response

  def Set(self, request, context):
    key = request.key
    value = request.value
    self.cache[key] = value
    response = cache_pb2.CacheSetResponse()
    return response


def serve():
  server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
  cache_pb2_grpc.add_CacheServiceServicer_to_server(CacheServicer(), server)
  server.add_insecure_port('127.0.0.1:50051')
  server.start()
  try:
    while True:
      time.sleep(_ONE_DAY_IN_SECONDS)
  except KeyboardInterrupt:
    server.stop(0)

if __name__ == '__main__':
  serve()
