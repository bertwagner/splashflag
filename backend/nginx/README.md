This `nginx.conf` file is set to use basic authentication.

Set your username and password by generating the `.htpasswd` file with the following command:

`sudo htpasswd -c ./nginx/.htpasswd username`

That file will then get copied into the correct location in the Docker container on startup.

