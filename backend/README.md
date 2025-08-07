In order for Splashflag to work, an MQTT broker needs to be hosted somewhere.

This Docker Compose file starts the [Eclipse Mosquitto MQTT broker service on Websockets](https://hub.docker.com/_/eclipse-mosquitto) and reveals it through a [Cloudflare Tunnel](https://developers.cloudflare.com/cloudflare-one/connections/connect-networks/). This allows me to host the server securely and locally.

You must [setup authentication for Mosquitto](https://mosquitto.org/documentation/authentication-methods/) by creating a password file `mosquitto_passwd -c /mosquitto/config/passwd splashflag`.


Helpful command for testing/troubleshooting Mosquitto:
```
mosquitto_sub -u splashflag -P $MOSQUITTO_PASSWORD -t splashflag/all -q 1
mosquitto_pub -u splashflag -P $MOSQUITTO_PASSWORD -t "splashflag/all"  -m '
Hello!'
```
Note: the `mosquitto_sub` and `mosquitto_pub` utilities don't support websocket connections, only mqtt. Therefore, using them to test through the cloudflare tunnel isn't possible. You can run the above command within the mosquitto container to subscribe to the mqtt broker, but then run the `mqtt-websocket-test.py` file to test that websocket requests work over Cloudflare.

The Cloudflare tunnel has to point to the websocket port of the MQTT broker (http://mosquitto:9001 in thi setup). Any websocket pub/subs must then connect to the Cloudflare Tunnel endpoint over https/port 443. See the `mqtt-websocket-test.py` file for an example of publishing over websockets. 

The Splashflag hardware is then able to receive MQTT messages from this broker.

Make sure the environment variables `$MOSQUITTO_PASSWORD`, `$CLOUDFLARE_TUNNEL_TOKEN` are set.

If using the web app to send messages, you also need to add the $MOSQUITTO_PASSWORD to /mosquitto/http/secrets.js.
