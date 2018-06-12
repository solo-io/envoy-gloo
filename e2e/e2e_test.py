import httplib
import logging
import os
import requests
import signal
import subprocess
import time
import unittest


def envoy_preexec_fn():
  import ctypes
  PR_SET_PDEATHSIG = 1  # See prtcl(2).
  os.setpgrp()
  libc = ctypes.CDLL(ctypes.util.find_library('c'), use_errno=True)
  libc.prctl(PR_SET_PDEATHSIG, signal.SIGTERM)

class ClientCertificateRestrictionTestCase(unittest.TestCase):
  def setUp(self):
    self.cleanup()

  def tearDown(self):
    if self.upstream is not None:
      self.upstream.terminate()
    if self.envoy is not None:
      self.envoy.send_signal(signal.SIGINT)
      self.envoy.wait()

    self.cleanup()

  def cleanup(self):
    self.upstream = None
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

  def __start_envoy(self, prefix = None, suffix = None):
    if prefix is None:
      prefix = []
    if suffix is None:
      suffix = suffix = ["--log-level", "debug"] if DEBUG else []

    envoy = os.environ.get("TEST_ENVOY_BIN","envoy")

    self.envoy = subprocess.Popen(prefix + [envoy, "-c", "./envoy.yaml"]+suffix, preexec_fn=envoy_preexec_fn)
    time.sleep(5)

  def test_make_requests(self):
    # Set up environment.
    self.__create_config()
    self.__start_upstream()
    self.__start_envoy()

    # Make a valid request.
    response = requests.get(
      'https://localhost:10000/get',
      verify=False,
      cert=('./e2e/redis.crt', './e2e/redis.key'))
    self.assertEqual(httplib.OK, response.status_code)

    # Make an invalid request.
    with self.assertRaises(requests.exceptions.ConnectionError):
      requests.get('https://localhost:10000/get', verify=False)

if __name__ == "__main__":
  global DEBUG
  DEBUG =  True if os.environ.get("DEBUG","") != "0" else False
  if DEBUG:
    logging.basicConfig(level=logging.DEBUG)
  unittest.main()
