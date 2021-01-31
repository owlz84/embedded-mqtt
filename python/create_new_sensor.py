import yaml
import json
import math
from time import sleep
from random import random
import paho.mqtt.client as mqtt


def on_connect(client, userdata, flags, rc):
    print("Connected with result code " + str(rc))

    # register new devices
    for device in config["devices"]:
        root_topic = f"homeassistant/{device['platform']}/{config['node_id']}/{device['name']}"
        measurements = device["measurements"]
        for measurement in measurements:
            config_payload = {
                "name": measurement["name"],
                "device_class": measurement["device_class"],
                "state_topic": root_topic + "/state",
                "unit_of_measurement": measurement["unit_of_measurement"],
                "value_template": f"{{{{ value_json.{measurement['device_class']} }}}}"
            }
            print(f"{root_topic}_{measurement['device_class']}" + "/config")
            print(json.dumps(config_payload))
            client.publish(
                topic=f"{root_topic}_{measurement['device_class']}" + "/config",
                payload=json.dumps(config_payload)
            )

        for _ in range(5):
            state_payload = {measurement["device_class"]: math.floor(1000*random()) / 10 for measurement in measurements}
            print(root_topic + "/state", state_payload)
            info = client.publish(topic=root_topic + "/state", payload=json.dumps(state_payload))
            sleep(1)


with open("config.yaml", "r") as fh:
    config = yaml.load(fh, Loader=yaml.CLoader)
client = mqtt.Client()
client.on_connect = on_connect
client.username_pw_set(username=config["broker"]["username"], password=config["broker"]["password"])
client.connect(config["broker"]["host"], config["broker"]["port"], 60)
client.loop_forever()
