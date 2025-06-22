In order for Splashflag to work, an MQTT broker needs to be hosted somewhere.

This Docker Compose file starts the [Eclipse Mosquitto MQTT broker service on Websockets](https://hub.docker.com/_/eclipse-mosquitto) and reveals it through a [Cloudflare Tunnel](https://developers.cloudflare.com/cloudflare-one/connections/connect-networks/). This allows me to host the server securely and locally.

You must [setup authentication for Mosquitto](https://mosquitto.org/documentation/authentication-methods/) by creating a password file `mosquitto_passwd -c /mosquitto/config/passwd splashflagclient`.

Helpful commands for testing/troubleshooting Mosquitto:
```
mosquitto_sub -h splashflag-mqtt.bertwagner.com -p 9001 -u splashflagclient -P $MOSQUITTO_PASSWORD -t splashflag/all -q 1
mosquitto_pub -h splashflag-mqtt.bertwagner.com -p 9001 -u splashflagclient -P $MOSQUITTO_PASSWORD -t splashflag/all -m 'hello' -q 1
```

The Splashflag hardware is then able to receive MQTT messages from this broker.

Make sure the environment variables `$MOSQUITTO_PASSWORD`, `$CLOUDFLARE_TUNNEL_TOKEN` are set.

