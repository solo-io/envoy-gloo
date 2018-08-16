import ctypes
import ctypes.util
import os
import signal
import subprocess
import time
import unittest

def envoy_preexec_fn():
  PR_SET_PDEATHSIG = 1  # See prtcl(2).
  os.setpgrp()
  libc = ctypes.CDLL(ctypes.util.find_library('c'), use_errno=True)
  libc.prctl(PR_SET_PDEATHSIG, signal.SIGTERM)

class TestCase(unittest.TestCase):
  def __init__(self, artifact_root_path, *args, **kwargs):
    super(TestCase, self).__init__(*args, **kwargs)
    self.__artifact_root_path = artifact_root_path

  def setUp(self):
    self.cleanup()

  def tearDown(self):
    for p in self._processes.values():
      p.terminate()
    if self.__envoy is not None:
      self.__envoy.send_signal(signal.SIGINT)
      self.__envoy.wait()

    self.cleanup()

  def cleanup(self):
    self._processes = {}
    self.__envoy = None

  def _join_artifact_path(self, path):
    joined_path = os.path.join(self.__artifact_root_path, path)
    if not os.path.exists(joined_path):
      self.fail('"{}" was not found'.format(path))
    return joined_path

  def _start_envoy(self, yaml_filename, debug, prefix = None, suffix = None):
    if prefix is None:
      prefix = []
    if suffix is None:
      suffix = ["--log-level", "debug"] if debug else []

    envoy = os.environ.get("TEST_ENVOY_BIN","envoy")

    self.__envoy = subprocess.Popen(prefix + [envoy, "-c", yaml_filename]+suffix, preexec_fn=envoy_preexec_fn)
    time.sleep(5)
