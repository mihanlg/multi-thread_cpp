#!/usr/bin/env python
# -*- coding: utf-8 -*-

import socket
from sys import stdin

sock = socket.socket()
sock.connect(('localhost', 12345))
while(True):
    print "Input command:"
    command = stdin.read()
    if command == "exit":
        print "Leaving..."
        break
    sock.send(command)
    print "Sent message: ", command
    data = sock.recv(1024)
    print data
sock.close()

