import os, sys, re
from urllib.parse import urlparse
import paho.mqtt.client as mosquitto

re_io_peripheral = r"^dev/(?P<dev>[\w\d]+)/uuid/(?P<uuid>[\da-f\-]+)/(?P<dir>in|out)/(?P<peripheral>[\w]+)/(?P<index>\d+)$"

def on_message(mqttc, obj, msg):
    match = re.match(re_io_peripheral, msg.topic)
    if not match:
        print("Invalid topic format")
        return

    print(msg.topic + " " + str(msg.qos) + " " + str(msg.payload))

    if match.group("peripheral") == "sw":
        dest_topic = "dev/{dev}/uuid/{uuid}/out/led/{index}".format(**match.groupdict())
        mqttc.publish(dest_topic, msg.payload)

if __name__ == "__main__":
    mqttc = mosquitto.Client()
    mqttc.on_message = on_message

    if len(sys.argv) > 1:
        url = urlparse(sys.argv[1])
    else:
        url = urlparse('mqtt://localhost:1883')

    mqttc.connect(url.hostname, url.port)
    mqttc.subscribe('dev/pcu/uuid/+/in/sw/+')

    rc = 0
    while rc == 0:
        rc = mqttc.loop()
    print("rc: " + str(rc))