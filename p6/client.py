#!/usr/bin/env python
# -*- coding: utf-8 -*-

import socket
from sys import stdin

sock = socket.socket()
sock.connect(('localhost', 12345))
while(True):
    print "Input command:"
    command = stdin.readline()
    if len(command) == 0:
        continue
    if command.startswith("exit"):
        print "Leaving..."
        break
    sock.send(command)
    data = sock.recv(1024)
    print "Answer: ", data
sock.close()

