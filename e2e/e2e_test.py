import contextlib
import httplib
import json
import logging
import os
import requests
import signal
import subprocess
import tempfile
import time
import unittest


raw_yaml_template = r"""admin:
  access_log_path: /dev/stdout
  address:
    socket_address:
      address: 127.0.0.1
      port_value: 19000
static_resources:
  listeners:
  - name: listener_0
    address:
      socket_address: { address: 127.0.0.1, port_value: 10000 }
    filter_chains:
    - filters:
      - name: io.solo.filters.network.client_certificate_restriction
        config:
          target: "redis"
      - name: envoy.http_connection_manager
        config:
          stat_prefix: http
          codec_type: AUTO
          route_config:
            name: local_route
            virtual_hosts:
            - name: local_service
              domains: ["*"]
              routes:
              - match:
                  prefix: /get
                route:
                  cluster: cluster_0
          http_filters:
          - name: envoy.router
      tls_context:
        common_tls_context:
          tls_certificates:
          - certificate_chain:
              filename: /tmp/pki/root/certs/www.acme.com.crt
            private_key:
              filename: /tmp/pki/root/keys/www.acme.com.key
          tls_params: {}
          validation_context:
            trusted_ca:
              filename: %s
  clusters:
  - connect_timeout: 5.000s
    hosts:
    - socket_address:
        address: 127.0.0.1
        port_value: 4222
    name: cluster_0
    type: STRICT_DNS
"""

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
    for p in (self.consul, self.upstream):
      if p is not None:
        p.terminate()
    if self.envoy is not None:
      self.envoy.send_signal(signal.SIGINT)
      self.envoy.wait()

    self.cleanup()

  def cleanup(self):
    self.consul = None
    self.upstream = None
    self.envoy = None

  def __write_temp_file(self, str, suffix=''):
    write_file = tempfile.NamedTemporaryFile('w', suffix=suffix)
    write_file.write(str)
    write_file.flush()
    return write_file

  def __create_config(self, root_crt_filename):
    raw_yaml = raw_yaml_template % root_crt_filename
    return self.__write_temp_file(raw_yaml, suffix='.yaml')

  def __start_consul(self):
    consul_path = "./e2e/consul"
    if not os.path.isfile(consul_path):
      self.fail('"consul" was not found')
    self.consul = subprocess.Popen([consul_path, "agent", "-dev", "-config-dir=./e2e/etc/consul.d"])

  def __start_upstream(self):
    for upstream_path in ("./upstream.py", "./e2e/upstream.py"):
      if os.path.isfile(upstream_path):
        self.upstream = subprocess.Popen([upstream_path])
        break
    else:
      self.fail('"upstream.py" was not found')

  def __start_envoy(self, yaml_filename,  prefix = None, suffix = None):
    if prefix is None:
      prefix = []
    if suffix is None:
      suffix = suffix = ["--log-level", "debug"] if DEBUG else []

    envoy = os.environ.get("TEST_ENVOY_BIN","envoy")

    self.envoy = subprocess.Popen(prefix + [envoy, "-c", yaml_filename]+suffix, preexec_fn=envoy_preexec_fn)
    time.sleep(5)

  def __get_parsed_json(self, url):
    response = requests.get(url)
    self.assertEqual(httplib.OK, response.status_code)
    json_string = response.text
    parsed_json = json.loads(json_string)
    return parsed_json

  def __get_root_crt_file(self):
    parsed_json = self.__get_parsed_json('http://127.0.0.1:8500/v1/connect/ca/roots')
    root_crt_file = self.__write_temp_file(parsed_json["Roots"][0]["RootCert"])
    return root_crt_file

  @contextlib.contextmanager
  def __get_crt_file_and_key_file(self):
    parsed_json = self.__get_parsed_json('http://127.0.0.1:8500/v1/agent/connect/ca/leaf/redis')
    crt_file = self.__write_temp_file(parsed_json["CertPEM"])
    key_file = self.__write_temp_file(parsed_json["PrivateKeyPEM"])
    yield crt_file, key_file

  def test_make_requests(self):
    # Set up Consul.
    self.__start_consul()
    time.sleep(1)

    # Set up Envoy.
    with self.__get_root_crt_file() as root_crt_file:
      with self.__create_config(root_crt_file.name) as yaml_file:
        self.__start_envoy(yaml_file.name)

    # Set up upstream.
    self.__start_upstream()
    time.sleep(1)

    # Make a valid request.
    with self.__get_crt_file_and_key_file() as (crt_file, key_file):
      response = requests.get(
        'https://localhost:10000/get',
        verify=False,
        cert=(crt_file.name, key_file.name))
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
