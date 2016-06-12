#!/usr/bin/env python

# Test whether a PUBLISH to a topic with QoS 2 results in the correct packet
# flow. This test introduces delays into the flow in order to force the broker
# to send duplicate PUBREC and PUBCOMP messages.

import subprocess
import socket
import time

import inspect, os, sys
# From http://stackoverflow.com/questions/279237/python-import-a-module-from-a-folder
cmd_subfolder = os.path.realpath(os.path.abspath(os.path.join(os.path.split(inspect.getfile( inspect.currentframe() ))[0],"..")))
if cmd_subfolder not in sys.path:
    sys.path.insert(0, cmd_subfolder)

import mosq_test

rc = 1
keepalive = 600
connect_packet = mosq_test.gen_connect("pub-qos2-timeout-test", keepalive=keepalive)
connack_packet = mosq_test.gen_connack(rc=0)

mid = 1926
publish_packet = mosq_test.gen_publish("pub/qos2/test", qos=2, mid=mid, payload="timeout-message")
pubrec_packet = mosq_test.gen_pubrec(mid)
pubrel_packet = mosq_test.gen_pubrel(mid)
pubcomp_packet = mosq_test.gen_pubcomp(mid)

broker = mosq_test.start_broker(filename=os.path.basename(__file__))

try:
    sock = mosq_test.do_client_connect(connect_packet, connack_packet)
    sock.send(publish_packet)

    if mosq_test.expect_packet(sock, "pubrec", pubrec_packet):
        # Timeout is 8 seconds which means the broker should repeat the PUBREC.

        if mosq_test.expect_packet(sock, "pubrec", pubrec_packet):
            sock.send(pubrel_packet)

            if mosq_test.expect_packet(sock, "pubcomp", pubcomp_packet):
                rc = 0

    sock.close()
finally:
    broker.terminate()
    broker.wait()
    if rc:
        (stdo, stde) = broker.communicate()
        print(stde)

exit(rc)

