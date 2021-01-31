import yaml
import paho.mqtt.client as mqtt


# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    print("Connected with result code " + str(rc))
    for topic in config["topics"]:
        client.subscribe(topic)


def on_message(client, userdata, msg):
    print(msg.topic + " " + str(msg.payload))


with open("config.yaml", "r") as fh:
    config = yaml.load(fh, Loader=yaml.CLoader)
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.username_pw_set(username=config["broker"]["username"], password=config["broker"]["password"])

client.connect(config["broker"]["host"], config["broker"]["port"], 60)
client.loop_forever()
