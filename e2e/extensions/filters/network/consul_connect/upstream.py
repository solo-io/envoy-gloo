#!/usr/bin/env python

import BaseHTTPServer

class _Handler(BaseHTTPServer.BaseHTTPRequestHandler):
  def do_GET(self):
    self.send_response(200)
    self.send_header('Content-type','text/html')
    self.end_headers()
    self.wfile.write("Hello, world!")

def _start_upstream():
  host_name = ""
  port_number = 4222
  httpd = BaseHTTPServer.HTTPServer((host_name, port_number), _Handler)
  httpd.serve_forever()

if __name__ == "__main__":
  _start_upstream()
