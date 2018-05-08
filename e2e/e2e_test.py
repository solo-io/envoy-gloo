import grpc
import httplib
import logging
import multiprocessing
import os
import requests
import signal
import subprocess
import time
import unittest

# Generate Python code.
# See: https://grpc.io/docs/tutorials/basic/python.html
os.system("python -m grpc_tools.protoc -I./api/envoy/service/cache/v2/ "
          "--python_out=. --grpc_python_out=. ./api/envoy/service/cache/v2/cache.proto")

import cache_pb2
import cache_pb2_grpc


def envoy_preexec_fn():
  import ctypes
  PR_SET_PDEATHSIG = 1  # See prtcl(2).
  os.setpgrp()
  libc = ctypes.CDLL(ctypes.util.find_library('c'), use_errno=True)
  libc.prctl(PR_SET_PDEATHSIG, signal.SIGTERM)

DEBUG=True

class CacheTestCase(unittest.TestCase):
  def setUp(self):
    self.cleanup()

  def tearDown(self):
    for p in (self.upstream, self.grpc_server, self.redis_server):
      if p is not None:
        p.terminate()
    if self.envoy is not None:
      self.envoy.send_signal(signal.SIGINT)
      self.envoy.wait()

    self.cleanup()

  def cleanup(self):
    self.upstream = None
    self.grpc_server = None
    self.redis_server = None
    self.envoy = None

  def __create_config(self):
    for create_config_path in ("./create_config.sh", "./e2e/create_config.sh"):
      if os.path.isfile(create_config_path):
        subprocess.check_call(create_config_path)
        break
    else:
      self.fail('"create_config.sh" was not found')

  def __start_upstream(self):
    for upstream_path in ("./upstream.py", "./e2e/upstream.py"):
      if os.path.isfile(upstream_path):
        self.upstream = subprocess.Popen([upstream_path])
        break
    else:
      self.fail('"upstream.py" was not found')

  def __start_grpc_server(self):
    for grpc_server_path in ("./server.py", "./e2e/server.py"):
      if os.path.isfile(grpc_server_path):
        self.grpc_server = subprocess.Popen([grpc_server_path])
        time.sleep(1)
        break
    else:
      self.fail('"server.py" was not found')

  def __start_redis_server(self):
    self.redis_server = subprocess.Popen(["docker", "run", "--rm", "-p", "6379:6379", "redis"])

  def __start_envoy(self, prefix = None, suffix = None):
    if prefix is None:
      prefix = []
    if suffix is None:
      suffix = suffix = ["--log-level", "debug"] if DEBUG else []

    envoy = os.environ.get("TEST_ENVOY_BIN","envoy")

    self.envoy = subprocess.Popen(prefix + [envoy, "-c", "./envoy.yaml"]+suffix, preexec_fn=envoy_preexec_fn)
    time.sleep(5)

  def __make_request(self, expected_status):
    response = requests.get('http://localhost:10000/get')
    self.assertEqual(expected_status, response.status_code)
    return response.text

  def test_grpc_server(self):
    # Set up gRPC server.
    self.__start_grpc_server()

    # Connect a gRPC client.
    channel = grpc.insecure_channel('127.0.0.1:50051')
    stub = cache_pb2_grpc.CacheServiceStub(channel)

    # Send a `SET` request.
    set_request = cache_pb2.CacheSetRequest(key="mykey", value="Hello")
    stub.Set(set_request)

    # Send a `GET` request.
    get_request = cache_pb2.CacheGetRequest(key="mykey")

    # Validate that the correct value was returned.
    result = stub.Get(get_request)
    self.assertEqual("Hello", result.value)

  def test_make_requests(self):
    # Set up environment.
    self.__create_config()
    self.__start_upstream()
    self.__start_grpc_server()
    self.__start_redis_server()
    self.__start_envoy()

    # Make multiple requests and aggregate the response text values.
    response_text_set = set()
    for _ in xrange(1000):
      response_text = self.__make_request(httplib.OK)
      response_text_set.add(response_text)

    # TODO(talnordan): Should this test support a non-default Envoy configuration of the number of
    # worker threads?
    num_worker_threads = multiprocessing.cpu_count()

    # The upstream counter is expected to be incremented once by each worker thread.
    expected_sorted_responses = range(1, num_worker_threads + 1)

    # Validate the aggregated response text values.
    sorted_responses = sorted(map(int, response_text_set))
    self.assertEqual(expected_sorted_responses, sorted_responses)

if __name__ == "__main__":
  global DEBUG
  DEBUG =  True if os.environ.get("DEBUG","") != "0" else False
  if DEBUG:
    logging.basicConfig(level=logging.DEBUG)
  unittest.main()
