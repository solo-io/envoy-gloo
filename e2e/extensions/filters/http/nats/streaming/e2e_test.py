import grequests
import httplib
import logging
import os
import requests
import subprocess
import tempfile
import time
import unittest

from e2e.extensions.filters.common import filtertest

DEBUG=True

class NatsStreamingTestCase(filtertest.TestCase):
  def __init__(self, *args, **kwargs):
    artifact_root_path = "./e2e/extensions/filters/http/nats/streaming"
    super(NatsStreamingTestCase, self).__init__(artifact_root_path, *args, **kwargs)

  def setUp(self):
    super(NatsStreamingTestCase, self).setUp()

    # A temporary file is used to avoid pipe buffering issues.
    self.stderr = tempfile.NamedTemporaryFile("rw+", delete=True)

  def tearDown(self):
    super(NatsStreamingTestCase, self).tearDown()

    # The file is deleted as soon as it is closed.
    if self.stderr is not None:
      self.stderr.close()
    self.stderr = None

  def __create_config(self):
    create_config_path = self._join_artifact_path("create_config.sh")
    subprocess.check_call(create_config_path)

  def __start_nats_server(self):
    args = ["gnatsd", "-DV"] if DEBUG else "gnatsd"
    self._processes["nats_server"] = subprocess.Popen(args)

  def __start_nats_streaming_server(self):
    args = ["nats-streaming-server", "-ns", "nats://localhost:4222"]
    if DEBUG:
      args.append("-SDV")
    self._processes["nats_streaming_server"] = subprocess.Popen(args)

  def __sub(self):
    self._processes["sub_process"] = subprocess.Popen(
      ["stan-sub", "-id", "17", "subject1"],
      stderr=self.stderr)
    time.sleep(.1)

  def __make_request(self, payload, expected_status):
    response = requests.post('http://localhost:10000/post', payload)
    self.assertEqual(expected_status, response.status_code)

  def __make_many_requests(self, payloads, expected_status):
    requests = (grequests.post('http://localhost:10000/post', data=p) for p in payloads)
    responses = grequests.map(requests)
    if expected_status:
      for response in responses:
        self.assertEqual(expected_status, response.status_code)

  def __wait_for_response(self, data):
    time.sleep(0.1)
    self._processes["sub_process"].terminate()
    del self._processes["sub_process"]
    self.stderr.seek(0, 0)
    stderr = self.stderr.read()
    expected = 'subject:"subject1" data:"%s"' % data
    self.assertIn(expected, stderr)

  def __make_request_batches(self,
                             format_string,
                             batches,
                             requests_in_batch,
                             sleep_interval,
                             expected_status):
    for i in xrange(batches):
      payloads = [(format_string % (i, j)) for j in xrange(requests_in_batch)]
      self.__make_many_requests(payloads, expected_status)
      time.sleep(sleep_interval)

  def test_make_many_requests(self):
    # Set up environment.
    self.__create_config()
    self.__start_nats_server()
    self.__start_nats_streaming_server()
    self._start_envoy("./envoy.yaml", DEBUG)
    self.__sub()

    # Make many requests and assert that they succeed.
    self.__make_request_batches("solopayload %d %d", 3, 1024, 0.1, httplib.OK)
    self.__wait_for_response("solopayload 2 1023")

    # Terminate NATS Streaming to make future requests timeout.
    self._processes["nats_streaming_server"].terminate()
    del self._processes["nats_streaming_server"]

    # Make many requests and assert that they timeout.
    self.__make_request_batches("solopayload %d %d", 2, 1024, 0.1, httplib.REQUEST_TIMEOUT)

  def test_profile(self):
    report_loc = os.environ.get("TEST_PROF_REPORT","")
    if not report_loc:
      self.skipTest("to enable, set TEST_PROF_REPORT to where you want the report to be saved. " + \
                    "i.e. TEST_PROF_REPORT=report.data")
    print("Starting perf tests; if you have issues you might need to enable perf for normal users:")
    print("'echo -1 | sudo tee  /proc/sys/kernel/perf_event_paranoid'")
    print("'echo  0 | sudo tee  /proc/sys/kernel/kptr_restrict'")
    # Set up environment.
    # See https://github.com/envoyproxy/envoy/blob/e51c8ad0e0526f78c47a7f90807c184a039207d5/tools/envoy_collect/envoy_collect.py#L192
    self.__create_config()
    self.__start_nats_server()
    self.__start_nats_streaming_server()
    self._start_envoy(["perf", "record", "-g","--"], ["-l","error"])
    self.__sub()
    
    # Make many requests and assert that they succeed.
    self.__make_request_batches("solopayload %d %d", 20, 1024, 0.1, None)
    # The performance tests are slower so we have lower expectations of whats received
    self.__wait_for_response("solopayload 0 500")

    # tear down everything so we can copy the report
    self.tearDown()

    # print the report
    subprocess.check_call(["cp", "perf.data", report_loc])

if __name__ == "__main__":
  global DEBUG
  DEBUG =  True if os.environ.get("DEBUG","") != "0" else False
  if DEBUG:
    logging.basicConfig(level=logging.DEBUG)
  unittest.main()
  