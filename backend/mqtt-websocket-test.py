# THIS FILE IS USED FOR TESTING/DEBUGGING THE CLOUDFLARE TUNNEL MQTT WEBSOCKET PROXY ONLY. 
# IT IS NOT USED IN PRODUCTION.
# Make sure your environment variable MOSQUITTO_PASSWORD is set

import time
import paho.mqtt.client as mqtt
import os
import base64

def on_publish(client, userdata, mid, reason_code, properties):
    # reason_code and properties will only be present in MQTTv5. It's always unset in MQTTv3
    try:
        userdata.remove(mid)
    except KeyError:
        print("on_publish() is called with a mid not present in unacked_publish")
        print("This is due to an unavoidable race-condition:")
        print("* publish() return the mid of the message sent.")
        print("* mid from publish() is added to unacked_publish by the main thread")
        print("* on_publish() is called by the loop_start thread")
        print("While unlikely (because on_publish() will be called after a network round-trip),")
        print(" this is a race-condition that COULD happen")
        print("")
        print("The best solution to avoid race-condition is using the msg_info from publish()")
        print("We could also try using a list of acknowledged mid rather than removing from pending list,")
        print("but remember that mid could be re-used !")

unacked_publish = set()
mqttc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, transport="websockets")
mqttc.on_publish = on_publish

#MQTT broker auth
mqttc.user_data_set(unacked_publish)
mqttc.username_pw_set(os.environ.get("MOSQUITTO_USERNAME"),os.environ.get("MOSQUITTO_PASSWORD"))


# # HTTP Basic Auth credentials for nginx
# http_username = os.environ.get("NGINX_BASIC_AUTH_USERNAME")
# http_password = os.environ.get("NGINX_BASIC_AUTH_PASSWORD")

# # Create Authorization header for HTTP basic auth
# auth_string = f"{http_username}:{http_password}"
# auth_bytes = auth_string.encode('ascii')
# auth_b64 = base64.b64encode(auth_bytes).decode('ascii')

# # Set custom headers for HTTP basic auth
# headers = {
#     "Authorization": f"Basic {auth_b64}"
# }

# # Set WebSocket options with authentication headers
# mqttc.ws_set_options(path="/", headers=headers)



mqttc.tls_set()
mqttc.connect(os.environ.get("MQTT_SERVER_URL"), 443)
mqttc.loop_start()

# Our application produce some messages
msg_info = mqttc.publish("splashflag/all", "Hello World", qos=1)
unacked_publish.add(msg_info.mid)



# Wait for all message to be published
while len(unacked_publish):
    time.sleep(0.1)

# Due to race-condition described above, the following way to wait for all publish is safer
msg_info.wait_for_publish()


mqttc.disconnect()
mqttc.loop_stop()