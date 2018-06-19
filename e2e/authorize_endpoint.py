#!/usr/bin/env python

import BaseHTTPServer
import json

class _Handler(BaseHTTPServer.BaseHTTPRequestHandler):
  def __is_authorized(self):
    payload = self.rfile.read(int(self.headers.getheader('Content-Length')))
    parsed_payload = json.loads(payload)
    target = parsed_payload["Target"]
    client_cert_uri = parsed_payload["ClientCertURI"]
    return target == "db" and client_cert_uri.split('/')[-1] == "web"

  @staticmethod
  def __authorized_response_obj():
    return {
      "Authorized": True,
      "Reason": "Matched intention: web => db (allow)"
    }

  @staticmethod
  def __unauthorized_response_obj():
    return {
      "Authorized": False
    }

  def do_POST(self):
    assert (self.path == '/agent/connect/authorize')

    self.send_response(200)
    self.send_header('Content-type','application/json')
    self.end_headers()

    reponse_obj = \
      _Handler.__authorized_response_obj() if self.__is_authorized() \
      else _Handler.__unauthorized_response_obj()
    response = json.dumps(reponse_obj)
    self.wfile.write(response)

def _start_endpoint():
  host_name = ""
  port_number = 4223
  httpd = BaseHTTPServer.HTTPServer((host_name, port_number), _Handler)
  httpd.serve_forever()

if __name__ == "__main__":
  _start_endpoint()
