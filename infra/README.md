In order for Splashflag to work, an MQTT broker needs to be hosted somewhere.

This Docker Compose file starts the [Eclipse Mosquitto MQTT broker service](https://hub.docker.com/_/eclipse-mosquitto) and reveals it through a [Cloudflare Tunnel](https://developers.cloudflare.com/cloudflare-one/connections/connect-networks/). 

The Splashflag hardware is then able to receive MQTT messages from this broker.

Make sure the environment variable $CLOUDFLARE_TUNNEL_TOKEN is set.