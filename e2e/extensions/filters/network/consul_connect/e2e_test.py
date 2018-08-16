import contextlib
import httplib
import json
import logging
import os
import requests
import subprocess
import tempfile
import time
import unittest

from e2e.extensions.filters.common import filtertest


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
      - name: io.solo.filters.network.consul_connect
        config:
          target: db
          authorize_hostname: example.com
          authorize_cluster_name: authorize
          request_timeout: 2s
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
  - connect_timeout: 5.000s
    hosts:
    - socket_address:
        address: 127.0.0.1
        port_value: 8501
    name: authorize
    type: STRICT_DNS
"""

class ConsulConnectTestCase(filtertest.TestCase):
  def __init__(self, *args, **kwargs):
    artifact_root_path = "./e2e/extensions/filters/network/consul_connect"
    super(ConsulConnectTestCase, self).__init__(artifact_root_path, *args, **kwargs)

  def __write_temp_file(self, str, suffix=''):
    write_file = tempfile.NamedTemporaryFile('w', suffix=suffix)
    write_file.write(str)
    write_file.flush()
    return write_file

  def __create_config(self, root_crt_filename):
    raw_yaml = raw_yaml_template % root_crt_filename
    return self.__write_temp_file(raw_yaml, suffix='.yaml')

  def __start_consul(self):
    consul_path = self._join_artifact_path("consul")
    consul_d_path = self._join_artifact_path("etc/consul.d")
    args = [consul_path, "agent", "-dev", "-config-dir={}".format(consul_d_path)]
    self._processes["consul"] = subprocess.Popen(args)

  def __start_authorize_endpoint(self):
    authorize_endpoint_path = self._join_artifact_path("authorize_endpoint.py")
    self._processes["authorize_endpoint"] = subprocess.Popen([authorize_endpoint_path])

  def __start_upstream(self):
    upstream_path = self._join_artifact_path("upstream.py")
    self._processes["upstream"] = subprocess.Popen([upstream_path])

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
    parsed_json = self.__get_parsed_json('http://127.0.0.1:8500/v1/agent/connect/ca/leaf/web')
    crt_file = self.__write_temp_file(parsed_json["CertPEM"])
    key_file = self.__write_temp_file(parsed_json["PrivateKeyPEM"])
    yield crt_file, key_file

  def __is_authorized(self, payload_obj):
    payload = json.dumps(payload_obj)
    response = requests.post('http://localhost:8501/v1/agent/connect/authorize', data=payload)
    self.assertEqual(httplib.OK, response.status_code)
    parsed_response = json.loads(response.text)
    return parsed_response["Authorized"]

  def __make_get_request(self, cert):
    return requests.get('https://localhost:10000/get', verify=False, cert=cert)

  def __get_allowed_and_denied_stats(self):
    response = requests.get('http://localhost:19000/stats?format=json')
    self.assertEqual(httplib.OK, response.status_code)
    stats = response.text
    parsed_stats = json.loads(stats)['stats']
    stats_dict = {s['name']:s['value'] for s in parsed_stats if 'name' in s}
    return (stats_dict['consul_connect.allowed'], stats_dict['consul_connect.denied'])

  def test_authorize_endpoint(self):
    # Set up Authorize endpoint.
    self.__start_authorize_endpoint()
    time.sleep(1)

    # Make an authorized request.
    payload = {
 "Target": "db",
 "ClientCertURI": "spiffe://dc1-7e567ac2-551d-463f-8497-f78972856fc1.consul/ns/default/dc/dc1/svc/web",
 "ClientCertSerial": "04:00:00:00:00:01:15:4b:5a:c3:94"
}
    self.assertTrue(self.__is_authorized(payload))

    # Make an unauthorized request.
    payload = {
 "Target": "redis",
 "ClientCertURI": "spiffe://dc1-7e567ac2-551d-463f-8497-f78972856fc1.consul/ns/default/dc/dc1/svc/db",
 "ClientCertSerial": "04:00:00:00:00:01:15:4b:5a:c3:94"
}
    self.assertFalse(self.__is_authorized(payload))

  def test_make_requests(self):
    # Set up Consul.
    self.__start_consul()
    time.sleep(1)

    # Set up Envoy.
    with self.__get_root_crt_file() as root_crt_file:
      with self.__create_config(root_crt_file.name) as yaml_file:
        self._start_envoy(yaml_file.name, DEBUG)

    # Set up upstream.
    self.__start_upstream()
    time.sleep(1)

    # Fetch many service certificates.
    # The purpose of this step is to increase the serial numbers of certificates fetched later. By
    # doing so, this test allows coverage of the colon-hex encoding string manipulation performed by
    # the filter for serial numbers longer than a single byte.
    for i in xrange(260):
      self.__get_parsed_json('http://127.0.0.1:8500/v1/agent/connect/ca/leaf/service{}'.format(i))

    self.assertEqual((0, 0), self.__get_allowed_and_denied_stats())

    # Assert that requests fail if the Authorize endpoint is still down.
    for _ in xrange(100):
      with self.__get_crt_file_and_key_file() as (crt_file, key_file):
        with self.assertRaises(requests.exceptions.ConnectionError):
          self.__make_get_request(cert=(crt_file.name, key_file.name))

    self.assertEqual((0, 100), self.__get_allowed_and_denied_stats())

    # Set up the Authorize endpoint.
    self.__start_authorize_endpoint()
    time.sleep(1)

    # Make valid requests using an authorized client certificate.
    for _ in xrange(100):
      with self.__get_crt_file_and_key_file() as (crt_file, key_file):
        response = self.__make_get_request(cert=(crt_file.name, key_file.name))
        self.assertEqual(httplib.OK, response.status_code)

    self.assertEqual((100, 100), self.__get_allowed_and_denied_stats())

    # TODO(talnordan): Make invalid requests using an unauthorized client certificate.

    # Make invalid requests without a client certificate.
    for _ in xrange(100):
      with self.assertRaises(requests.exceptions.ConnectionError):
        self.__make_get_request(cert=None)

    self.assertEqual((100, 200), self.__get_allowed_and_denied_stats())

if __name__ == "__main__":
  global DEBUG
  DEBUG =  True if os.environ.get("DEBUG","") != "0" else False
  if DEBUG:
    logging.basicConfig(level=logging.DEBUG)
  unittest.main()
